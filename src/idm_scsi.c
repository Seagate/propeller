/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
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
#include <byteswap.h>

#include "ilm.h"

#include "idm_scsi.h"
#include "inject_fault.h"
#include "list.h"
#include "log.h"
#include "util.h"

#define IDM_MUTEX_OP_NORMAL		0x0
#define IDM_MUTEX_OP_INIT		0x1
#define IDM_MUTEX_OP_TRYLOCK	0x2
#define IDM_MUTEX_OP_LOCK		0x3
#define IDM_MUTEX_OP_UNLOCK		0x4
#define IDM_MUTEX_OP_REFRESH	0x5
#define IDM_MUTEX_OP_BREAK		0x6
#define IDM_MUTEX_OP_DESTROY	0x7

#define IDM_RES_VER_NO_UPDATE_NO_VALID	0x0
#define IDM_RES_VER_UPDATE_NO_VALID		0x1
#define IDM_RES_VER_UPDATE_VALID		0x2
#define IDM_RES_VER_INVALID				0x3

/* Now simply read out data with predefined size: 512B * 512 = 64KB */
#define IDM_DATA_BLOCK_SIZE		512

#define IDM_STATE_UNINIT			0
#define IDM_STATE_LOCKED			0x101
#define IDM_STATE_UNLOCKED			0x102
#define IDM_STATE_MULTIPLE_LOCKED	0x103
#define IDM_STATE_TIMEOUT			0x104
#define IDM_STATE_DEAD				0xdead

#define IDM_CLASS_EXCLUSIVE				0x0
#define IDM_CLASS_PROTECTED_WRITE		0x1
#define IDM_CLASS_SHARED_PROTECTED_READ	0x2

#define IDM_SCSI_WRITE			0xF0  /* Updated  to latest spec */
#define IDM_SCSI_READ			0xF0  /* Updated  to latest spec */

#define IDM_MUTEX_GROUP				0x1
#define IDM_MUTEX_GROUP_INQUIRY		0xFF

#define MBYTE0(val)			((char)(val & 0xff))
#define MBYTE1(val)			((char)((val >> 8) & 0xff))
#define MBYTE2(val)			((char)((val >> 16) & 0xff))
#define MBYTE3(val)			((char)((val >> 24) & 0xff))

// struct idm_data {
// 	uint64_t state;		/* ignored when write */
// 	uint64_t time_now;
// 	uint64_t countdown;
// 	uint64_t class;
// 	char resource_ver[8];
// 	char reserved0[24];
// 	char resource_id[64];
// 	char metadata[64];
// 	char host_id[32];
// 	char reserved1[32];
// 	char ignore1[256];
// };

#define SCSI_VER_INQ_LEN		6
#define SCSI_VER_DATA_LEN		150
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

	char res_ver_type;
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

static char sense_mutex_list_is_full[16] = {
	0x72, 0x07, 0x40, 0x00, 0x00, 0x00, 0x00, 0x14,
	0x03, 0x02, 0x00, 0x01, 0x80, 0x0e, 0x00, 0x00,
};

static char sense_host_list_is_full[16] = {
	0x72, 0x07, 0x41, 0x00, 0x00, 0x00, 0x00, 0x14,
	0x03, 0x02, 0x00, 0x01, 0x80, 0x0e, 0x00, 0x00,
};

static char sense_1_mutex_host_list_is_full[16] = {
	0x72, 0x07, 0x42, 0x00, 0x00, 0x00, 0x00, 0x14,
	0x03, 0x02, 0x00, 0x01, 0x80, 0x0e, 0x00, 0x00,
};

static char sense_fixed_invalid_opcode[18] = {
	0x70, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0x01, 0x00,
	0x00, 0x00,
};

static char sense_fixed_lba_oor[18] = {
	0x70, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x01, 0x00,
	0x00, 0x00,
};

static char sense_fixed_mutex_list_is_full[18] = {
	0x70, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x01, 0x00,
	0x00, 0x00,
};

static char sense_fixed_host_list_is_full[18] = {
	0x70, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x01, 0x00,
	0x00, 0x00,
};

static char sense_fixed_1_mutex_host_list_is_full[18] = {
	0x70, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x01, 0x00,
	0x00, 0x00,
};

static void _scsi_generate_version_inquiry_cdb(uint8_t *cdb){
	cdb[0] = 0x12;		/* Inquiry */
	cdb[1] = 0x01;		/* EVPD bit needs to be set */
	cdb[2] = 0xc3;		/* Located in log page C3 */
	cdb[3] = 0x00;		/* Reserved */
	cdb[4] = 0x70;		/* Mutex version located at 0x60 */
	cdb[5] = 0x00;		/* Reserved */
}

static void _scsi_generate_write_cdb(uint8_t *cdb, int mutex_op)
{
	cdb[0] = IDM_SCSI_WRITE;
	cdb[1] = mutex_op<<4;		/* The MUTEX_OP is in the upper nibble of Byte 1 */
	cdb[2] = 0x00;				/* Reserved */
	cdb[3] = 0x00;				/* Reserved */
	cdb[4] = 0x00;				/* Reserved */
	cdb[5] = 0x00;				/* Reserved */
	cdb[6] = 0x00;				/* Reserved */
	cdb[7] = 0x00;				/* Reserved */
	cdb[8] = 0x00;				/* Reserved */
	cdb[9] = 0x00;				/* Reserved */
	cdb[10] = 0x00;				/* Transfer Length */
	cdb[11] = 0x00;				/* Transfer Length */
	cdb[12] = 0x02;				/* Transfer Length  = 512 */
	cdb[13] = 0x00;				/* Transfer Length */
	cdb[14] = IDM_MUTEX_GROUP;	/* Mutex Group */
	cdb[15] = 0;				/* Reserved */
}

static void _scsi_generate_read_cdb(uint8_t *cdb, uint8_t group, int num)
{
	cdb[0] = IDM_SCSI_READ;
	cdb[1] = 0x01;			/* Ignore the Mutex Opcode for now */
	cdb[2] = 0x00;			/* Reserved */
	cdb[3] = 0x00;			/* Reserved */
	cdb[4] = 0x00;			/* Reserved */
	cdb[5] = 0x00;			/* Reserved */
	cdb[6] = 0x00;			/* Reserved */
	cdb[7] = 0x00;			/* Reserved */
	cdb[8] = 0x00;			/* Reserved */
	cdb[9] = 0x00;			/* Reserved */
	cdb[10] = MBYTE3(num);
	cdb[11] = MBYTE2(num);
	cdb[12] = MBYTE1(num);
	cdb[13] = MBYTE0(num);
	cdb[14] = group;		/* MUTEX GROUP=1 as default value */
	cdb[15] = 0;			/* Reserved */
}

static int _scsi_sg_io(char *drive, uint8_t *cdb, int cdb_len,
		       uint8_t *sense, int sense_len,
		       uint8_t *data, int data_len, int direction)
{
	sg_io_hdr_t io_hdr;
	int sg_fd;
	int ret, status;
	uint8_t op = cdb[1];

	if (direction == SG_DXFER_TO_DEV)
		op = op>>4;

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

	if (status != GOOD) {
		ilm_log_array_warn("SCSI CDB:", (char *)cdb, cdb_len);
		ilm_log_array_warn("SCSI DATA:", (char *)data, data_len);
		ilm_log_array_warn("SCSI SENSE:", (char *)sense, sense_len);
		ilm_log_warn("SCSI status: 0x%x", io_hdr.status);
		ilm_log_warn("SCSI masked status: 0x%x", io_hdr.masked_status);
		ilm_log_warn("SCSI host status: 0x%x", io_hdr.host_status);
		ilm_log_warn("SCSI driver status: 0x%x", io_hdr.driver_status);
	}

	switch (status) {
	case CHECK_CONDITION:
		if (!memcmp(sense, sense_invalid_opcode,
			    sizeof(sense_invalid_opcode))) {
			ilm_log_err("%s: invalid opcode", __func__);
			ret = -EINVAL;
			break;
		}

		if (!memcmp(sense, sense_fixed_invalid_opcode,
			    sizeof(sense_fixed_invalid_opcode))) {
			ilm_log_err("%s: invalid opcode", __func__);
			ret = -EINVAL;
			break;
		}

		/* check if LBA is out of range */
		if (!memcmp(sense, sense_lba_oor, sizeof(sense_lba_oor))) {
			ilm_log_err("%s: LBA is out of range", __func__);
			ret = -ENOENT;
			break;
		}

		if (!memcmp(sense, sense_fixed_lba_oor,
			    sizeof(sense_fixed_lba_oor))) {
			ilm_log_err("%s: LBA is out of range", __func__);
			ret = -ENOENT;
			break;
		}

		/* check if mutex list is full */
		if (!memcmp(sense, sense_mutex_list_is_full,
			    sizeof(sense_mutex_list_is_full))) {
			ilm_log_err("%s: Mutex list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		if (!memcmp(sense, sense_fixed_mutex_list_is_full,
			    sizeof(sense_fixed_mutex_list_is_full))) {
			ilm_log_err("%s: Mutex list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		/* check if host list is full */
		if (!memcmp(sense, sense_host_list_is_full,
			    sizeof(sense_host_list_is_full))) {
			ilm_log_err("%s: Host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		if (!memcmp(sense, sense_fixed_host_list_is_full,
			    sizeof(sense_fixed_host_list_is_full))) {
			ilm_log_err("%s: Host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		/* check if a mutex's host list is full */
		if (!memcmp(sense, sense_1_mutex_host_list_is_full,
			    sizeof(sense_1_mutex_host_list_is_full))) {
			ilm_log_err("%s: A mutex's host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		if (!memcmp(sense, sense_fixed_1_mutex_host_list_is_full,
			    sizeof(sense_fixed_1_mutex_host_list_is_full))) {
			ilm_log_err("%s: A mutex's host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		/* Otherwise, also reports error */
		ret = -EINVAL;
		break;

	case RESERVATION_CONFLICT:
		if (op == IDM_MUTEX_OP_REFRESH)
			ret = -ETIME;
		else if (op == IDM_MUTEX_OP_UNLOCK)
			ret = -ENOENT;
		else
			ret = -EBUSY;
		break;

	case BUSY:
		ret = -EBUSY;
		break;

	case COMMAND_TERMINATED:
		if (op == IDM_MUTEX_OP_REFRESH)
			ret = -EPERM;
		else if (op == IDM_MUTEX_OP_UNLOCK)
			ret = -EINVAL;
		else
			ret = -EAGAIN;
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

	/* Make success */
	if ((io_hdr.info & SG_INFO_OK_MASK) == SG_INFO_OK)
		return 0;

	status = io_hdr.masked_status;

	if (status != GOOD) {
		ilm_log_array_warn("SCSI CDB:", (char *)request->cdb, SCSI_CDB_LEN);
		ilm_log_array_warn("SCSI DATA:", (char *)request->data, request->data_len);
		ilm_log_array_warn("SCSI SENSE:", (char *)request->sense, SCSI_SENSE_LEN);
		ilm_log_warn("SCSI status: 0x%x", io_hdr.status);
		ilm_log_warn("SCSI masked status: 0x%x", io_hdr.masked_status);
		ilm_log_warn("SCSI host status: 0x%x", io_hdr.host_status);
		ilm_log_warn("SCSI driver status: 0x%x", io_hdr.driver_status);
	}

	switch (status) {
	case CHECK_CONDITION:
		if (!memcmp(request->sense, sense_invalid_opcode,
			    sizeof(sense_invalid_opcode))) {
			ilm_log_err("%s: unsupported opcode=%d",
				    __func__, request->op);
			ret = -EINVAL;
			break;
		}

		if (!memcmp(request->sense, sense_fixed_invalid_opcode,
			    sizeof(sense_fixed_invalid_opcode))) {
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
			ret = -ENOENT;
			break;
		}

		if (!memcmp(request->sense, sense_fixed_lba_oor,
			    sizeof(sense_fixed_lba_oor))) {
			ilm_log_err("%s: LBA is out of range=%d",
				    __func__, request->op);
			ret = -ENOENT;
			break;
		}

		/* check if mutex list is full */
		if (!memcmp(request->sense, sense_mutex_list_is_full,
			    sizeof(sense_mutex_list_is_full))) {
			ilm_log_err("%s: Mutex list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		if (!memcmp(request->sense, sense_fixed_mutex_list_is_full,
			    sizeof(sense_fixed_mutex_list_is_full))) {
			ilm_log_err("%s: Mutex list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		/* check if host list is full */
		if (!memcmp(request->sense, sense_host_list_is_full,
			    sizeof(sense_host_list_is_full))) {
			ilm_log_err("%s: Host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		if (!memcmp(request->sense, sense_fixed_host_list_is_full,
			    sizeof(sense_fixed_host_list_is_full))) {
			ilm_log_err("%s: Host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		/* check if a mutex's host list is full */
		if (!memcmp(request->sense, sense_1_mutex_host_list_is_full,
			    sizeof(sense_1_mutex_host_list_is_full))) {
			ilm_log_err("%s: A mutex's host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		if (!memcmp(request->sense, sense_fixed_1_mutex_host_list_is_full,
			    sizeof(sense_fixed_1_mutex_host_list_is_full))) {
			ilm_log_err("%s: A mutex's host list is full", __func__);
			ret = -ENOMEM;
			break;
		}

		ret = -EINVAL;
		break;

	case RESERVATION_CONFLICT:
		if (request->op == IDM_MUTEX_OP_REFRESH)
			ret = -ETIME;
		else if (request->op == IDM_MUTEX_OP_UNLOCK)
			ret = -ENOENT;
		else
			ret = -EBUSY;
		break;

	case BUSY:
		ret = -EBUSY;
		break;

	case COMMAND_TERMINATED:
		if (request->op == IDM_MUTEX_OP_REFRESH)
			ret = -EPERM;
		else if (request->op == IDM_MUTEX_OP_UNLOCK)
			ret = -EINVAL;
		else
			ret = -EAGAIN;
		break;

	default:
		ilm_log_err("%s: unknown status %d", __func__, status);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void _scsi_data_swap(char *dst, char *src, int len)
{
	int i;

	for (i = 0; i < len; i++)
		dst[i] = src[len - i - 1];
}

static int _scsi_xfer_sync(struct idm_scsi_request *request)
{
	uint8_t *cdb = request->cdb;
	uint8_t *sense = request->sense;
	struct idm_data *data = request->data;

	_scsi_generate_write_cdb(cdb, request->op);

	data->time_now = __bswap_64(ilm_read_utc_time());
	data->countdown = __bswap_64(request->timeout);
	data->class = __bswap_64(request->mode);	/* TODO: Fixup mode in up layer */
	_scsi_data_swap(data->host_id, request->host_id, IDM_HOST_ID_LEN);
	_scsi_data_swap(data->resource_id, request->lock_id, IDM_LOCK_ID_LEN);
	_scsi_data_swap(data->resource_ver, request->lvb, IDM_VALUE_LEN);
	data->resource_ver[0] = request->res_ver_type;

	ilm_log_array_dbg("resrouce_ver", data->resource_ver, IDM_VALUE_LEN);

	return _scsi_sg_io(request->drive, cdb, SCSI_CDB_LEN,
			   sense, SCSI_SENSE_LEN,
			   (uint8_t *)data, request->data_len,
			   SG_DXFER_TO_DEV);
}

static int _scsi_xfer_async(struct idm_scsi_request *request)
{
	struct idm_data *data = request->data;
	int ret;

	_scsi_generate_write_cdb(request->cdb, request->op);

	data->time_now = __bswap_64(ilm_read_utc_time());
	data->countdown = __bswap_64(request->timeout);
	data->class = __bswap_64(request->mode);	/* TODO: Fixup mode in up layer */
	_scsi_data_swap(data->host_id, request->host_id, IDM_HOST_ID_LEN);
	_scsi_data_swap(data->resource_id, request->lock_id, IDM_LOCK_ID_LEN);
	_scsi_data_swap(data->resource_ver, request->lvb, IDM_VALUE_LEN);
	data->resource_ver[0] = request->res_ver_type;

	ret = _scsi_write(request, SG_DXFER_TO_DEV);
	if (ret < 0) {
		ilm_log_err("%s: fail to write scsi %d", __func__, ret);
		return ret;
	}

	return 0;
}

static int _scsi_recv_sync(struct idm_scsi_request *request, char group, int num)
{
	uint8_t *cdb = request->cdb;
	uint8_t *sense = request->sense;
	struct idm_data *data = request->data;

	_scsi_generate_read_cdb(cdb, group, num);

	return _scsi_sg_io(request->drive, cdb, SCSI_CDB_LEN,
			   sense, SCSI_SENSE_LEN,
			   (uint8_t *)data, request->data_len,
			   SG_DXFER_FROM_DEV);
}

static int _scsi_recv_async(struct idm_scsi_request *request, char group, int num)
{
	int ret;

	_scsi_generate_read_cdb(request->cdb, group, num);

	ret = _scsi_write(request, SG_DXFER_FROM_DEV);
	if (ret < 0) {
		ilm_log_err("%s: fail to write scsi %d", __func__, ret);
		return ret;
	}

	return 0;
}

static int _scsi_get_async_result(struct idm_scsi_request *request,
				  int direction)
{
	int ret;

	ret = _scsi_read(request, direction);
        close(request->fd);
	return ret;
}

/**
  * scsi_idm_read_version - Read out IDM spec version
 * @drive:		Drive path name.
 * @version_major:	Returned major version, using "major.minor" version format
 * @version_minor:	Returned minor version, using 'major.minor" version format
*
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int scsi_idm_read_version(char *drive, uint8_t *version_major, uint8_t *version_minor)
{
	uint8_t ver_request[SCSI_VER_INQ_LEN];
	uint8_t sense[SCSI_SENSE_LEN];
	uint8_t data[SCSI_VER_DATA_LEN];

	_scsi_generate_version_inquiry_cdb(ver_request);

	_scsi_sg_io(drive, ver_request, SCSI_VER_INQ_LEN, sense, SCSI_SENSE_LEN, data, SCSI_VER_DATA_LEN, SG_DXFER_FROM_DEV);

	ilm_log_array_warn("SCSI CDB:", (char *)ver_request, SCSI_VER_INQ_LEN);
	ilm_log_array_warn("SCSI DATA:", (char *)data, SCSI_VER_DATA_LEN);
	ilm_log_array_warn("SCSI SENSE:", (char *)sense, SCSI_SENSE_LEN);
	ilm_log_warn("MUTEX VERSION: %u", data[96]);

	//TODO: Update SCSI firmware to use new versioning using the IDM Spec version (major.minor)
	// *version = ((0 << 16) | (data[96] << 8) | (0));
	*version_major = data[96];
	*version_minor = 0;
	return 0;
}

/**
 * scsi_idm_sync_lock - acquire an IDM on a specified drive
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int scsi_idm_sync_lock(char *lock_id, int mode, char *host_id,
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
	if (!request->data) {
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
 * scsi_idm_async_lock - acquire an IDM on a specified drive with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @fd:			File descriptor (emulated with an index).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int scsi_idm_async_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout, uint64_t *handle)
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
	if (!request->data) {
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
	request->data_len = sizeof(struct idm_data);
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_async(request);
	if (ret < 0) {
		ilm_log_err("%s: command fail %d", __func__, ret);
		free(request->data);
		free(request);
		return ret;
	}

	*handle = (uint64_t)request;
	return ret;
}

/**
 * scsi_idm_sync_unlock - release an IDM on a specified drive
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int scsi_idm_sync_unlock(char *lock_id, int mode, char *host_id,
		     char *lvb, int lvb_size, char *drive)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
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

	if (mode == IDM_MODE_EXCLUSIVE)
		mode = IDM_CLASS_EXCLUSIVE;
	else if (mode == IDM_MODE_SHAREABLE)
		mode = IDM_CLASS_SHARED_PROTECTED_READ;

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_UNLOCK;
	request->mode = mode;
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
 * scsi_idm_async_unlock - release an IDM on a specified drive with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int scsi_idm_async_unlock(char *lock_id, int mode, char *host_id,
			   char *lvb, int lvb_size,
			   char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
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
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	memset(request->data, 0x0, sizeof(struct idm_data));
	request->data_len = sizeof(struct idm_data);

	if (mode == IDM_MODE_EXCLUSIVE)
		mode = IDM_CLASS_EXCLUSIVE;
	else if (mode == IDM_MODE_SHAREABLE)
		mode = IDM_CLASS_SHARED_PROTECTED_READ;

	strncpy(request->drive, drive, PATH_MAX);
	request->op = IDM_MUTEX_OP_UNLOCK;
	request->mode = mode;
	request->timeout = 0;
	request->res_ver_type = IDM_RES_VER_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);
	memcpy(request->lvb, lvb, lvb_size);

	ret = _scsi_xfer_async(request);
	if (ret < 0) {
		ilm_log_err("%s: command fail %d", __func__, ret);
		free(request->data);
		free(request);
		return ret;
	}

	*handle = (uint64_t)request;
	return ret;
}

static int scsi_idm_drive_refresh_lock(char *lock_id, int mode, char *host_id,
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
	if (!request->data) {
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
	request->op = IDM_MUTEX_OP_REFRESH;
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

static int scsi_idm_drive_refresh_lock_async(char *lock_id, int mode,
					char *host_id, char *drive,
					uint64_t timeout,
					uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive || !handle)
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
	request->op = IDM_MUTEX_OP_REFRESH;
	request->mode = mode;
	request->timeout = timeout;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	request->data_len = sizeof(struct idm_data);
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_async(request);
	if (ret < 0) {
		ilm_log_err("%s: command fail %d", __func__, ret);
		free(request->data);
		free(request);
		return ret;
	}

	*handle = (uint64_t)request;
	return ret;
}

/**
 * scsi_idm_sync_lock_convert - Convert the lock mode for an IDM
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int scsi_idm_sync_lock_convert(char *lock_id, int mode, char *host_id,
			   char *drive, uint64_t timeout)
{
	return scsi_idm_drive_refresh_lock(lock_id, mode, host_id,
				      drive, timeout);
}

/**
 * scsi_idm_async_lock_convert - Convert the lock mode with async
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @handle:		File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int scsi_idm_async_lock_convert(char *lock_id, int mode, char *host_id,
				 char *drive, uint64_t timeout,
				 uint64_t *handle)
{
	return scsi_idm_drive_refresh_lock_async(lock_id, mode, host_id,
					    drive, timeout, handle);
}

/**
 * scsi_idm_sync_lock_renew - Renew host's membership for an IDM
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int scsi_idm_sync_lock_renew(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout)
{
	return scsi_idm_drive_refresh_lock(lock_id, mode, host_id,
				      drive, timeout);
}

/**
 * scsi_idm_async_lock_renew - Renew host's membership for an IDM
 * 				with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @handle:		File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int scsi_idm_async_lock_renew(char *lock_id, int mode,
			       char *host_id, char *drive,
			       uint64_t timeout,
			       uint64_t *handle)
{
	return scsi_idm_drive_refresh_lock_async(lock_id, mode, host_id,
					    drive, timeout, handle);
}

/**
 * scsi_idm_sync_lock_break - Break an IDM if before other hosts have
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
int scsi_idm_sync_lock_break(char *lock_id, int mode, char *host_id,
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
	if (!request->data) {
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
	request->op = IDM_MUTEX_OP_BREAK;
	request->mode = mode;
	request->timeout = timeout;
	request->data_len = sizeof(struct idm_data);
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
 * scsi_idm_async_lock_break - Break an IDM with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @fd:			File descriptor (emulated in index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_lock_break(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive || !handle)
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
	request->op = IDM_MUTEX_OP_BREAK;
	request->mode = mode;
	request->timeout = timeout;
	request->data_len = sizeof(struct idm_data);
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_async(request);
	if (ret < 0) {
		ilm_log_err("%s: command fail %d", __func__, ret);
		free(request->data);
		free(request);
		return ret;
	}

	*handle = (uint64_t)request;
	return ret;
}

static int scsi_idm_drive_read_mutex_num(char *drive, unsigned int *num)
{
	struct idm_scsi_request *request;
	unsigned char *data;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(IDM_DATA_BLOCK_SIZE);
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}

	strncpy(request->drive, drive, PATH_MAX);
	memset(request->data, 0x0, IDM_DATA_BLOCK_SIZE);
	request->data_len = IDM_DATA_BLOCK_SIZE;

	ret = _scsi_recv_sync(request, IDM_MUTEX_GROUP_INQUIRY, 1);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	data = (unsigned char *)request->data;
	*num = ((data[1]) & 0xff);
	*num |= ((data[0]) & 0xff) << 8;

	ilm_log_dbg("%s: data[0]=%u data[1]=%u mutex num=%u",
		    __func__, data[0], data[1], *num);

	if ((*num > MAX_MUTEX_NUM_WARNING_LIMIT) && (*num <= MAX_MUTEX_NUM_ERROR_LIMIT))
		ilm_log_warn("%s: total mutex_num warning limit exceeded: %d > %d",
			__func__, *num, MAX_MUTEX_NUM_WARNING_LIMIT);
	if (*num > MAX_MUTEX_NUM_ERROR_LIMIT)
		ilm_log_err("%s: total mutex_num error limit exceeded: %d > %d",
			__func__, *num, MAX_MUTEX_NUM_ERROR_LIMIT);
out:
	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_sync_read_lvb - Read value block which is associated to an IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_sync_read_lvb(char *lock_id, char *host_id,
		       char *lvb, int lvb_size, char *drive)
{
	struct idm_scsi_request *request;
	struct idm_data *data;
	int ret, i, block_size;
	unsigned int num = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lvb)
		return -EINVAL;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	if (!num) {
		memset(lvb, 0x0, lvb_size);
		return 0;
	}

	block_size = IDM_DATA_BLOCK_SIZE * num;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(block_size);
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}

	strncpy(request->drive, drive, PATH_MAX);
	memset(request->data, 0x0, block_size);
	request->data_len = block_size;

	ret = _scsi_recv_sync(request, IDM_MUTEX_GROUP, num);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	_scsi_data_swap(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	_scsi_data_swap(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = -ENOENT;
	data = request->data;
	for (i = 0; i < num; i++) {
		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (memcmp(data[i].host_id, request->host_id,
			   IDM_HOST_ID_LEN))
			continue;

		_scsi_data_swap(lvb, data[i].resource_ver, lvb_size);
		ret = 0;
		ilm_log_array_dbg("lvb", lvb, lvb_size);
		break;
	}

	if (ret)
		ilm_log_err("%s: lvb not found: host id(%s), lock id(%s) on %s: %d",
			    __func__, request->host_id, request->lock_id,
			    request->drive, ret);
out:
	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_async_read_lvb - Read value block with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_read_lvb(char *lock_id, char *host_id, char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret, block_size;
	unsigned int num = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	/*
	 * We know there have no any mutex entry in the drive,
	 * the async opertion is devided into two steps: send
	 * the async operation and get the async result.
	 *
	 * In this step, if we directly return success with
	 * count '0', the raid thread will poll forever.  This
	 * is the reason we proceed to send SCSI command to
	 * read back 1 data block and defer to return count.
	 */
	if (!num)
		num = 1;

	block_size = IDM_DATA_BLOCK_SIZE * num;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(block_size);
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = block_size;

	strncpy(request->drive, drive, PATH_MAX);
	_scsi_data_swap(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	_scsi_data_swap(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_recv_async(request, IDM_MUTEX_GROUP, num);
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
 * scsi_idm_async_get_result_lvb - Read the result for read_lvb operation with
 * 				     async mode.
 * @fd:			File descriptor (emulated with index).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_get_result_lvb(uint64_t handle, char *lvb, int lvb_size,
				    int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;
	struct idm_data *data = request->data;
	int ret, i, num;

	ret = _scsi_get_async_result(request, SG_DXFER_FROM_DEV);

	num = request->data_len / IDM_DATA_BLOCK_SIZE;
	for (i = 0; i < num; i++) {
		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (memcmp(data[i].host_id, request->host_id,
			   IDM_HOST_ID_LEN))
			continue;

		_scsi_data_swap(lvb, data[i].resource_ver, lvb_size);
		ret = 0;
		ilm_log_array_dbg("lvb", lvb, lvb_size);
		break;
	}

	*result = ret;

	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_sync_read_lock_count - Read the host count for an IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @count:		Returned count value's pointer.
 * @self:		Returned self count value's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_sync_read_lock_count(char *lock_id, char *host_id,
			 int *count, int *self, char *drive)
{
	struct idm_scsi_request *request;
	struct idm_data *data;
	uint64_t state;
	int ret, i, locked, block_size;
	unsigned int num = 0;

	// Initialize the output
	*count = 0;
	*self = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!count || !self)
		return -EINVAL;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	if (!num)
		return 0;

	block_size = IDM_DATA_BLOCK_SIZE * num;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(block_size);
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}

	strncpy(request->drive, drive, PATH_MAX);
	memset(request->data, 0x0, block_size);
	request->data_len = block_size;

	ret = _scsi_recv_sync(request, IDM_MUTEX_GROUP, num);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	_scsi_data_swap(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	_scsi_data_swap(request->host_id, host_id, IDM_HOST_ID_LEN);

	data = request->data;
	for (i = 0; i < num; i++) {

		state = __bswap_64(data[i].state);
		locked = (state == 0x101) || (state == 0x103);

		if (!locked)
			continue;

		ilm_log_array_dbg("resource_id", data[i].resource_id, IDM_LOCK_ID_LEN);
		ilm_log_array_dbg("lock_id", request->lock_id, IDM_LOCK_ID_LEN);

		ilm_log_array_dbg("data host_id", data[i].host_id, IDM_HOST_ID_LEN);
		ilm_log_array_dbg("host_id", request->host_id, IDM_HOST_ID_LEN);

		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (!memcmp(data[i].host_id, request->host_id,
			    IDM_HOST_ID_LEN)) {
			/* Must be wrong if self has been accounted */
			if (*self) {
				ilm_log_err("%s: duplicate host id (%s) found for lock id (%s) on %s",
					    __func__, request->host_id, request->lock_id,
					    request->drive);
			}

			*self = 1;
		} else {
			*count += 1;
		}
	}

out:
	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_async_read_lock_count - Read the host count for an IDM with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_read_lock_count(char *lock_id, char *host_id,
			       char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret, block_size;
	unsigned int num = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	/*
	 * We know there have no any mutex entry in the drive,
	 * the async opertion is devided into two steps: send
	 * the async operation and get the async result.
	 *
	 * In this step, if we directly return success with
	 * count '0', the raid thread will poll forever.  This
	 * is the reason we proceed to send SCSI command to
	 * read back 1 data block and defer to return count.
	 */
	if (!num)
		num = 1;

	block_size = IDM_DATA_BLOCK_SIZE * num;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(block_size);
	if (!request) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	memset(request->data, 0x0, block_size);
	request->data_len = block_size;

	strncpy(request->drive, drive, PATH_MAX);
	_scsi_data_swap(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	_scsi_data_swap(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_recv_async(request, IDM_MUTEX_GROUP, num);
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
 * scsi_idm_async_get_result_lock_count - Read the result for host count.
 * @fd:			File descriptor (emulated with index).
 * @count:		Returned count value's pointer.
 * @self:		Returned self count value's pointer.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_get_result_lock_count(uint64_t handle, int *count, int *self,
				      int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;
	struct idm_data *data = request->data;
	uint64_t state;
	int ret, i, locked, num;

	ret = _scsi_get_async_result(request, SG_DXFER_FROM_DEV);

	num = request->data_len / IDM_DATA_BLOCK_SIZE;

	*count = 0;
	*self = 0;
	for (i = 0; i < num; i++) {
		state = __bswap_64(data[i].state);
		locked = (state == 0x101) || (state == 0x103);

		if (!locked)
			continue;

		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (!memcmp(data[i].host_id, request->host_id,
			    IDM_HOST_ID_LEN)) {
			/* Must be wrong if self has been accounted */
			if (*self) {
				ilm_log_err("%s: duplicate host id (%s) found for lock id (%s) on %s",
					    __func__, request->host_id, request->lock_id, request->drive);
			}

			*self = 1;
		} else {
			*count += 1;
		}
	}

	*result = ret;

	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_sync_read_lock_mode - Read back an IDM's current mode.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Returned mode's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_sync_read_lock_mode(char *lock_id, int *mode, char *drive)
{
	struct idm_scsi_request *request;
	struct idm_data *data;
	uint64_t state, class;
	int ret, i, block_size;
	unsigned int num = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!mode)
		return -EINVAL;

	if (!lock_id || !drive)
		return -EINVAL;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	if (!num) {
		*mode = IDM_MODE_UNLOCK;
		return 0;
	}

	block_size = IDM_DATA_BLOCK_SIZE * num;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(block_size);
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}

	strncpy(request->drive, drive, PATH_MAX);
	memset(request->data, 0x0, block_size);
	request->data_len = block_size;

	ret = _scsi_recv_sync(request, IDM_MUTEX_GROUP, num);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	_scsi_data_swap(request->lock_id, lock_id, IDM_LOCK_ID_LEN);

	*mode = -1;
	data = request->data;
	for (i = 0; i < num; i++) {

		state = __bswap_64(data[i].state);
		class = __bswap_64(data[i].class);

		ilm_log_dbg("%s: state=%lx class=%lx", __func__, state, class);

		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id, IDM_LOCK_ID_LEN))
			continue;

		if (state == IDM_STATE_UNINIT ||
		    state == IDM_STATE_UNLOCKED ||
		    state == IDM_STATE_TIMEOUT) {
			*mode = IDM_MODE_UNLOCK;
		} else if (class == IDM_CLASS_EXCLUSIVE) {
			*mode = IDM_MODE_EXCLUSIVE;
		} else if (class == IDM_CLASS_SHARED_PROTECTED_READ) {
			*mode = IDM_MODE_SHAREABLE;
		} else if (class == IDM_CLASS_PROTECTED_WRITE) {
			ilm_log_err("%s: PROTECTED_WRITE is not unsupported",
				    __func__);
			ret = -EFAULT;
		}

		ilm_log_dbg("%s: mode=%d", __func__, *mode);

		if (*mode == IDM_MODE_EXCLUSIVE || *mode == IDM_MODE_SHAREABLE)
			break;
	}

	/*
	 * If the mutex is not found in drive fimware,
	 * simply return success and mode is unlocked.
	 */
	if (*mode == -1)
		*mode = IDM_MODE_UNLOCK;

out:
	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_async_read_lock_mode - Read an IDM's mode with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_read_lock_mode(char *lock_id, char *drive, uint64_t *handle)
{
	struct idm_scsi_request *request;
	int ret, block_size;
	unsigned int num = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	/*
	 * We know there have no any mutex entry in the drive,
	 * the async opertion is devided into two steps: send
	 * the async operation and get the async result.
	 *
	 * In this step, if we directly return success with
	 * count '0', the raid thread will poll forever.  This
	 * is the reason we proceed to send SCSI command to
	 * read back 1 data block and defer to return count.
	 */
	if (!num)
		num = 1;

	block_size = IDM_DATA_BLOCK_SIZE * num;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}

	request->data = malloc(block_size);
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}
	request->data_len = block_size;

	strncpy(request->drive, drive, PATH_MAX);
	_scsi_data_swap(request->lock_id, lock_id, IDM_LOCK_ID_LEN);

	ret = _scsi_recv_async(request, IDM_MUTEX_GROUP, num);
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
 * scsi_idm_async_get_result_lock_mode - Read the result for lock mode.
 * @fd:			File descriptor (emulated with index).
 * @mode:		Returned mode's pointer.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_get_result_lock_mode(uint64_t handle, int *mode, int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;
	struct idm_data *data = request->data;
	uint64_t state, class;
	int ret, i, num;

	ret = _scsi_get_async_result(request, SG_DXFER_FROM_DEV);

	num = request->data_len / IDM_DATA_BLOCK_SIZE;

	*mode = -1;
	for (i = 0; i < num; i++) {
		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		state = __bswap_64(data[i].state);
		class = __bswap_64(data[i].class);

		if (state == IDM_STATE_UNINIT ||
		    state == IDM_STATE_UNLOCKED ||
		    state == IDM_STATE_TIMEOUT) {
			*mode = IDM_MODE_UNLOCK;
		} else if (class == IDM_CLASS_EXCLUSIVE) {
			*mode = IDM_MODE_EXCLUSIVE;
		} else if (class == IDM_CLASS_SHARED_PROTECTED_READ) {
			*mode = IDM_MODE_SHAREABLE;
		} else if (class == IDM_CLASS_PROTECTED_WRITE) {
			ilm_log_err("%s: PROTECTED_WRITE is not unsupported",
				    __func__);
			ret = -EFAULT;
			goto out;
		}

		if (*mode == IDM_MODE_EXCLUSIVE || *mode == IDM_MODE_SHAREABLE)
			break;
	}

	if (*mode == -1)
		*mode = IDM_MODE_UNLOCK;

	*result = ret;
out:
	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_async_get_result - Read the result for normal operations.
 * @handle:		SCSI request handle
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_async_get_result(uint64_t handle, int *result)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;

	*result = _scsi_get_async_result(request, SG_DXFER_TO_DEV);
	free(request->data);
	free(request);
	return 0;
}

/**
 * scsi_idm_async_free_result - Free the async result
 * @handle:		SCSI request handle
 *
 * No return value
 */
void scsi_idm_async_free_result(uint64_t handle)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;

	free(request->data);
	free(request);
}

/**
 * scsi_idm_sync_read_host_state - Read back the host's state for an specific IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (64 bytes).
 * @host_state:		Returned host state's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_sync_read_host_state(char *lock_id, char *host_id,
			 int *host_state, char *drive)
{
	struct idm_scsi_request *request;
	struct idm_data *data;
	int ret, i, block_size;
	unsigned int num = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	if (!num) {
		*host_state = -1;
		return 0;
	}

	block_size = IDM_DATA_BLOCK_SIZE * num;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		return -ENOMEM;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(block_size);
	if (!request->data) {
		free(request);
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		return -ENOMEM;
	}

	strncpy(request->drive, drive, PATH_MAX);
	memset(request->data, 0x0, block_size);
	request->data_len = block_size;

	ret = _scsi_recv_sync(request, IDM_MUTEX_GROUP, num);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	_scsi_data_swap(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	_scsi_data_swap(request->host_id, host_id, IDM_HOST_ID_LEN);

	*host_state = -1;
	data = request->data;
	for (i = 0; i < num; i++) {
		/* Skip for other locks */
		if (memcmp(data[i].resource_id, request->lock_id,
			   IDM_LOCK_ID_LEN))
			continue;

		if (memcmp(data[i].host_id, request->host_id,
			   IDM_HOST_ID_LEN))
			continue;

		*host_state = data[i].state;
		break;
	}

out:
	free(request->data);
	free(request);
	return ret;
}

/**
 * scsi_idm_read_whitelist - Read back hosts list for an specific drive.
 * @drive:		Drive path name.
 * @whitelist:		Returned pointer for host's white list.
 * @whitelist:		Returned pointer for host num.
 *
 * Returns zero or a negative error (ie. ENOMEM).
 */
int scsi_idm_read_whitelist(char *drive, char **whitelist, int *whitelist_num)
{
	/* TODO */
	return 0;
}

/**
 * scsi_idm_sync_read_mutex_group - Read back mutex group for all IDM in the drives
 * @drive:		Drive path name.
 * @info_ptr:		Returned pointer for info list.
 * @info_num:		Returned pointer for info num.
 *
 * Returns zero or a negative error (ie. ENOMEM).
 */
int scsi_idm_sync_read_mutex_group(char *drive, struct idm_info **info_ptr, int *info_num)
{
	struct idm_scsi_request *request;
	struct idm_data *data;
	int ret, i, block_size;;
	struct idm_info *info_list, *info;
	uint64_t state, class;
	unsigned int num = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	ret = scsi_idm_drive_read_mutex_num(drive, &num);
	if (ret < 0)
		return -ENOENT;

	if (!num) {
		*info_ptr = NULL;
		*info_num = 0;
		return 0;
	}

	block_size = IDM_DATA_BLOCK_SIZE * num;

	/* Let's allocate for the same item with data block */
	info_list = malloc(sizeof(struct idm_info) * num);
	if (!info_list)
		return -ENOMEM;

	request = malloc(sizeof(struct idm_scsi_request));
	if (!request) {
		ilm_log_err("%s: fail to allocat scsi request", __func__);
		ret = -ENOMEM;
		goto free_info_list;
	}
	memset(request, 0x0, sizeof(struct idm_scsi_request));

	request->data = malloc(block_size);
	if (!request->data) {
		ilm_log_err("%s: fail to allocat scsi data", __func__);
		ret = -ENOMEM;
		goto free_request;
	}

	strncpy(request->drive, drive, PATH_MAX);
	memset(request->data, 0x0, block_size);
	request->data_len = block_size;

	ret = _scsi_recv_sync(request, IDM_MUTEX_GROUP, num);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	data = request->data;
	for (i = 0; i < num; i++) {

		state = __bswap_64(data[i].state);
		if (state == IDM_STATE_DEAD)
			break;

		info = info_list + i;

		/* Copy host ID */
		_scsi_data_swap(info->id, data[i].resource_id, IDM_LOCK_ID_LEN);
		_scsi_data_swap(info->host_id, data[i].host_id, IDM_HOST_ID_LEN);

		class = __bswap_64(data[i].class);

		if (class == IDM_CLASS_EXCLUSIVE) {
			info->mode = IDM_MODE_EXCLUSIVE;
		} else if (class == IDM_CLASS_SHARED_PROTECTED_READ) {
			info->mode = IDM_MODE_SHAREABLE;
		} else {
			ilm_log_err("%s: IDM class is not unsupported %ld",
				    __func__, class);
			ret = -EFAULT;
			goto out;
		}

		if (state == IDM_STATE_UNINIT)
			info->state = -1;
		else if (state == IDM_STATE_UNLOCKED ||
			 state == IDM_STATE_TIMEOUT)
			info->state = IDM_MODE_UNLOCK;
		else
			info->state = 1;

		info->last_renew_time = __bswap_64(data[i].time_now);
	}

	*info_ptr = info_list;
	*info_num = i;

out:
	free(request->data);
free_request:
	free(request);
free_info_list:
	if (ret != 0 && info_list)
		free(info_list);
	return ret;
}

/**
 * scsi_idm_sync_lock_destroy - Destroy an IDM and release all associated resource.
 * @lock_id:		Lock ID (64 bytes).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int scsi_idm_sync_lock_destroy(char *lock_id, int mode, char *host_id,
		      char *drive)
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
	if (!request->data) {
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
	request->op = IDM_MUTEX_OP_DESTROY;
	request->mode = mode;
	request->timeout = 0;
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
 * scsi_idm_async_lock_destroy - Asynchronously destroy an IDM and release
 * all associated resource.
 *
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @handle:		Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int scsi_idm_async_lock_destroy(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t *handle)
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
	if (!request->data) {
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
	request->op = IDM_MUTEX_OP_DESTROY;
	request->mode = mode;
	request->timeout = 0;
	request->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;
	request->data_len = sizeof(struct idm_data);
	memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
	memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);

	ret = _scsi_xfer_async(request);
	if (ret < 0) {
		ilm_log_err("%s: command fail %d", __func__, ret);
		free(request->data);
		free(request);
		return ret;
	}

	*handle = (uint64_t)request;
	return ret;
}

int scsi_idm_get_fd(uint64_t handle)
{
	struct idm_scsi_request *request = (struct idm_scsi_request *)handle;

	return request->fd;
}
