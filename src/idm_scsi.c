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
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "ilm.h"

#include "idm_wrapper.h"
#include "inject_fault.h"
#include "list.h"
#include "log.h"
#include "util.h"

#define IDM_MUTEX_OP_INIT	0x1
#define IDM_MUTEX_OP_TRYLOCK	0x2
#define IDM_MUTEX_OP_LOCK	0x3
#define IDM_MUTEX_OP_UNLOCK	0x4
#define IDM_MUTEX_OP_REFRESH	0x5
#define IDM_MUTEX_OP_BREAK	0x6
#define IDM_MUTEX_OP_DESTROY	0x7

#define IDM_RES_VER_NO_UPDATE_NO_VALID		0x0
#define IDM_RES_VER_UPDATE_NO_VALID		0x1
#define IDM_RES_VER_UPDATE_VALID		0x2
#define IDM_RES_VER_INVALID			0x3

/* Now simply read out data with predefined size: 512B * 1000 = 500KB */
#define IDM_DATA_BLOCK_SIZE	512
#define IDM_DATA_BLOCK_NUM	1000
#define IDM_DATA_SIZE		(IDM_DATA_BLOCK_SIZE * IDM_DATA_BLOCK_NUM)

#define IDM_STATE_UNINIT		0
#define IDM_STATE_LOCKED		0x101
#define IDM_STATE_UNLOCKED		0x102
#define IDM_STATE_MULTIPLE_LOCKED	0x103
#define IDM_STATE_TIMEOUT		0x104

#define IDM_CLASS_EXCLUSIVE			0x0
#define IDM_CLASS_PROTECTED_WRITE		0x1
#define IDM_CLASS_SHARED_PROTECTED_READ		0x2

#define IDM_SCSI_WRITE				0x89  /* Or change to use 0x8E */
#define IDM_SCSI_READ				0x88  /* Or change to use 0x8E */

struct idm_data {
	char state[8];  /* ignored when write */
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

static tDevice *_alloc_device_handle(void)
{
	tDevice *dev_handle;

	dev_handle = malloc(sizeof(tDevice));
	if (!dev_handle)
		return NULL;

	memset(dev_handle, 0x0, sizeof(tDevice));

	dev_handle->dFlags = OPEN_HANDLE_ONLY;

	/* Later can change to VERBOSITY_COMMAND_NAMES */
	dev_handle->deviceVerbosity = VERBOSITY_COMMAND_VERBOSE;
	dev_handle->sanity.size = sizeof(tDevice);
	dev_handle->sanity.version = DEVICE_BLOCK_VERSION;
	return dev_handle;
}

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
	cdb[2] = 0x0;			/* MUTEX GROUP=0 as default value */
	cdb[3] = 0x0;			/* cdb[3..9] are ignored */
	cdb[4] = 0x0;
	cdb[5] = 0x0;
	cdb[6] = 0x0;
	cdb[7] = 0x0;
	cdb[8] = 0x0;
	cdb[9] = 0x0;
	cdb[10] = RESERVED;
	cdb[11] = RESERVED;
	cdb[12] = RESERVED;
	cdb[13] = 0x1;			/* Number of logical blocks = 1 */
	cdb[14] = RESERVED;		/* Mutex OP = TryLock */
	cdb[14] &= 0xc0;
	cdb[14] |= op;
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
	cdb[10] = M_Byte3(IDM_DATA_BLOCK_NUM);
	cdb[11] = M_Byte2(IDM_DATA_BLOCK_NUM);
	cdb[12] = M_Byte1(IDM_DATA_BLOCK_NUM);
	cdb[13] = M_Byte0(IDM_DATA_BLOCK_NUM);
	cdb[14] = 0;			/* DLD1=0 DLD0=0 Group number=0 */
	cdb[15] = 0;			/* Control = 0 */
}

static int _scsi_write_command(char *drive, int op,
			       char *resource_id, int mode,
			       char *host_id, uint64_t timeout,
			       int res_ver_type, char *lvb, int lvb_size)
{
	uint8_t cdb[CDB_LEN_16] = { 0 };
	uint8_t sense[256] = { 0 };
	struct idm_data data;
	sg_io_hdr_t io_hdr;
	int status;

	_scsi_generate_write_cdb(cdb, op);

	data.time_now = ilm_read_utc_time();
	data.countdown = timeout;
	data.class = mode;		/* TODO: Fixup mode in up layer */
	data.resource_ver[7] = res_ver_type;
	memcpy(data.host_id, host_id, IDM_HOST_ID_LEN);
	if (resource_id)
		memcpy(data.resource_id, resource_id, IDM_LOCK_ID_LEN);

	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof(cdb);
	io_hdr.cmdp = cdb;
	/* io_hdr.iovec_count = 0; */  /* memset takes care of this */
	io_hdr.mx_sb_len = sizeof(sense);
	io_hdr.sbp = sense;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof(struct idm_data);
	io_hdr.dxferp = &data;
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
	if ((io_hdr.info & SG_INFO_OK_MASK) == SG_INFO_OK)
       		return 0;

	status = io_hdr.masked_status;
	switch (status) {
	case SAM_STAT_CHECK_CONDITION:
		if (!memcmp(sense, sense_invalid_opcode,
			    sizeof(sense_invalid_opcode))) {
			ilm_log_err("%s: unsupported opcode=%d",
				    __func__, op);
			ret = -EINVAL;
			break;
		}

		/* check if LBA is out of range */
		if (!memcmp(sense, sense_lba_oor, sizeof(sense_lba_oor))) {
			ilm_log_err("%s: LBA is out of range=%d",
				    __func__, op);
			ret = -EINVAL;
			break;
		}

		/* Otherwise, also reports error */
		ilm_log_array_err("sense:", sense, sizeof(sense));
		ret = -EINVAL;
		break;

	case SAM_STAT_RESERVATION_CONFLICT:
		if (op == IDM_MUTEX_OP_REFRESH)
			ret = -ETIME;
		else
			ret = -EAGAIN;
		break;

	case SAM_STAT_BUSY:
		ret = -EBUSY;
		break;

	/*
	 * Take this case as success since the IDM has achieved
	 * the required state.
	 */
	case SAM_STAT_COMMAND_TERMINATED:
		ret = 0;
		break;

	default:
		ilm_log_err("%s: unknown status %d", __func__, status);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int _scsi_read_command(tDevice *dev_handle,
			      struct idm_data **data,
			      int *data_len)
{
	uint8_t cdb[CDB_LEN_16] = { 0 };
	uint8_t sense[256] = { 0 };

	*data = malloc(IDM_DATA_SIZE);
	if (!*data)
		return -ENOMEM;

	*data_len = IDM_DATA_SIZE;

	_scsi_generate_read_cdb(cdb);

	ret = scsi_Send_Cdb(dev_handle, cdb, sizeof(cdb),
			    *data, *data_len,
			    XFER_DATA_IN, sense, 256, 15 /* timeout */);
	if (ret) {
		ilm_log_err("%s: fail to send cdb %d", __func__, ret);
		goto out;
	}

	return 0;
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
	tDevice *dev_handle = NULL;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = get_Device(drive, dev_handle);
	if (ret) {
		ilm_log_err("%s: fail to get device %d", __func__, ret);
		ret = -EIO;
		goto out;
	}

	ret = _scsi_write_command(dev_handle, IDM_MUTEX_OP_TRYLOCK,
				  lock_id, mode, host_id, timeout,
				  IDM_RES_VER_NO_UPDATE_NO_VALID,
				  NULL, 0);
	if (ret)
		ilm_log_err("%s: command fail %d", __func__, ret);

out:
	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
			 char *drive, uint64_t timeout, int *fd)
{

	/* TODO */
	return 0;
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
	tDevice *dev_handle = NULL;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	if (lvb_size > IDM_VALUE_LEN)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = get_Device(drive, dev_handle);
	if (ret) {
		ilm_log_err("%s: fail to get device %d", __func__, ret);
		ret = -EIO;
		goto out;
	}

	ret = _scsi_write_command(dev_handle, IDM_MUTEX_OP_UNLOCK,
				  lock_id, 0, host_id, 0,
				  IDM_RES_VER_UPDATE_NO_VALID,
				  lvb, lvb_size);
	if (ret)
		ilm_log_err("%s: command fail %d", __func__, ret);

out:
	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
			   void *lvb, int lvb_size, char *drive, int *fd)
{
	/* TODO */
	return 0;
}

int idm_drive_refresh_lock(char *lock_id, int mode, char *host_id, char *drive)
{
	tDevice *dev_handle = NULL;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = get_Device(drive, dev_handle);
	if (ret) {
		ilm_log_err("%s: fail to get device %d", __func__, ret);
		ret = -EIO;
		goto out;
	}

	ret = _scsi_write_command(dev_handle, IDM_MUTEX_OP_REFRESH,
				  lock_id, mode, host_id, 0,
				  IDM_RES_VER_NO_UPDATE_NO_VALID,
				  NULL, 0);
	if (ret)
		ilm_log_err("%s: command fail %d", __func__, ret);

out:
	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
				 char *drive, int *fd)
{
	/* TODO */
	return 0;
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
			       char *host_id, char *drive, int *fd)
{
	/* TODO */
	return 0;
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
	tDevice *dev_handle = NULL;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = get_Device(drive, dev_handle);
	if (ret) {
		ilm_log_err("%s: fail to get device %d", __func__, ret);
		ret = -EIO;
		goto out;
	}

	ret = _scsi_write_command(dev_handle, IDM_MUTEX_OP_BREAK,
				  lock_id, mode, host_id, timeout,
				  IDM_RES_VER_NO_UPDATE_NO_VALID,
				  NULL, 0);
	if (ret)
		ilm_log_err("%s: command fail %d", __func__, ret);

out:
	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
			       char *drive, uint64_t timeout, int *fd)
{
	/* TODO */
	return 0;
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
	tDevice *dev_handle;
	struct idm_data *data = NULL;
	int data_len;
	int ret, i;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!count)
		return -EINVAL;

	if (!lock_id || !drive)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = _scsi_read_command(dev_handle, &data, &data_len);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	*count = 0;
	*self = 0;
	for (i = 0; i < data_len; i++) {
		/* Skip for other locks */
		if (memcpy(data[i].resource_id, lock_id, IDM_LOCK_ID_LEN))
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

	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
			       char *drive, int *fd)
{
	/* TODO */
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
int idm_drive_lock_count_async_result(int fd, int *count, int *self,
				      int *result)
{
	/* TODO */
	return 0;
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
	tDevice *dev_handle;
	struct idm_data *data = NULL;
	int data_len;
	int ret, i;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!mode)
		return -EINVAL;

	if (!lock_id || !drive)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = _scsi_read_command(dev_handle, &data, &data_len);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	*mode = -1;
	for (i = 0; i < data_len; i++) {
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

	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
int idm_drive_lock_mode_async(char *lock_id, char *drive, int *fd)
{
	/* TODO */
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
int idm_drive_lock_mode_async_result(int fd, int *mode, int *result)
{
	/* TODO */
	return 0;
}

/**
 * idm_drive_async_result - Read the result for normal operations.
 * @fd:			File descriptor (emulated with index).
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_async_result(int fd, int *result)
{
	/* TODO */
	return -1;
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
int idm_drive_host_state(char *lock_id,
			 char *host_id,
			 int *host_state,
			 char *drive)
{
	tDevice *dev_handle;
	struct idm_data *data = NULL;
	int data_len;
	int ret, i;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = _scsi_read_command(dev_handle, &data, &data_len);
	if (ret < 0) {
		ilm_log_err("%s: fail to read data %d", __func__, ret);
		goto out;
	}

	*mode = -1;
	for (i = 0; i < data_len; i++) {
		/* Skip for other locks */
		if (memcpy(data[i].resource_id, lock_id, IDM_LOCK_ID_LEN) ||
		    memcpy(data[i].host_id, host_id, IDM_HOST_ID_LEN))
			continue;

		*host_state = data[i]->state;
		break;
	}

out:
	if (data)
		free(data);

	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
	tDevice *dev_handle;
	struct idm_data *data = NULL;
	int data_len;
	int ret, i;
	struct idm_info *info_list, *info;
	int max_alloc = 8;

	/* Let's firstly assume to allocet for 8 items */
	info_list = malloc(sizeof(struct idm_info) * max_alloc);
	if (!info_list)
		return -ENOMEM;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		free(info_list);
		return -ENOMEM;
	}

	ret = _scsi_read_command(dev_handle, &data, &data_len);
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

	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

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
	tDevice *dev_handle = NULL;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !drive)
		return -EINVAL;

	dev_handle = _alloc_device_handle();
	if (!dev_handle) {
		ilm_log_err("%s: fail to alloc device", __func__);
		return -ENOMEM;
	}

	ret = get_Device(drive, dev_handle);
	if (ret) {
		ilm_log_err("%s: fail to get device %d", __func__, ret);
		ret = -EIO;
		goto out;
	}

	ret = _scsi_write_command(dev_handle, IDM_MUTEX_OP_DESTROY,
				  lock_id, 0, NULL, 0,
				  IDM_RES_VER_NO_UPDATE_NO_VALID,
				  NULL, 0);
	if (ret)
		ilm_log_err("%s: command fail %d", __func__, ret);

out:
	if (dev_handle && dev_handle->os_info.fd >= 0)
		close_Device(dev_handle);

	if (dev_handle)
		free(dev_handle);

	return ret;
}
