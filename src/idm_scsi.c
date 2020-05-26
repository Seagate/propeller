/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <scsi/sg.h>
#include <scsi/scsi.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ilm.h"

#include "idm_wrapper.h"
#include "inject_fault.h"
#include "list.h"
#include "log.h"
#include "util.h"

#define IDM_MUTEX_OP_INIT		0x1
#define IDM_MUTEX_OP_TRYLOCK		0x2
#define IDM_MUTEX_OP_LOCK		0x3
#define IDM_MUTEX_OP_UNLOCK		0x4
#define IDM_MUTEX_OP_REFRESH		0x5
#define IDM_MUTEX_OP_BREAK		0x6
#define IDM_MUTEX_OP_DESTROY		0x7

#define IDM_RES_VER_NO_UPDATE_NO_VALID	0x0
#define IDM_RES_VER_UPDATE_NO_VALID	0x1
#define IDM_RES_VER_UPDATE_VALID	0x2
#define IDM_RES_VER_INVALID		0x3

/* Now simply read out data with predefined size: 512B * 1000 = 500KB */
#define IDM_DATA_BLOCK_SIZE		512
#define IDM_DATA_BLOCK_NUM		1000
#define IDM_DATA_SIZE			(IDM_DATA_BLOCK_SIZE * IDM_DATA_BLOCK_NUM)

#define IDM_STATE_UNINIT		0
#define IDM_STATE_LOCKED		0x101
#define IDM_STATE_UNLOCKED		0x102
#define IDM_STATE_MULTIPLE_LOCKED	0x103
#define IDM_STATE_TIMEOUT		0x104

#define IDM_CLASS_EXCLUSIVE		0x0
#define IDM_CLASS_PROTECTED_WRITE	0x1
#define IDM_CLASS_SHARED_PROTECTED_READ	0x2

#define IDM_SCSI_WRITE			0x89  /* Or change to use 0x8E */
#define IDM_SCSI_READ			0x88  /* Or change to use 0x8E */

#define IDM_MUTEX_GROUP			0x1

#define MBYTE0(val)			((char)(val & 0xff))
#define MBYTE1(val)			((char)((val >> 8) & 0xff))
#define MBYTE2(val)			((char)((val >> 16) & 0xff))
#define MBYTE3(val)			((char)((val >> 24) & 0xff))

struct idm_data {
	uint64_t state;		/* ignored when write */
	uint64_t time_now;
	uint64_t countdown;
	uint64_t class;
	char resource_ver[8];
	char reserved0[24];
	char resource_id[64];
	char metadata[64];
	char host_id[32];
	char reserved1[32];
	char ignore1[256];
};

#define SCSI_CDB_LEN			16
#define SCSI_SENSE_LEN			64

struct idm_scsi_request {
	int op;
	int mode;
	uint64_t timeout;

	/* Lock ID */
	char lock_id[IDM_LOCK_ID_LEN];

	/* Host ID */
	char host_id[IDM_HOST_ID_LEN];

	int res_ver_type;
	char lvb[IDM_VALUE_LEN];

	char drive[PATH_MAX];
	int fd;
	uint8_t cdb[SCSI_CDB_LEN];
	uint8_t sense[SCSI_SENSE_LEN];
	struct idm_data *data;
	int data_len;
};

static char sense_invalid_opcode[32] = {
	0x72, 0x05, 0x20, 0x00, 0x00, 0x00, 0x00, 0x1c,
	0x02, 0x06, 0x00, 0x00, 0xcf, 0x00, 0x00, 0x00,
	0x03, 0x02, 0x00, 0x01, 0x80, 0x0e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static char sense_lba_oor[28] = {
	0x72, 0x05, 0x21, 0x00, 0x00, 0x00, 0x00, 0x14,
	0x03, 0x02, 0x00, 0x01, 0x80, 0x0e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static void _scsi_generate_write_cdb(uint8_t *cdb, int mutex_op)
{
	cdb[0] = IDM_SCSI_WRITE;
	cdb[1] = 0x0;			/* WRPROTECT=000b DPO=0b, FUA=0b */
	cdb[2] = IDM_MUTEX_GROUP;	/* MUTEX GROUP=1 as default value */
	cdb[3] = 0x0;			/* cdb[3..9] are ignored */
	cdb[4] = 0x0;
	cdb[5] = 0x0;
	cdb[6] = 0x0;
	cdb[7] = 0x0;
	cdb[8] = 0x0;
	cdb[9] = 0x0;
	cdb[10] = 0x0;			/* Reserved */
	cdb[11] = 0x0;
	cdb[12] = 0x0;
	cdb[13] = 0x1;			/* Number of logical blocks = 1 */
	cdb[14] = mutex_op;		/* Mutex OP */
	cdb[15] = 0;			/* Control = 0 */
}

static void _scsi_generate_read_cdb(uint8_t *cdb)
{
	cdb[0] = IDM_SCSI_READ;
	cdb[1] = 0x0;			/* WRPROTECT=000b DPO=0b FUA=0b */
	cdb[2] = 0x0;			/* MUTEX GROUP=0 as default value */
	cdb[3] = 0x0;			/* cdb[3..9] are ignored */
	cdb[4] = 0x0;
	cdb[5] = 0x0;
	cdb[6] = 0x0;
	cdb[7] = 0x0;
	cdb[8] = 0x0;
	cdb[9] = 0x0;
	cdb[10] = MBYTE3(IDM_DATA_BLOCK_NUM);
	cdb[11] = MBYTE2(IDM_DATA_BLOCK_NUM);
	cdb[12] = MBYTE1(IDM_DATA_BLOCK_NUM);
	cdb[13] = MBYTE0(IDM_DATA_BLOCK_NUM);
	cdb[14] = IDM_MUTEX_GROUP;	/* DLD1=0 DLD0=0 Group number */
	cdb[15] = 0;			/* Control = 0 */
}

static int _scsi_sg_io(char *drive, uint8_t *cdb, int cdb_len,
		       uint8_t *sense, int sense_len,
		       uint8_t *data, int data_len, int direction)
{
	sg_io_hdr_t io_hdr;
	int sg_fd;
	int ret, status;
	uint8_t op = cdb[14];

	if ((sg_fd = open(drive, O_RDWR | O_NONBLOCK)) < 0) {
		ilm_log_err("%s: error opening drive %s fd %d",
			    __func__, drive, sg_fd);
		return sg_fd;
	}

	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = cdb_len;
	io_hdr.cmdp = cdb;
	/* io_hdr.iovec_count = 0; */  /* memset takes care of this */
	io_hdr.mx_sb_len = sense_len;
	io_hdr.sbp = sense;
	io_hdr.dxfer_direction = direction;
	io_hdr.dxfer_len = data_len;
	io_hdr.dxferp = data;
	io_hdr.timeout = 15000;     /* 15000 millisecs == 15 seconds */
	/* io_hdr.flags = 0; */     /* take defaults: indirect IO, etc */
	/* io_hdr.pack_id = 0; */
	/* io_hdr.usr_ptr = NULL; */

	ilm_log_array_dbg("cdb", cdb, cdb_len);
	ilm_log_array_dbg("data", data, data_len);

	ret = ioctl(sg_fd, SG_IO, &io_hdr);
	if (ret) {
		ilm_log_err("%s: fail to send cdb %d", __func__, ret);
		goto out;
	}

	/* Make success */
	if ((io_hdr.info & SG_INFO_OK_MASK) == SG_INFO_OK) {
		ret = 0;
		goto out;
	}

	status = io_hdr.masked_status;

	ilm_log_dbg("%s: status %d", __func__, status);

	switch (status) {
	case CHECK_CONDITION:
		if (!memcmp(sense, sense_invalid_opcode,
			    sizeof(sense_invalid_opcode))) {
			ilm_log_err("%s: invalid opcode", __func__);
			ret = -EINVAL;
			break;
		}

		/* check if LBA is out of range */
		if (!memcmp(sense, sense_lba_oor, sizeof(sense_lba_oor))) {
			ilm_log_err("%s: LBA is out of range", __func__);
			ret = -EINVAL;
			break;
		}

		/* Otherwise, also reports error */
		ilm_log_array_err("sense:", (char *)sense, sense_len);
		ret = -EINVAL;
		break;

	case RESERVATION_CONFLICT:
		if (op == IDM_MUTEX_OP_REFRESH)
			ret = -ETIME;
		else if (op == IDM_MUTEX_OP_UNLOCK)
			ret = -ENOENT;
		else
			ret = -EAGAIN;
		break;

	case BUSY:
		ret = -EBUSY;
		break;

	/*
	 * Take this case as success since the IDM has achieved
	 * the required state.
	 */
	case COMMAND_TERMINATED:
		ret = 0;
		break;

	default:
		ilm_log_err("%s: unknown status %d", __func__, status);
		ret = -EINVAL;
		break;
	}

out:
	close(sg_fd);
	return ret;
}

static int _scsi_write(struct idm_scsi_request *request, int direction)
{
	sg_io_hdr_t io_hdr;
	int sg_fd;
	int ret;

	if ((sg_fd = open(request->drive, O_RDWR | O_NONBLOCK)) < 0) {
		ilm_log_err("%s: error opening drive %s fd %d",
			    __func__, request->drive, sg_fd);
		return sg_fd;
	}

	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = SCSI_CDB_LEN;
	io_hdr.cmdp = request->cdb;
	/* io_hdr.iovec_count = 0; */  /* memset takes care of this */
	io_hdr.mx_sb_len = SCSI_SENSE_LEN;
	io_hdr.sbp = request->sense;
	io_hdr.dxfer_direction = direction;
	io_hdr.dxfer_len = request->data_len;
	io_hdr.dxferp = request->data;
	io_hdr.timeout = 15000;     /* 15000 millisecs == 15 seconds */
	/* io_hdr.flags = 0; */     /* take defaults: indirect IO, etc */
	/* io_hdr.pack_id = 0; */
	/* io_hdr.usr_ptr = NULL; */

	ret = write(sg_fd, &io_hdr, sizeof(io_hdr));
	if (ret < 0) {
		close(sg_fd);
		ilm_log_err("%s: fail to write %d", __func__, ret);
		return ret;
	}

	request->fd = sg_fd;
	return ret;
}

static int _scsi_read(struct idm_scsi_request *request, int direction)
{
	sg_io_hdr_t io_hdr;
	int ret, status;

	memset(&io_hdr, 0, sizeof(io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.dxfer_direction = direction;

	ret = read(request->fd, &io_hdr, sizeof(io_hdr));
	if (ret < 0) {
		ilm_log_err("%s: fail to read scsi %d", __func__, ret);
		return ret;
	}

	status = io_hdr.masked_status;
	switch (status) {
	case CHECK_CONDITION:
		if (!memcmp(request->sense, sense_invalid_opcode,
			    sizeof(sense_invalid_opcode))) {
			ilm_log_err("%s: unsupported opcode=%d",
				    __func__, request->op);
			ret = -EINVAL;
			break;
		}

		/* check if LBA is out of range */
		if (!memcmp(request->sense, sense_lba_oor,
			    sizeof(sense_lba_oor))) {
			ilm_log_err("%s: LBA is out of range=%d",
				    __func__, request->op);
			ret = -EINVAL;
			break;
		}

		/* Otherwise, also reports error */
		ilm_log_array_err("sense:", (char *)request->sense,
				  SCSI_SENSE_LEN);
		ret = -EINVAL;
		break;

	case RESERVATION_CONFLICT:
		if (request->op == IDM_MUTEX_OP_REFRESH)
			ret = -ETIME;
		else if (request->op == IDM_MUTEX_OP_UNLOCK)
			ret = -ENOENT;
		else
			ret = -EAGAIN;
		break;

	case BUSY:
		ret = -EBUSY;
		break;

	/*
	 * Take this case as success since the IDM has achieved
	 * the required state.
	 */
	case COMMAND_TERMINATED:
		ret = 0;
		break;

	default:
		ilm_log_err("%s: unknown status %d", __func__, status);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int _scsi_xfer_sync(struct idm_scsi_request *request)
{
	uint8_t *cdb = request->cdb;
	uint8_t *sense = request->sense;
	struct idm_data *data = request->data;

	_scsi_generate_write_cdb(cdb, request->op);

	data->time_now = ilm_read_utc_time();
	data->countdown = request->timeout;
	data->class = request->mode;		/* TODO: Fixup mode in up layer */
	data->resource_ver[7] = request->res_ver_type;
	memcpy(data->host_id, request->host_id, IDM_HOST_ID_LEN);
	memcpy(data->resource_id, request->lock_id, IDM_LOCK_ID_LEN);
	memcpy(data->resource_ver, request->lvb, IDM_VALUE_LEN);

	return _scsi_sg_io(request->drive, cdb, SCSI_CDB_LEN,
			   sense, SCSI_SENSE_LEN,
			   (uint8_t *)data, sizeof(struct idm_data),
			   SG_DXFER_TO_DEV);
}

static int _scsi_xfer_async(struct idm_scsi_request *request)
{
	struct idm_data *data;
	int ret;

	_scsi_generate_write_cdb(request->cdb, request->op);

	data = request->data;
	data->time_now = ilm_read_utc_time();
	data->countdown = request->timeout;
	data->class = request->mode;		/* TODO: Fixup mode in up layer */
	data->resource_ver[7] = request->res_ver_type;
	memcpy(data->host_id, request->host_id, IDM_HOST_ID_LEN);
	memcpy(data->resource_id, request->lock_id, IDM_LOCK_ID_LEN);
	memcpy(data->resource_ver, request->lvb, IDM_VALUE_LEN);

	ret = _scsi_write(request, SG_DXFER_TO_DEV);
	if (ret < 0) {
		ilm_log_err("%s: fail to write scsi %d", __func__, ret);
		return ret;
	}

	return 0;
}

static int _scsi_recv_sync(char *drive, struct idm_data **data,
			   int *data_len)
{
	uint8_t cdb[SCSI_CDB_LEN] = { 0 };
	uint8_t sense[256] = { 0 };
	int ret;

	*data = malloc(IDM_DATA_SIZE);
	if (!*data)
		return -ENOMEM;

	*data_len = IDM_DATA_SIZE;
	_scsi_generate_read_cdb(cdb);

	ret = _scsi_sg_io(drive, cdb, sizeof(cdb), sense, sizeof(sense),
			  (uint8_t *)*data, *data_len, SG_DXFER_FROM_DEV);
	if (ret) {
		free(*data);
		*data_len = 0;
	}

	return ret;
}

static int _scsi_recv_async(struct idm_scsi_request *request)
{
	int ret;

	_scsi_generate_read_cdb(request->cdb);

	ret = _scsi_write(request, SG_DXFER_FROM_DEV);
	if (ret < 0) {
		ilm_log_err("%s: fail to write scsi %d", __func__, ret);
		free(request);
		return ret;
	}

	return 0;
}

static int _scsi_get_async_result(struct idm_scsi_request *request,
				  int direction)
{
	int ret;

	ret = _scsi_read(request, direction);
	free(request->data);
	free(request);
	return ret;
}

/**
 * idm_drive_version - Read out IDM version
 * @version:		Lock mode (unlock, shareable, exclusive).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_version(int *version, char *drive)
{
	/* Current version 0.1.0 */
	*version = ((0 << 16) | (1 << 8) | (0));
	return 0;
}

/**
 * idm_drive_lock - acquire an IDM on a specified drive
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock(char *lock_id, int mode, char *host_id,
                   char *drive, uint64_t timeout)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(sizeof(struct idm_data));
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	memset(request->data, 0x0, sizeof(struct idm_data));

	if (mode == IDM_MODE_EXCLUSIVE)
		mode = IDM_CLASS_EXCLUSIVE;
	else if (mode == IDM_MODE_SHAREABLE)
		mode = IDM_CLASS_SHARED_PROTECTED_READ;

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_TRYLOCK;
	request->mode = mode;
	request->timeout = timeout;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	request->data_len = sizeof(struct idm_data);
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_sync(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	free(request->data);
	free(request);
	return ret;
}

/**
 * idm_drive_lock_async - acquire an IDM on a specified drive with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @fd:			File descriptor (emulated with an index).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_async(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(sizeof(struct idm_data));
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	memset(request->data, 0x0, sizeof(struct idm_data));

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_TRYLOCK;
	request->mode = mode;
	request->timeout = timeout;
	request->data_len = sizeof(struct idm_data);
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_async(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	*handle = (uint64_t)request;
	return ret;
}

/**
 * idm_drive_unlock - release an IDM on a specified drive
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_unlock(char *lock_id, char *host_id,
		     void *lvb, int lvb_size, char *drive)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	if (lvb_size > IDM_VALUE_LEN)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(sizeof(struct idm_data));
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	memset(request->data, 0x0, sizeof(struct idm_data));

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_UNLOCK;
	request->mode = 0;
	request->timeout = 0;
	request->data_len = sizeof(struct idm_data);
	request->res_ver_type = IDM_RES_VER_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);
	memcpy(request->lvb, lvb, lvb_size);

	ret = _scsi_xfer_sync(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	free(request->data);
	free(request);
	return ret;
}

/**
 * idm_drive_unlock_async - release an IDM on a specified drive with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_unlock_async(char *lock_id, char *host_id,
			   void *lvb, int lvb_size,
			   char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	if (lvb_size > IDM_VALUE_LEN)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(sizeof(struct idm_data));
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = sizeof(struct idm_data);

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_UNLOCK;
	request->mode = 0;
	request->timeout = 0;
	request->res_ver_type = IDM_RES_VER_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);
	memcpy(request->lvb, lvb, lvb_size);

	ret = _scsi_xfer_async(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	*handle = (uint64_t)request;
	return ret;
}

static int idm_drive_refresh_lock(char *lock_id, int mode, char *host_id,
				  char *drive)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(sizeof(struct idm_data));
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_REFRESH;
	request->mode = mode;
	request->timeout = 0;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_sync(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	free(request->data);
	free(request);
	return ret;
}

static int idm_drive_refresh_lock_async(char *lock_id, int mode,
					char *host_id, char *drive,
					uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (!lock_id || !host_id || !drive || !handle)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(sizeof(struct idm_data));
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = sizeof(struct idm_data);

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_REFRESH;
	request->mode = mode;
	request->timeout = 0;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_async(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	*handle = (uint64_t)request;
	return ret;
}

/**
 * idm_drive_convert_lock - Convert the lock mode for an IDM
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_convert_lock(char *lock_id, int mode, char *host_id, char *drive)
{
	return idm_drive_refresh_lock(lock_id, mode, host_id, drive);
}

/**
 * idm_drive_convert_lock_async - Convert the lock mode with async
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_convert_lock_async(char *lock_id, int mode, char *host_id,
				 char *drive, uint64_t *handle)
{
	return idm_drive_refresh_lock_async(lock_id, mode, host_id,
					    drive, handle);
}

/**
 * idm_drive_renew_lock - Renew host's membership for an IDM
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_renew_lock(char *lock_id, int mode, char *host_id, char *drive)
{
	return idm_drive_refresh_lock(lock_id, mode, host_id, drive);
}

/**
 * idm_drive_renew_lock_async - Renew host's membership for an IDM
 * 				with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_renew_lock_async(char *lock_id, int mode,
			       char *host_id, char *drive,
			       uint64_t *handle)
{
	return idm_drive_refresh_lock_async(lock_id, mode, host_id,
					    drive, handle);
}

/**
 * idm_drive_break_lock - Break an IDM if before other hosts have
 * acquired this IDM.  This function is to allow a host_id to take
 * over the ownership if other hosts of the IDM is timeout, or the
 * countdown value is -1UL.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_break_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(sizeof(struct idm_data));
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_TRYLOCK;
	request->mode = mode;
	request->timeout = timeout;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_sync(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	free(request->data);
	free(request);
	return ret;
}

/**
 * idm_drive_break_lock_async - Break an IDM with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @fd:			File descriptor (emulated in index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_break_lock_async(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (!lock_id || !host_id || !drive || !handle)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(sizeof(struct idm_data));
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = sizeof(struct idm_data);

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_BREAK;
	request->mode = mode;
	request->timeout = timeout;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_async(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	*handle = (uint64_t)request;
	return ret;
}

/**
 * idm_drive_read_lvb_async - Read value block with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_read_lvb_async(char *lock_id, char *host_id, char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(IDM_DATA_SIZE);
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = IDM_DATA_SIZE;

	strncpy(request->drive, drive, PATH_MAX);
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_recv_async(request);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		return ret;
	}

	*handle = (uint64_t)request;
	return 0;
}

/**
 * idm_drive_read_lvb_async_result - Read the result for read_lvb operation with
 * 				     async mode.
 * @fd:			File descriptor (emulated with index).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_read_lvb_async_result(uint64_t handle, void *lvb, int lvb_size,
				    int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;
	struct idm_data *data = request->data;
	int ret, i;

	ret = _scsi_get_async_result(request, SG_DXFER_FROM_DEV);

	for (i = 0; i < IDM_DATA_BLOCK_NUM; i++) {
		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (memcmp(data[i].host_id, request->host_id,
			   IDM_HOST_ID_LEN))
			continue;

		memcpy(lvb, data[i].resource_ver, lvb_size);
		break;
	}

	*result = ret;

	if (data)
		free(data);
	free(request);

	return ret;
}

/**
 * idm_drive_lock_count - Read the host count for an IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @count:		Returned count value's pointer.
 * @self:		Returned self count value's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_count(char *lock_id, char *host_id,
			 int *count, int *self, char *drive)
{
	struct idm_data *data = NULL;
	int data_len;
	int ret, i;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!count || !self)
		return -EINVAL;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	ret = _scsi_recv_sync(drive, &data, &data_len);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	*count = 0;
	*self = 0;
	for (i = 0; i < data_len / IDM_DATA_BLOCK_SIZE; i++) {
		/* Skip for other locks */
		if (memcmp(data[i].resource_id, lock_id, IDM_LOCK_ID_LEN))
			continue;

		if (!memcmp(data[i].host_id, host_id, IDM_HOST_ID_LEN)) {
			/* Must be wrong if self has been accounted */
			if (*self) {
				ilm_log_err("%s: account self %d > 1",
					    __func__, *self);
				goto out;
			}

			*self = 1;
		} else {
			*count++;
		}
	}

out:
	if (data)
		free(data);

	return ret;
}

/**
 * idm_drive_lock_count_async - Read the host count for an IDM with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_count_async(char *lock_id, char *host_id,
			       char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(IDM_DATA_SIZE);
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = IDM_DATA_SIZE;

	strncpy(request->drive, drive, PATH_MAX);
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_recv_async(request);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		return ret;
	}

	*handle = (uint64_t)request;
	return 0;
}

/**
 * idm_drive_lock_count_async_result - Read the result for host count.
 * @fd:			File descriptor (emulated with index).
 * @count:		Returned count value's pointer.
 * @self:		Returned self count value's pointer.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_count_async_result(uint64_t handle, int *count, int *self,
				      int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;
	struct idm_data *data = request->data;
	int ret, i;

	ret = _scsi_get_async_result(request, SG_DXFER_FROM_DEV);

	*count = 0;
	*self = 0;
	for (i = 0; i < IDM_DATA_BLOCK_NUM; i++) {
		/* Skip for other locks */
		if (memcpy(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (!memcmp(data[i].host_id, request->host_id,
			    IDM_HOST_ID_LEN)) {
			/* Must be wrong if self has been accounted */
			if (*self) {
				ilm_log_err("%s: account self %d > 1",
					    __func__, *self);
				goto out;
			}

			*self = 1;
		} else {
			*count++;
		}
	}

	*result = ret;

out:
	if (data)
		free(data);

	return ret;
}

/**
 * idm_drive_lock_mode - Read back an IDM's current mode.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Returned mode's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_mode(char *lock_id, int *mode, char *drive)
{
	struct idm_data *data;
	int data_len;
	int ret, i;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!mode)
		return -EINVAL;

	if (!lock_id || !drive)
		return -EINVAL;

	ret = _scsi_recv_sync(drive, &data, &data_len);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	*mode = -1;
	for (i = 0; i < data_len / IDM_DATA_BLOCK_SIZE; i++) {
		/* Skip for other locks */
		if (memcpy(data[i].resource_id, lock_id, IDM_LOCK_ID_LEN))
			continue;

		if (data[i].state == IDM_STATE_UNINIT ||
		    data[i].state == IDM_STATE_UNLOCKED ||
		    data[i].state == IDM_STATE_TIMEOUT) {
			*mode = IDM_MODE_UNLOCK;
		} else if (data[i].class == IDM_CLASS_EXCLUSIVE) {
			*mode = IDM_MODE_EXCLUSIVE;
		} else if (data[i].class == IDM_CLASS_SHARED_PROTECTED_READ) {
			*mode = IDM_MODE_SHAREABLE;
		} else if (data[i].class == IDM_CLASS_PROTECTED_WRITE) {
			ilm_log_err("%s: PROTECTED_WRITE is not unsupported",
				    __func__);
			ret = -EFAULT;
			goto out;
		}

		break;
	}

	if (*mode == -1)
		*mode = IDM_MODE_UNLOCK;

out:
	if (data)
		free(data);

	return ret;
}

/**
 * idm_drive_lock_mode_async - Read an IDM's mode with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_mode_async(char *lock_id, char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(IDM_DATA_SIZE);
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = IDM_DATA_SIZE;

	strncpy(request->drive, drive, PATH_MAX);
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);

	ret = _scsi_recv_async(request);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		free(request->data);
		free(request);
		return ret;
	}

	*handle = (uint64_t)request;
	return 0;
}

/**
 * idm_drive_lock_mode_async_result - Read the result for lock mode.
 * @fd:			File descriptor (emulated with index).
 * @mode:		Returned mode's pointer.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_mode_async_result(uint64_t handle, int *mode, int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;
	struct idm_data *data = request->data;
	int ret, i;

	ret = _scsi_get_async_result(request, SG_DXFER_FROM_DEV);

	*mode = -1;
	for (i = 0; i < IDM_DATA_BLOCK_NUM; i++) {
		/* Skip for other locks */
		if (memcpy(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (data[i].state == IDM_STATE_UNINIT ||
		    data[i].state == IDM_STATE_UNLOCKED ||
		    data[i].state == IDM_STATE_TIMEOUT) {
			*mode = IDM_MODE_UNLOCK;
		} else if (data[i].class == IDM_CLASS_EXCLUSIVE) {
			*mode = IDM_MODE_EXCLUSIVE;
		} else if (data[i].class == IDM_CLASS_SHARED_PROTECTED_READ) {
			*mode = IDM_MODE_SHAREABLE;
		} else if (data[i].class == IDM_CLASS_PROTECTED_WRITE) {
			ilm_log_err("%s: PROTECTED_WRITE is not unsupported",
				    __func__);
			ret = -EFAULT;
			goto out;
		}

		break;
	}

	if (*mode == -1)
		*mode = IDM_MODE_UNLOCK;

	*result = ret;
out:
	if (data)
		free(data);

	return ret;
}

/**
 * idm_drive_async_result - Read the result for normal operations.
 * @fd:			File descriptor (emulated with index).
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_async_result(uint64_t handle, int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;

	*result = _scsi_get_async_result(request, SG_DXFER_TO_DEV);
	return 0;
}

/**
 * idm_drive_host_state - Read back the host's state for an specific IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (64 bytes).
 * @host_state:		Returned host state's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_host_state(char *lock_id, char *host_id,
			 int *host_state, char *drive)
{
	struct idm_data *data = NULL;
	int data_len;
	int ret, i;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	ret = _scsi_recv_sync(drive, &data, &data_len);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	*host_state = -1;
	for (i = 0; i < data_len; i++) {
		/* Skip for other locks */
		if (memcmp(data[i].resource_id, lock_id, IDM_LOCK_ID_LEN) ||
		    memcmp(data[i].host_id, host_id, IDM_HOST_ID_LEN))
			continue;

		*host_state = data[i].state;
		break;
	}

out:
	if (data)
		free(data);

	return ret;
}

/**
 * idm_drive_whitelist - Read back hosts list for an specific drive.
 * @drive:		Drive path name.
 * @whitelist:		Returned pointer for host's white list.
 * @whitelist:		Returned pointer for host num.
 *
 * Returns zero or a negative error (ie. ENOMEM).
 */
int idm_drive_whitelist(char *drive, char **whitelist, int *whitelist_num)
{
	/* TODO */
	return 0;
}

/**
 * idm_drive_read_group - Read back mutex group for all IDM in the drives
 * @drive:		Drive path name.
 * @info_ptr:		Returned pointer for info list.
 * @info_num:		Returned pointer for info num.
 *
 * Returns zero or a negative error (ie. ENOMEM).
 */
int idm_drive_read_group(char *drive, struct idm_info **info_ptr, int *info_num)
{
	struct idm_data *data = NULL;
	int data_len;
	int ret, i;
	struct idm_info *info_list, *info;
	int max_alloc = 8;

	/* Let's firstly assume to allocet for 8 items */
	info_list = malloc(sizeof(struct idm_info) * max_alloc);
	if (!info_list)
		return -ENOMEM;

	ret = _scsi_recv_sync(drive, &data, &data_len);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	for (i = 0; i < data_len; i++) {

		if (i >= max_alloc) {
			max_alloc += 8;

			info_list = realloc(info_list,
					sizeof(struct idm_info) * max_alloc);
			if (!info_list) {
				ret = -ENOMEM;
				goto out;
			}
		}

		info = info_list + i;

		/* Copy host ID */
		memcpy(info->id, data[i].resource_id, IDM_LOCK_ID_LEN);
		memcpy(info->host_id, data[i].host_id, IDM_HOST_ID_LEN);

		if (data[i].state == IDM_STATE_UNINIT ||
		    data[i].state == IDM_STATE_UNLOCKED ||
		    data[i].state == IDM_STATE_TIMEOUT) {
			info->mode = IDM_MODE_UNLOCK;
		} else if (data[i].class == IDM_CLASS_EXCLUSIVE) {
			info->mode = IDM_MODE_EXCLUSIVE;
		} else if (data[i].class == IDM_CLASS_SHARED_PROTECTED_READ) {
			info->mode = IDM_MODE_SHAREABLE;
		} else if (data[i].class == IDM_CLASS_PROTECTED_WRITE) {
			ilm_log_err("%s: PROTECTED_WRITE is not unsupported",
				    __func__);
			ret = -EFAULT;
			goto out;
		}

		info->last_renew_time = data[i].time_now;
		info->timeout = data[i].countdown;
		i++;
	}

	*info_ptr = info_list;
	*info_num = i;

out:
	if (ret != 0 && info_list)
		free(info_list);

	if (data)
		free(data);

	return ret;
}

/**
 * idm_drive_destroy - Destroy an IDM and release all associated resource.
 * @lock_id:		Lock ID (64 bytes).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_destroy(char *lock_id, char *drive)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !drive)
		return -EINVAL;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(sizeof(struct idm_data));
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = sizeof(struct idm_data);

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_DESTROY;
	request->mode = 0;
	request->timeout = 0;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);

	ret = _scsi_xfer_sync(request);
	if (ret < 0)
		ilm_log_err("%s: command fail %d", __func__, ret);

	free(request->data);
	free(request);
	return ret;
}

int idm_drive_get_fd(uint64_t handle)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;

	return request->fd;
}
