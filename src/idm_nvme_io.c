/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_io.c - Contains the lowest-level function that allow the IDM In-drive Mutex (IDM)
 *                  to talk to the Linux kernel (via ioctl(), read() or write())
 *
 *
 * NOTES on READ\WRITE vs SEND\RECEIVE nomenclature:
 * Want to clarify the difference between these 2 concepts as appiled to this IO file.
 *
 * The only time Read\Write is referred to is when an IDM Read\Write command (opcode_nvme=0xC2\0xC1)
 * is being issued over the NVMe interface
 * VERSUS
 * the actual sending\receiving of specific command structure via the NVMe communications protocol,
 * which currently occurs using a ioctl(), write() or read() system command.
 *
 * For example, when an async IDM Read is issued to the system, the communication to NVMe happens via
 * the system write() command.  So, the concept of R\W becomes somewhat confusing, depending on
 * what "level" of the system you're referring to.
 *
 * In an attempt to reduce confusion, the concepts of SEND\RECEIVE are introduced here to help
 * separate the low-level sending\receiving of NVMe communications from the slightly higher level
 * concept of IDM R\W opcodes.
 *
 *
 *
 * NOTES on ASYNC vs SYNC
 * Some functions have async(asynchronous) or sync(synchronous) in their name, to specify how the
 * function is used.  Functions that have NEITHER word in their name are generic and are intended
 * to be used for both types of functions.
 *
 */

#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/nvme_ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "idm_nvme_io.h"
#include "idm_nvme_utils.h"
#include "log.h"
#include "util.h"


//////////////////////////////////////////
// COMPILE FLAGS
//////////////////////////////////////////
#ifdef MAIN_ACTIVATE_NVME_IO
#define MAIN_ACTIVATE_NVME_IO 1
#else
#define MAIN_ACTIVATE_NVME_IO 0
#endif

#define FUNCTION_ENTRY_DEBUG    //TODO: Remove this entirely???


//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * nvme_idm_async_data_rcv - Asynchronously retrieves the status word and
 * (as needed) data from a specified device for a previously issued NVMe
 * IDM command.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for
 *                  the requested IDM action.
 * @result:         Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_data_rcv(struct idm_nvme_request *request_idm, int *result)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int ret;

	ret = _async_idm_data_rcv(request_idm, result);
	if (ret < 0) {
		ilm_log_err("%s: _async_idm_data_rcv fail %d", __func__, ret);
	}

	close(request_idm->fd_nvme);
	return ret;
}

/**
 * nvme_idm_async_read - Issues a custom (vendor-specific) NVMe command to
 * the drive that then executes a READ action on the IDM.  The specific
 * action performed on the IDM is defined by the IDM opcode set within the
 * NVMe command structure.
 * However, this function issues this NVMe command asychronously.  Therefore,
 * a separate async command must be called to determine the success\failure
 * of the specified IDM opcode.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_read(struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int ret;

	ret = _idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_READ);
	if (ret < 0) {
		return ret;
	}

	// ret = _idm_data_init_rd(request_idm);   TODO: Needed?
	// if (ret < 0) {
	// 	return ret;
	// }

	ret = _async_idm_cmd_send(request_idm);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

/**
 * nvme_idm_async_write - Issues a custom (vendor-specific) NVMe command to
 * the drive that then executes a WRITE action on the IDM.  The specific
 * action performed on the IDM is defined by the IDM opcode set within the
 * NVMe command structure.
 * However, this function issues this NVMe command asychronously.  Therefore,
 * a separate async command must be called to determine the success\failure
 * of the specified IDM opcode.
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_write(struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int ret;

	ret = _idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_WRITE);
	if (ret < 0) {
		return ret;
	}

	ret = _idm_data_init_wrt(request_idm);
	if (ret < 0) {
		return ret;
	}

	ret = _async_idm_cmd_send(request_idm);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

/**
 * nvme_idm_read_init - Initializes an NVMe READ to the IDM by validating
 * and then collecting all the IDM API input params and storing them in the
 * "request_idm" data struct for use later (but before the NVMe write command
 * is sent to the OS kernel).
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @drive:          Drive path name.
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 */
void nvme_idm_read_init(char *drive, struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	request_idm->opcode_idm = IDM_OPCODE_INIT;  //Ignored, but default for all idm reads.
	strncpy(request_idm->drive, drive, PATH_MAX);
}

/**
 * nvme_idm_write_init - Initializes an NVMe WRITE to the IDM by validating
 * and then collecting all the IDM API input params and storing them in the
 * "request_idm" data struct for use later (but before the NVMe write command
 * is sent to the OS kernel).
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @lock_id:        Lock ID (64 bytes).
 * @mode:           Lock mode (unlock, shareable, exclusive).
 * @host_id:        Host ID (32 bytes).
 * @drive:          Drive path name.
 * @timeout:        Timeout for membership (unit: millisecond).
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_write_init(char *lock_id, int mode, char *host_id, char *drive,
                        uint64_t timeout, struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	//Cache the input params
//TODO: memcpy() -OR- strncpy() HERE?? (and everywhere else for that matter)
	memcpy(request_idm->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);
	memcpy(request_idm->host_id, host_id, IDM_HOST_ID_LEN_BYTES);
	memcpy(request_idm->drive  , drive  , PATH_MAX);
	request_idm->mode_idm = mode;
	request_idm->timeout  = timeout;

	//Currently fixed for NVME writes
//TODO: This is variable for NVMe reads.  How handle?  MAY be variable for writes too (future).
	request_idm->group_idm = IDM_GROUP_DEFAULT;

	switch(mode) {
		case IDM_MODE_EXCLUSIVE:
			request_idm->class = IDM_CLASS_EXCLUSIVE;
			break;
		case IDM_MODE_SHAREABLE:
			request_idm->class = IDM_CLASS_SHARED_PROTECTED_READ;
			break;
		default:
			//Other modes not supported at this time
			return -EINVAL;
	}

	return SUCCESS;
}

/**
 * nvme_idm_sync_read - Issues a custom (vendor-specific) NVMe command to
 * the drive that then executes a READ action on the IDM.  The specific action
 * performed on the IDM is defined by the IDM opcode set within the NVMe
 * command structure.
 * Intended to be called by higher level read IDM API's.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_read(struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int ret;

	ret = _idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_READ);
	if (ret < 0) {
		return ret;
	}

	// ret = _idm_data_init_rd(request_idm);   TODO: Needed?
	// if (ret < 0) {
	// 	return ret;
	// }

	ret = _sync_idm_cmd_send(request_idm);
	if (ret < 0) {
		return ret;
	}

	ilm_log_dbg("%s: retrieved struct idm_data", __func__);
//TODO: Put on debug flag OR remove??
	dumpIdmDataStruct(request_idm->data_idm);

	return ret;
}

/**
 * nvme_idm_sync_write - Issues a custom (vendor-specific) NVMe command to
 * the device that then executes a WRITE action on the IDM.  The specific action
 * performed on the IDM is defined by the IDM opcode set within the NVMe
 * command structure.
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_write(struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int ret;

	ret = _idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_WRITE);
	if (ret < 0) {
		return ret;
	}

	ret = _idm_data_init_wrt(request_idm);
	if (ret < 0) {
		return ret;
	}

	ret = _sync_idm_cmd_send(request_idm);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

/**
 * _async_idm_cmd_send - Forms and then sends an NVMe IDM command to the
 * system device, but does so asynchronously.
 * Does so by transfering data from the "request_idm" struct to the final
 * "nvme_passthru_cmd" struct used by the system.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _async_idm_cmd_send(struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int fd_nvme;
	int ret = FAILURE;

	//TODO: Put this under a debug flag of some kind??
	dumpNvmeCmdStruct(&request_idm->cmd_nvme, 1, 1);
	dumpIdmDataStruct(request_idm->data_idm);

	fd_nvme = open(request_idm->drive, O_RDWR | O_NONBLOCK);
	if (fd_nvme < 0) {
		ilm_log_err("%s: error opening drive %s fd %d",
		            __func__, request_idm->drive, fd_nvme);
		ret = fd_nvme;
		goto EXIT;
	}
	request_idm->fd_nvme = fd_nvme;  //Used here and saved for later

	ret = ani_send_cmd(request_idm, NVME_IOCTL_ADMIN_CMD);
	if (ret) {
		close(fd_nvme);
		ilm_log_err("%s: send failed: %d(0x%X)", __func__, ret, ret);
	}
EXIT:
	return ret;
}

/**
 * _async_idm_data_rcv - Asynchronously retrieves the status word and
 * (as needed) data from a specified device for a previously issued NVMe
 * IDM command.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 * @result:         Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _async_idm_data_rcv(struct idm_nvme_request *request_idm, int *result)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int result_ani;
	int ret;

	if (!request_idm->fd_nvme) {
		ilm_log_err("%s: invalid device handle", __func__);
		ret = FAILURE;
		goto EXIT;
	}

	ret = ani_data_rcv(request_idm, &result_ani);
	if (ret < 0) {
		ilm_log_err("%s: ani_data_rcv fail", __func__);
		goto EXIT;
	}

	*result = _idm_cmd_check_status(result_ani, request_idm->opcode_idm);

	// //TODO: Put this under a debug flag of some kind??
	// dumpIdmDataStruct(request_idm->data_idm);

	ilm_log_dbg("%s: result found: ani:%d(0x%X), final:%d(0x%X)",
	       __func__, result_ani, result_ani, *result, *result);
EXIT:
	return ret;
}

/**
 * _idm_cmd_check_status -  Evaluates the NVMe command status word returned
 * from a device.
 *
 * @status:     The status code returned by a system device after the
 *              completed NVMe command request.
 * @opcode_idm: IDM-specific opcode specifying the desired IDM action
 *              performed by the device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _idm_cmd_check_status(int status, uint8_t opcode_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	uint16_t sct, sc;   // status code type, status code
	int ret;

	if (status <= 0)
		return status;  // Just return the status code on success or pre-existing negative error.

	sc  = status & STATUS_CODE_MASK;
	sct = (status & STATUS_CODE_TYPE_MASK) >> STATUS_CODE_TYPE_RSHIFT;
	ilm_log_dbg("%s: for opcode_idm=0x%X: sct=0x%X, sc=0x%X", __func__, opcode_idm, sct, sc);

	switch(sc) {
		case NVME_IDM_ERR_MUTEX_OP_FAILURE:
//TODO: Replace all these with ilm_log_dbg() calls????? (If so, remove the "\n")
			printf("NVME_IDM_ERR_MUTEX_OP_FAILURE\n");
			ret = -EINVAL;
			break;
		case NVME_IDM_ERR_MUTEX_REVERSE_OWNER_CHECK_FAILURE:
			printf("NVME_IDM_ERR_MUTEX_REVERSE_OWNER_CHECK_FAILURE\n");
			ret = -EINVAL;
			break;
		case NVME_IDM_ERR_MUTEX_OP_FAILURE_STATE:
			printf("NVME_IDM_ERR_MUTEX_OP_FAILURE_STATE\n");
			ret = -EINVAL;
			break;
		case NVME_IDM_ERR_MUTEX_OP_FAILURE_CLASS:
			printf("NVME_IDM_ERR_MUTEX_OP_FAILURE_CLASS\n");
			ret = -EINVAL;
			break;
		case NVME_IDM_ERR_MUTEX_OP_FAILURE_OWNER:
			printf("NVME_IDM_ERR_MUTEX_OP_FAILURE_OWNER\n");
			ret = -EINVAL;
			break;
		case NVME_IDM_ERR_MUTEX_OPCODE_INVALID:
			printf("NVME_IDM_ERR_MUTEX_OPCODE_INVALID\n");
			ret = -EINVAL;
			break;
		case NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED:
			printf("NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED\n");
			ret = -ENOMEM;
			break;
		case NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_HOST:
			printf("NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_HOST\n");
			ret = -ENOMEM;
			break;
		case NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_SHARED_HOST:
			printf("NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_SHARED_HOST\n");
			ret = -ENOMEM;
			break;
		case NVME_IDM_ERR_MUTEX_GROUP_INVALID:
			printf("NVME_IDM_ERR_MUTEX_GROUP_INVALID\n");
			ret = -ENOENT;
			break;
		case NVME_IDM_ERR_MUTEX_RESERVATION_CONFLICT:
			printf("NVME_IDM_ERR_MUTEX_RESERVATION_CONFLICT\n");
			switch(opcode_idm) {
				case IDM_OPCODE_REFRESH:
					printf("Bad refresh: timeout\n");
					ret = -ETIME;
					break;
				case IDM_OPCODE_UNLOCK:
					printf("Bad unlock\n");
					ret = -ENOENT;
					break;
				default:
					printf("Busy\n");
					ret = -EBUSY;
			}
			break;
		case NVME_IDM_ERR_MUTEX_COMMAND_TERMINATED:
			printf("NVME_IDM_ERR_MUTEX_COMMAND_TERMINATED\n");
			switch(opcode_idm) {
				case IDM_OPCODE_REFRESH:
					printf("Bad refresh: timeout\n");
					ret = -EPERM;
					break;
				case IDM_OPCODE_UNLOCK:
					printf("Bad unlock\n");
					ret = -EINVAL;
					break;
				default:
					printf("Try again\n");
					ret = -EAGAIN;
			}
			break;
		case NVME_IDM_ERR_MUTEX_BUSY:
			printf("NVME_IDM_ERR_MUTEX_BUSY\n");
			ret = -EBUSY;
			break;
		default:
			ilm_log_err("%s: unknown status code %d(0x%X)",
			            __func__, sc, sc);
			ret = -EINVAL;
	}

	return ret;
}

/**
 * _idm_cmd_init -  Initializes the NVMe Vendor Specific Command command
 * struct.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 * @opcode_nvme:    NVMe-specific opcode specifying the desired NVMe action to
 *                  perform on the IDM.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _idm_cmd_init(struct idm_nvme_request *request_idm, uint8_t opcode_nvme)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	struct nvme_idm_vendor_cmd *cmd_nvme = &request_idm->cmd_nvme;

	cmd_nvme->opcode_nvme        = opcode_nvme;
	cmd_nvme->addr               = (uint64_t)(uintptr_t)request_idm->data_idm;
	cmd_nvme->data_len           = request_idm->data_len;
	cmd_nvme->ndt                = request_idm->data_len / 4;
	cmd_nvme->opcode_idm_bits7_4 = request_idm->opcode_idm << 4;
	cmd_nvme->group_idm          = request_idm->group_idm;
	cmd_nvme->timeout_ms         = VENDOR_CMD_TIMEOUT_DEFAULT;

	return SUCCESS;
}

/**
 * _idm_data_init_wrt -  Initializes the IDM's data struct prior to an
 * IDM write.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action, including the target idm_data struct.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _idm_data_init_wrt(struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	struct idm_data *data_idm = request_idm->data_idm;

	data_idm->time_now  = __bswap_64(ilm_read_utc_time());
	data_idm->countdown = __bswap_64(request_idm->timeout);
	data_idm->class     = __bswap_64(request_idm->class);

	bswap_char_arr(data_idm->host_id,      request_idm->host_id, IDM_HOST_ID_LEN_BYTES);
	bswap_char_arr(data_idm->resource_id,  request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);
	bswap_char_arr(data_idm->resource_ver, request_idm->lvb    , IDM_LVB_LEN_BYTES);
	data_idm->resource_ver[0] = request_idm->res_ver_type;

//TODO: DELETE.  Frederick's exact data payload used during firmware debug
	// data_idm->state           = 0x1111111111111111;
	// data_idm->time_now        = 0x1111111111111111;
	// data_idm->countdown       = 0x6666666666777777;
	// data_idm->class           = 0x0;
	// data_idm->resource_ver[0] = (char)0x01;
	// data_idm->resource_ver[1] = (char)0x00;
	// data_idm->resource_ver[2] = (char)0x11;
	// data_idm->resource_ver[3] = (char)0x11;
	// data_idm->resource_ver[4] = (char)0x33;
	// data_idm->resource_ver[5] = (char)0x21;
	// data_idm->resource_ver[6] = (char)0x2E;
	// data_idm->resource_ver[7] = (char)0xEE;
	// memset(data_idm->rsvd0, 0x0, IDM_DATA_RESERVED_0_LEN_BYTES);
	// data_idm->resource_id[60] = (char)0x11;
	// data_idm->resource_id[61] = (char)0x11;
	// data_idm->resource_id[62] = (char)0x11;
	// data_idm->resource_id[63] = (char)0x11;
	// data_idm->metadata[62]    = (char)0xFE;
	// data_idm->metadata[63]    = (char)0xED;
	// memset(data_idm->host_id, 0x31, IDM_HOST_ID_LEN_BYTES);
	// data_idm->rsvd1[30]       = (char)0xCC;
	// data_idm->rsvd1[31]       = (char)0xCC;

	return SUCCESS;
}

/**
 * _sync_idm_cmd_send - Forms and then sends an NVMe IDM command to the
 * system device.
 * Does so by transfering data from the "request_idm" struct to the final
 * "nvme_passthru_cmd" struct used by the system.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _sync_idm_cmd_send(struct idm_nvme_request *request_idm)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	struct nvme_passthru_cmd cmd_nvme_passthru;
	int fd_nvme;
	int ret = FAILURE;

	//TODO: Put this under a debug flag of some kind??
	dumpNvmeCmdStruct(&request_idm->cmd_nvme, 1, 1);
	dumpIdmDataStruct(request_idm->data_idm);

	memset(&cmd_nvme_passthru, 0, sizeof(struct nvme_passthru_cmd));

	fd_nvme = open(request_idm->drive, O_RDWR | O_NONBLOCK);
	if (fd_nvme < 0) {
		ilm_log_err("%s: error opening drive %s fd %d",
		            __func__, request_idm->drive, fd_nvme);
		return fd_nvme;
	}

	fill_nvme_cmd(request_idm, &cmd_nvme_passthru);

	//TODO: Keep?  Add debug flag?
	dumpNvmePassthruCmd(&cmd_nvme_passthru);

	//ioctl()'s return value comes from:
	//  NVMe Completion Queue Entry (CQE) DWORD3:
	//    DW3[31:17] - Status Bit Field Definitions
	//      DW3[31]:    Do Not Retry (DNR)
	//      DW3[30]:    More (M)
	//      DW3[29:28]: Command Retry Delay (CRD)
	//      DW3[27:25]: Status Code Type (SCT)
	//      DW3[24:17]: Status Code (SC)
	//Refer to the NVMe spec for more details.
	//
	//So, here, ioctl()'s return value is defined as:
	//  ret[14]:    Do Not Retry (DNR)
	//  ret[13]:    More (M)
	//  ret[12:11]: Command Retry Delay (CRD)
	//  ret[10:8]:  Status Code Type (SCT)
	//  ret[7:0]:   Status Code (SC)
	//NOTE: Only SC and SCT are actively used by the IDM firmware.
	//          The rest are "normally" 0.  However, the system can use them.
	ret = ioctl(fd_nvme, NVME_IOCTL_ADMIN_CMD, &cmd_nvme_passthru);
	if(ret) {
		ilm_log_err("%s: ioctl failed: %d(0x%X)", __func__, ret, ret);
	}

	ret = _idm_cmd_check_status(ret, request_idm->opcode_idm);

	close(fd_nvme);
	return ret;
}





#if MAIN_ACTIVATE_NVME_IO
/*#########################################################################################
########################### STAND-ALONE MAIN ##############################################
#########################################################################################*/
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1";

//To compile:
//gcc idm_nvme_io.c -o idm_nvme_io
int main(int argc, char *argv[])
{
    char *drive;

    if(argc >= 3){
        drive = argv[2];
    }
    else {
        drive = DRIVE_DEFAULT_DEVICE;
    }

    //cli usage: idm_nvme_io write
    if(argc >= 2){
        if(strcmp(argv[1], "write") == 0){

            //Simulated ILM input
            char        lock_id[64] = "lock_id";
            int         mode        = IDM_MODE_EXCLUSIVE;
            char        host_id[32] = "host_id";
            uint64_t    timeout     = 10;

            //Create required input structs the IDM API would normally create
            struct idm_nvme_request *request_idm = NULL;
            int ret = FAILURE;

            ret = nvme_idm_write_init(lock_id, mode, host_id, drive, timeout,
                                      request_idm);
            printf("%s exiting with %d\n", argv[1], ret);

            ret = nvme_idm_sync_write(request_idm);
            printf("%s exiting with %d\n", argv[1], ret);
        }

    }


    return 0;
}
#endif//MAIN_ACTIVATE_NVME_IO
