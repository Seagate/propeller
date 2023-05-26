/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_api.c - Primary NVMe interface for In-drive Mutex (IDM)
 *
 *
 * NOTES on IDM API:
 * The term IDM (In-Drive Mutex) API is defined here as the main interface to
 * the "lower-level" of the Propeller code (in that "lower" is "closer" to
 * the hardware).  These functions interact with the OS to write and\or read
 * to the target device's IDM. The functions in this file represent the
 * NVMe-specific versions of the IDM APIs.  These IDM APIs are intended to be
 * called by the ILM "layer" of Propeller (which is, in turn, called by external
 * programs such as Linux's lvm2.
 */

////////////////////////////////////////////////////////////////////////////////
// COMPILE FLAGS
////////////////////////////////////////////////////////////////////////////////
/* For using internal main() for stand-alone debug compilation.
Setup to be gcc-defined (-D) in make file */
#ifdef DBG__NVME_API_MAIN_ENABLE
#define DBG__NVME_API_MAIN_ENABLE 1
#else
#define DBG__NVME_API_MAIN_ENABLE 0
#endif

/* Define for logging a function's name each time it is entered. */
#define DBG__LOG_FUNC_ENTRY

/* Defines for logging struct field data for important data structs */
#define DBG__DUMP_STRUCTS

////////////////////////////////////////////////////////////////////////////////
// CONSTANT
////////////////////////////////////////////////////////////////////////////////
/* This value (from firmware) represent version 1.0.
To start, just needed a minimum value > 0 */
#define MIN_IDM_VERSION		10

////////////////////////////////////////////////////////////////////////////////
// INCLUDES
////////////////////////////////////////////////////////////////////////////////
#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "idm_nvme_api.h"
#include "idm_nvme_io.h"
#include "idm_nvme_io_admin.h"
#include "idm_nvme_utils.h"
#include "inject_fault.h"
#include "log.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/**
 * nvme_idm_async_free_result - Free the async result
 *
 * @handle:      NVMe request handle for the previously sent NVMe cmd.
 *
 * No return value
 */
void nvme_idm_async_free_result(uint64_t handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = (struct idm_nvme_request *)handle;

	_memory_free_idm_request(request_idm);
}

/**
 * nvme_idm_async_get_result - Retreive the result for normal async operations.
 *
 * @handle:      NVMe request handle for the previously sent NVMe cmd.
 * @result:      Returned result (0 or -ve value) for the previously sent NVMe
 *               command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_get_result(uint64_t handle, int *result)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = (struct idm_nvme_request *)handle;
	int ret;

	ret = nvme_idm_async_data_rcv(request_idm, result);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_data_rcv fail %d",
		            __func__, ret);
		goto EXIT;
	}

	//TODO: How should I handle the error code from the current VS the
	//      previous command??
	//          Especially as compared to what the SCSI code does.
	//          Talk to Tom.
	if (*result < 0) {
		ilm_log_err("%s: previous async cmd fail result=%d",
		            __func__, *result);
	}

EXIT:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_async_get_result_lock_count - Asynchronously retreive the result
 * for a previous asynchronous read lock count request.
 *
 * @handle:     NVMe request handle for the previously sent NVMe cmd.
 * @count:      Returned lock count.
 * @self:       Returned self count.
 * @result:     Returned result (0 or -ve value) for the previously sent
 *              NVMe command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_get_result_lock_count(uint64_t handle, int *count,
                                         int *self, int *result)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = (struct idm_nvme_request *)handle;
	int ret;

	// Initialize the return parameters
	*count = 0;
	*self  = 0;

	ret = nvme_idm_async_data_rcv(request_idm, result);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_data_rcv fail %d",
		            __func__, ret);
		goto EXIT_FAIL;
	}

	if (*result < 0) {
		ilm_log_err("%s: previous async cmd fail: result=%d",
		            __func__, *result);
		goto EXIT_FAIL;
	}

	ret = _parse_lock_count(request_idm, count, self);
	if (ret < 0) {
		ilm_log_err("%s: _parse_lock_count fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ilm_log_dbg("%s: found: lock_count=%d, self_count=%d",
	            __func__, *count, *self);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_async_get_result_lock_mode - Asynchronously retreive the result
 * for a previous asynchronous read lock mode request.
 *
 * @handle:     NVMe request handle for the previously sent NVMe cmd.
 * @mode:       Returned lock mode (unlock, shareable, exclusive).
 *              Referenced value set to -1 on error.
 * @result:     Returned result (0 or -ve value) for the previously sent
 *              NVMe command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_get_result_lock_mode(uint64_t handle, int *mode,
                                        int *result)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = (struct idm_nvme_request *)handle;
	int ret;

	// Initialize the return parameter
	*mode = -1;    //TODO: hardcoded state. add an "error" state to the enum?

	ret = nvme_idm_async_data_rcv(request_idm, result);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_data_rcv fail %d",
		            __func__, ret);
	}

	if (*result < 0) {
		ilm_log_err("%s: previous async cmd fail: result=%d",
		            __func__, *result);
		goto EXIT_FAIL;
	}

	ret = _parse_lock_mode(request_idm, mode);
	if (ret < 0) {
		ilm_log_err("%s: _parse_lock_mode fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ilm_log_dbg("%s: found: mode=%d", __func__, *mode);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_async_get_result_lvb - Asynchronously retreive the result for a
 * previous asynchronous read lvb request.
 *
 * @handle:     NVMe request handle for the previously sent NVMe cmd.
 * @mode:       Returned lock mode (unlock, shareable, exclusive).
 *              Referenced value set to -1 on error.
 * @lvb_size:   Lock value block size.
 * @result:     Returned result (0 or -ve value) for the previously sent
 *              NVMe command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_get_result_lvb(uint64_t handle, char *lvb, int lvb_size,
                                  int *result)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = (struct idm_nvme_request *)handle;
	int ret;

	//TODO: The -ve check below should go away cuz lvb_size should be of unsigned type.
	//However, this requires an IDM API parameter type change.
	if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
		return -EINVAL;

	ret = nvme_idm_async_data_rcv(request_idm, result);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_data_rcv fail %d",
		            __func__, ret);
		goto EXIT_FAIL;
	}

	if (*result < 0) {
		ilm_log_err("%s: previous async cmd fail: result=%d",
		            __func__, *result);
		goto EXIT_FAIL;
	}

	ret = _parse_lvb(request_idm, lvb, lvb_size);
	if (ret < 0) {
		ilm_log_err("%s: _parse_lvb fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ilm_log_array_dbg(" found: lvb", lvb, lvb_size);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_async_lock - Asynchronously acquire an IDM on a specified NVMe
 * drive.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_lock(char *lock_id, int mode, char *host_id,
                        char *drive, uint64_t timeout, uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock(lock_id, mode, host_id, drive, timeout, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_write fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_async_lock_break - Asynchronously break an IDM lock if before other
 * hosts have acquired this IDM.  This function is to allow a host_id to take
 * over the ownership if other hosts of the IDM is timeout, or the countdown
 * value is -1UL.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_lock_break(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout, uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock_break(lock_id, mode, host_id, drive, timeout,
	                       &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock_break fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_write fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_async_lock_convert - Asynchronously convert the lock mode for
 * an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_lock_convert(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t timeout,
                                uint64_t *handle)
{
	return nvme_idm_async_lock_refresh(lock_id, mode, host_id,
	                                   drive, timeout, handle);
}

/**
 * nvme_idm_async_lock_destroy - Asynchronously destroy an IDM and release
 * all associated resource.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_lock_destroy(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock_destroy(lock_id, mode, host_id, drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock_destroy fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_write fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_async_lock_refresh - Asynchronously refreshes the host's
 * membership for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_lock_refresh(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t timeout,
                                uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock_refresh(lock_id, mode, host_id, drive, timeout,
	                         &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock_refresh fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_write fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_async_lock_renew - Asynchronously renew host's membership for
 * an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_lock_renew(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout, uint64_t *handle)
{
	return nvme_idm_async_lock_refresh(lock_id, mode, host_id,
	                                   drive, timeout, handle);
}

/**
 * nvme_idm_async_read_lock_count - Asynchrnously read the lock count for
 * an IDM.
 * This command only issues the request.  It does NOT retrieve the data.
 * The desired data is returned during a separate call to
 * nvme_idm_async_get_result_lock_count().
 *
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_read_lock_count(char *lock_id, char *host_id, char *drive,
                                   uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_read_lock_count(ASYNC_ON, lock_id, host_id, drive,
					&request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_lock_count fail %d",
		            __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_async_read_lock_mode - Asynchronously read back an IDM's
 * current mode.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_read_lock_mode(char *lock_id, char *drive, uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_read_lock_mode(ASYNC_ON, lock_id, drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_lock_mode fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_async_read_lvb - Asynchronously read the lock value block(lvb),
 * which is associated to an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int nvme_idm_async_read_lvb(char *lock_id, char *host_id, char *drive,
                            uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_read_lvb(ASYNC_ON, lock_id, host_id, drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_lvb fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_async_unlock - Asynchronously release an IDM on a specified drive.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @lvb:        Lock value block pointer.
 * @lvb_size:   Lock value block size.
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_async_unlock(char *lock_id, int mode, char *host_id,
                          char *lvb, int lvb_size, char *drive,
                          uint64_t *handle)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_unlock(lock_id, mode, host_id, lvb, lvb_size, drive,
	                   &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_unlock fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_async_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_async_write fail %d",
		            __func__, ret);
		goto EXIT_FAIL;
	}

	*handle = (uint64_t)request_idm;
	goto EXIT;
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
EXIT:
	return ret;
}

/**
 * nvme_idm_environ_init - Convenience functions for running IDM-specific
 * code, for NVMe, required at seagate_ilm service startup.
 *
 * Currently, just used for initializing the NVMe's IDM async threadpool.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_environ_init(void)
{
	return ani_init();
}

/**
 * nvme_idm_environ_destroy - Convenience functions for running IDM-specific,
 * code, for NVMe, required at seagate_ilm service shutdown.
 *
 * Currently, just used for destroying the NVMe's IDM async threadpool.
 */
void nvme_idm_environ_destroy(void)
{
	ani_destroy();
}

/**
 * nvme_idm_get_fd - Retrieve the Linux device file descriptor from the
 * specified request handle.
 *
 * @handle:     Returned NVMe request handle.
 */
int nvme_idm_get_fd(uint64_t handle)
{
	struct idm_nvme_request *request_idm = (struct idm_nvme_request *)handle;

	return request_idm->fd_nvme;
}

/**
 * nvme_idm_read_version - Unfortunately, this function name is completely
 * misleading relative to it's current behavior.  This is entirely due to the
 * legacy behavior of the corresponding scsi cmd, which is, essentially, using
 * the "read version" function to detect a feature flag in the propeller
 * drive firmware.
 *
 * That "flag" is a bit set in the propeller firmware, which is supposed to
 * identify it as propeller-capable firmware.  The SCSI drive firmware
 * currently just sets a single bit if the firmware is for propeller.
 * The SCSI software then reads that bit and then (for unknown
 * reasons) shifts it up 8 bits to get a 0x100 value, and then
 * assigns it to the "version" return value.
 *
 * As a result, the expected behavior for this function is that it returns
 * 0x100 in "version" if propeller firmware is present.  Anythng else, is
 * considered NOT propeller capable.
 *
 * For NVMe, the drive firmware is actually using a real version number.
 * So, here, as long as the read version isn't less then a minimum IDM
 * SPEC VERSION value, the returned version value is set to the
 * 0x100 value to, again, emulate what the SCSI software is currently doing
 * when it detects valid propeller drive firmware.
 *
 * @version:    Lock mode (unlock, shareable, exclusive).
 * @drive:      Drive path name.
 *
 * TODO: This needs work. Done to match SCSI behavior:
 * Always returns 0, regardless of errors
 */
int nvme_idm_read_version(int *version, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct nvme_id_ctrl id_ctrl;
	int ver;
	int ret;

	ret = nvme_admin_identify(drive, &id_ctrl);
	if (ret < 0){
		*version = 0;
		goto EXIT;
	}

	ver = (int)id_ctrl.vs[1023];
	ilm_log_dbg("%s: found idm version %d", __func__, ver);

	if (ver < MIN_IDM_VERSION){
		ilm_log_err("%s: invalid idm version %d", __func__, ver);
		*version = 0;
		goto EXIT;
	}

	*version = 0x100;
EXIT:
	return 0;
}

/**
 * nvme_idm_sync_lock - Synchronously acquire an IDM on a specified NVMe drive.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_lock(char *lock_id, int mode, char *host_id,
                       char *drive, uint64_t timeout)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock(lock_id, mode, host_id, drive, timeout, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock fail %d", __func__, ret);
		goto EXIT;
	}

	ret = nvme_idm_sync_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_write fail %d", __func__, ret);
	}

EXIT:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_lock_break - Synchronously break an IDM lock if before other
 * hosts have acquired this IDM.  This function is to allow a host_id to take
 * over the ownership if other hosts of the IDM is timeout, or the countdown
 * value is -1UL.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_lock_break(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock_break(lock_id, mode, host_id, drive, timeout,
	                       &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock_break fail %d", __func__, ret);
		goto EXIT;
	}

	ret = nvme_idm_sync_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_write fail %d", __func__, ret);
	}

EXIT:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_lock_convert - Synchronously convert the lock mode for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_lock_convert(char *lock_id, int mode, char *host_id,
                               char *drive, uint64_t timeout)
{
	return nvme_idm_sync_lock_refresh(lock_id, mode, host_id,
	                                  drive, timeout);
}

/**
 * nvme_idm_sync_lock_destroy - Synchronously destroy an IDM and release all
 * associated resources.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_lock_destroy(char *lock_id, int mode, char *host_id,
                               char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock_destroy(lock_id, mode, host_id, drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock_destroy fail %d", __func__, ret);
		goto EXIT;
	}

	ret = nvme_idm_sync_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_write fail %d", __func__, ret);
	}

EXIT:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_lock_refresh - Synchronously refreshes the host's membership
 * for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_lock_refresh(char *lock_id, int mode, char *host_id,
                                  char *drive, uint64_t timeout)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_lock_refresh(lock_id, mode, host_id, drive, timeout,
	                         &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_lock_refresh fail %d", __func__, ret);
		goto EXIT;
	}

	ret = nvme_idm_sync_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_write fail %d", __func__, ret);
	}

EXIT:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_lock_renew - Synchronously renew host's membership for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int nvme_idm_sync_lock_renew(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout)
{
	return nvme_idm_sync_lock_refresh(lock_id, mode, host_id,
	                                  drive, timeout);
}

/**
 * nvme_idm_sync_read_host_state - Read back the host's state for a
 * specific IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @host_state: Returned host state.
 *              Referenced value set to -1 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_read_host_state(char *lock_id, char *host_id,
                                  int *host_state, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	// Initialize the output
//TODO: Does ALWAYS setting to -1 on failure make sense?
//          Refer to scsi-side if removed.
//          Was being set to -1 in a couple locations.
	*host_state = -1;    //TODO: hardcoded state. add an "error" state to the enum?

	ret = _init_read_host_state(lock_id, host_id, drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_host_state fail %d",
		            __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_sync_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = _parse_host_state(request_idm, host_state);
	if (ret < 0) {
		ilm_log_err("%s: _parse_host_state fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ilm_log_dbg("%s: found: host_state=0x%X (%d)",
	            __func__, *host_state, *host_state);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_read_lock_count - Synchrnously read the lock count for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @count:      Returned lock count.
 * @self:       Returned self count.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_read_lock_count(char *lock_id, char *host_id, int *count,
                                  int *self, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	// Initialize the output
	*count = 0;
	*self  = 0;

	ret = _init_read_lock_count(ASYNC_OFF, lock_id, host_id, drive,
	                            &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_lock_count fail %d",
		            __func__, ret);
		goto EXIT_FAIL;
	}

	if (!request_idm) {	//Occurs when mutex_num=0
		return SUCCESS;
	}

	ret = nvme_idm_sync_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = _parse_lock_count(request_idm, count, self);
	if (ret < 0) {
		ilm_log_err("%s: _parse_lock_count fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ilm_log_dbg("%s: found: lock_count=%d, self_count=%d",
	            __func__, *count, *self);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_read_lock_mode - Synchronously read back an IDM's
 * current mode.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Returned lock mode (unlock, shareable, exclusive).
 *              Referenced value set to -1 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_read_lock_mode(char *lock_id, int *mode, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	// Initialize the output
	*mode = -1;    //TODO: hardcoded state. add an "error" state to the enum?

	ret = _init_read_lock_mode(ASYNC_OFF, lock_id, drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_lock_mode fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	if (!request_idm) {         //Occurs when mutex_num=0
		*mode = IDM_MODE_UNLOCK;
		return SUCCESS;
	}

	ret = nvme_idm_sync_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = _parse_lock_mode(request_idm, mode);
	if (ret < 0) {
		ilm_log_err("%s: _parse_lock_mode fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ilm_log_dbg("%s: found: mode=%d", __func__, *mode);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_read_lvb - Synchronously read the lock value block(lvb)
 * which is associated to an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @lvb:        Lock value block pointer.
 *              Pointer's memory cleared on error.
 * @lvb_size:   Lock value block size.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int nvme_idm_sync_read_lvb(char *lock_id, char *host_id, char *lvb,
                           int lvb_size, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	//TODO: The -ve check below should go away cuz lvb_size should be of unsigned type.
	//However, this requires an IDM API parameter type change.
	if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
		return -EINVAL;

	ret = _init_read_lvb(ASYNC_OFF, lock_id, host_id, drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_lvb fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	if (!request_idm) {	//Occurs when mutex_num=0
		memset(lvb, 0x0, lvb_size);
		return SUCCESS;
	}

	ret = nvme_idm_sync_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = _parse_lvb(request_idm, lvb, lvb_size);
	if (ret < 0) {
		ilm_log_err("%s: _parse_lvb fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ilm_log_array_dbg(" found: lvb", lvb, lvb_size);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_read_mutex_group - Synchronously read back mutex group info
 * for all IDM in the drives
 *
 * @drive:      Drive path name.
 * @info_ptr:   Returned pointer for info list.
 *              Referenced pointer set to NULL on error.
 * @info_num:   Returned pointer for info num.
 *              Referenced value set to 0 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_read_mutex_group(char *drive, struct idm_info **info_ptr,
                                   int *info_num)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	// Initialize the output
	*info_ptr = NULL;
	*info_num = 0;

	ret = _init_read_mutex_group(drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_mutex_group fail %d",
		            __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_sync_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = _parse_mutex_group(request_idm, info_ptr, info_num);
	if (ret < 0) {
		ilm_log_err("%s: _parse_mutex_group fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	#ifdef DBG__DUMP_STRUCTS
	dumpIdmInfoStruct(*info_ptr);
	#endif

	ilm_log_dbg("%s: found: info_num=%d", __func__, *info_num);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_read_mutex_num - Synchronously retrieves the number of mutexes
 * present on the drive.
 *
 * @drive:       Drive path name.
 * @mutex_num:   Returned number of mutexes present on the drive.
//TODO: Does ALWAYS setting to 0 on failure make sense?
 *               Set to 0 on failure.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_read_mutex_num(char *drive, unsigned int *mutex_num)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

//TODO: The SCSI-side code tends to set this to 0 on certain failures.
//          Does ALWAYS setting to 0 on failure HERE make sense (this is a bit different from scsi-side?
	*mutex_num = 0;

	ret = _init_read_mutex_num(drive, &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_read_mutex_num fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	ret = nvme_idm_sync_read(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_read fail %d", __func__, ret);
		goto EXIT_FAIL;
	}

	_parse_mutex_num(request_idm, mutex_num);

	ilm_log_dbg("%s: found: mutex_num=%d", __func__, *mutex_num);
EXIT_FAIL:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * nvme_idm_sync_unlock - Synchronously release an IDM on a specified drive.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @lvb:        Lock value block pointer.
 * @lvb_size:   Lock value block size.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_sync_unlock(char *lock_id, int mode, char *host_id,
                         char *lvb, int lvb_size, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_nvme_request *request_idm = NULL;
	int ret;

	ret = _init_unlock(lock_id, mode, host_id, lvb, lvb_size, drive,
	                   &request_idm);
	if (ret < 0) {
		ilm_log_err("%s: _init_unlock fail %d", __func__, ret);
		goto EXIT;
	}

	ret = nvme_idm_sync_write(request_idm);
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_sync_write fail %d", __func__, ret);
	}

EXIT:
	_memory_free_idm_request(request_idm);
	return ret;
}

/**
 * _init_lock - Convenience function containing common code for the
 * IDM lock action.
 *
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @timeout:     Timeout for membership (unit: millisecond).
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock(char *lock_id, int mode, char *host_id, char *drive,
               uint64_t timeout, struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int ret;

	ret = _validate_input_write(lock_id, mode, host_id, drive);
	if (ret < 0) {
		ilm_log_err("%s: _validate_input_write fail %d",
		            __func__, ret);
		return ret;
	}

	ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
	if (ret < 0) {
		ilm_log_err("%s: _memory_init_idm_request fail %d",
		            __func__, ret);
		return ret;
	}

	ret = nvme_idm_write_init(lock_id, mode, host_id, drive, timeout,
	                          (*request_idm));
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
		return ret;
	}

	//API-specific code
	(*request_idm)->opcode_idm   = IDM_OPCODE_TRYLOCK;
	(*request_idm)->res_ver_type = (char)IDM_RES_VER_NO_UPDATE_NO_VALID;

	return ret;
}

/**
 * _init_lock_break - Convenience function containing common code for the
 * IDM lock break action.
 *
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @timeout:     Timeout for membership (unit: millisecond).
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock_break(char *lock_id, int mode, char *host_id, char *drive,
                     uint64_t timeout, struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int ret;

	ret = _validate_input_write(lock_id, mode, host_id, drive);
	if (ret < 0) {
		ilm_log_err("%s: _validate_input_write fail %d",
		            __func__, ret);
		return ret;
	}

	ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
	if (ret < 0) {
		ilm_log_err("%s: _memory_init_idm_request fail %d",
		            __func__, ret);
		return ret;
	}

	ret = nvme_idm_write_init(lock_id, mode, host_id, drive, timeout,
	                          (*request_idm));
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
		return ret;
	}

	//API-specific code
	(*request_idm)->opcode_idm   = IDM_OPCODE_BREAK;
	(*request_idm)->res_ver_type = (char)IDM_RES_VER_NO_UPDATE_NO_VALID;

	return ret;
}

/**
 * _init_lock_destroy - Convenience function containing common code for the
 * IDM lock destroy action.
 *
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock_destroy(char *lock_id, int mode, char *host_id, char *drive,
                       struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int ret;

	ret = _validate_input_write(lock_id, mode, host_id, drive);
	if (ret < 0) {
		ilm_log_err("%s: _validate_input_write fail %d",
		            __func__, ret);
		return ret;
	}

	ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
	if (ret < 0) {
		ilm_log_err("%s: _memory_init_idm_request fail %d",
		            __func__, ret);
		return ret;
	}

	ret = nvme_idm_write_init(lock_id, mode, host_id, drive, 0,
	                          (*request_idm));
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
		return ret;
	}

	//API-specific code
	(*request_idm)->opcode_idm   = IDM_OPCODE_DESTROY;
	(*request_idm)->res_ver_type = (char)IDM_RES_VER_NO_UPDATE_NO_VALID;

	return ret;
}

/**
 * _init_lock_refresh - Convenience function containing common code for the
 * IDM lock refresh action.
 *
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @timeout:     Timeout for membership (unit: millisecond).
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock_refresh(char *lock_id, int mode, char *host_id, char *drive,
                       uint64_t timeout, struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int ret;

	ret = _validate_input_write(lock_id, mode, host_id, drive);
	if (ret < 0) {
		ilm_log_err("%s: _validate_input_write fail %d",
		            __func__, ret);
		return ret;
	}

	ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
	if (ret < 0) {
		ilm_log_err("%s: _memory_init_idm_request fail %d",
		            __func__, ret);
		return ret;
	}

	ret = nvme_idm_write_init(lock_id, mode, host_id, drive, timeout,
	                          (*request_idm));
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
		return ret;
	}

	//API-specific code
	(*request_idm)->opcode_idm   = IDM_OPCODE_REFRESH;
	(*request_idm)->res_ver_type = (char)IDM_RES_VER_NO_UPDATE_NO_VALID;

	return ret;
}

/**
 * _init_read_host_state - Convenience function containing common code for
 * the retrieval of the IDM host state from the drive.
 *
 * @lock_id:     Lock ID (64 bytes).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_host_state(char *lock_id, char *host_id, char *drive,
                          struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	unsigned int mutex_num = 0;
	int ret;

	ret = _validate_input_common(lock_id, host_id, drive);
	if (ret < 0)
		return ret;

	ret = nvme_idm_sync_read_mutex_num(drive, &mutex_num);
	if (ret < 0)
		return -ENOENT;
	else if (!mutex_num)
		return SUCCESS;

	ret = _memory_init_idm_request(request_idm, mutex_num);
	if (ret < 0)
		return ret;

	nvme_idm_read_init(drive, *request_idm);

	//API-specific code
	(*request_idm)->group_idm = IDM_GROUP_DEFAULT;
	memcpy((*request_idm)->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);
	memcpy((*request_idm)->host_id, host_id, IDM_HOST_ID_LEN_BYTES);

	return ret;
}

/**
 * _init_read_lock_count - Convenience function containing common code for the
 * retrieval of the IDM lock count from the drive.
 *
 * @async_on:    Boolean flag indicating async(1) or sync(0) wrapping function.
 *               Async and sync usage have different control flows.
 * @lock_id:     Lock ID (64 bytes).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_lock_count(int async_on, char *lock_id, char *host_id,
                          char *drive, struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	unsigned int mutex_num = 0;
	int ret;

	ret = _validate_input_common(lock_id, host_id, drive);
	if (ret < 0)
		return ret;

	ret = nvme_idm_sync_read_mutex_num(drive, &mutex_num);
	if (ret < 0)
		return -ENOENT;

	if (async_on) {
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
		if (!mutex_num)
			mutex_num = 1;
	}
	else {
		if (!mutex_num) {
			request_idm = NULL;
			return SUCCESS;
		}
	}

	ret = _memory_init_idm_request(request_idm, mutex_num);
	if (ret < 0)
		return ret;

	nvme_idm_read_init(drive, *request_idm);

	//API-specific code
	(*request_idm)->group_idm = IDM_GROUP_DEFAULT;
	memcpy((*request_idm)->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);
	memcpy((*request_idm)->host_id, host_id, IDM_HOST_ID_LEN_BYTES);

	return ret;
}

/**
 * _init_read_lock_mode - Convenience function containing common code for the
 * retrieval of the IDM lock mode from the drive.
 *
 * @async_on:    Boolean flag indicating async(1) or sync(0) wrapping function.
 *               Async and sync usage have different control flows.
 * @lock_id:     Lock ID (64 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_lock_mode(int async_on, char *lock_id, char *drive,
                         struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	unsigned int mutex_num = 0;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !drive)
		return -EINVAL;

	ret = nvme_idm_sync_read_mutex_num(drive, &mutex_num);
	if (ret < 0)
		return -ENOENT;

	if (async_on) {
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
		if (!mutex_num)
		mutex_num = 1;
	}
	else {
		if (!mutex_num) {
		request_idm = NULL;
		return SUCCESS;
		}
	}

	ret = _memory_init_idm_request(request_idm, mutex_num);
	if (ret < 0)
		return ret;

	nvme_idm_read_init(drive, *request_idm);

	//API-specific code
	(*request_idm)->group_idm = IDM_GROUP_DEFAULT;
	memcpy((*request_idm)->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);

	return ret;
}

/**
 * _init_read_lvb - Convenience function containing common code for the
 * retrieval of the lock value block(lvb) from the drive.
 *
 * @async_on:    Boolean flag indicating async(1) or sync(0) wrapping function.
 *               Async and sync usage have different control flows.
 * @lock_id:     Lock ID (64 bytes).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_lvb(int async_on, char *lock_id, char *host_id, char *drive,
                   struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	unsigned int mutex_num = 0;
	int ret;

	ret = _validate_input_common(lock_id, host_id, drive);
	if (ret < 0)
		return ret;

	ret = nvme_idm_sync_read_mutex_num(drive, &mutex_num);
	if (ret < 0)
		return -ENOENT;

	if (async_on) {
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
		if (!mutex_num)
		mutex_num = 1;
	}
	else {
		if (!mutex_num) {
		request_idm = NULL;
		return SUCCESS;
		}
	}

	ret = _memory_init_idm_request(request_idm, mutex_num);
	if (ret < 0)
		return ret;

	nvme_idm_read_init(drive, *request_idm);

	//API-specific code
	(*request_idm)->group_idm = IDM_GROUP_DEFAULT;
	memcpy((*request_idm)->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);
	memcpy((*request_idm)->host_id, host_id, IDM_HOST_ID_LEN_BYTES);

	return ret;
}

/**
 * _init_read_mutex_group - Convenience function containing common code for
 * the retrieval of IDM group information from the drive.
 *
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_mutex_group(char *drive, struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	unsigned int mutex_num = 0;
	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!drive)
		return -EINVAL;

	ret = nvme_idm_sync_read_mutex_num(drive, &mutex_num);
	if (ret < 0)
		return -ENOENT;
	else if (!mutex_num)
		return SUCCESS;

	ret = _memory_init_idm_request(request_idm, mutex_num);
	if (ret < 0)
		return ret;

	nvme_idm_read_init(drive, *request_idm);

	//API-specific code
	(*request_idm)->group_idm = IDM_GROUP_DEFAULT;

	return ret;
}

/**
 * _init_read_mutex_num - Convenience function containing common code for the
 * retrieval of the number of lock mutexes available on the device.
 *
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_mutex_num(char *drive, struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int ret;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
	if (ret < 0) {
		ilm_log_err("%s: _memory_init_idm_request fail %d",
		            __func__, ret);
		return ret;
	}

	nvme_idm_read_init(drive, *request_idm);

	//API-specific code
	(*request_idm)->group_idm = IDM_GROUP_INQUIRY;

	return ret;
}

/**
 * _init_unlock - Convenience function containing common code for the
 * IDM unlock action.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @lvb:        Lock value block pointer.
 * @lvb_size:   Lock value block size.
 * @drive:      Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for
 *               the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_unlock(char *lock_id, int mode, char *host_id, char *lvb,
                 int lvb_size, char *drive,
		 struct idm_nvme_request **request_idm)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int ret;

	//TODO: The -ve check here should go away cuz lvb_size should be of unsigned type.
	//However, this requires an IDM API parameter type change.
	if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
		return -EINVAL;

	ret = _validate_input_write(lock_id, mode, host_id, drive);
	if (ret < 0) {
		ilm_log_err("%s: _validate_input_write fail %d",
		            __func__, ret);
		return ret;
	}

	ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
	if (ret < 0) {
		ilm_log_err("%s: _memory_init_idm_request fail %d",
		            __func__, ret);
		return ret;
	}

	//TODO: Why 0 timeout here (ported as-is from scsi-side)?
	ret = nvme_idm_write_init(lock_id, mode, host_id, drive, 0,
	                          (*request_idm));
	if (ret < 0) {
		ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
		return ret;
	}

	//API-specific code
	(*request_idm)->opcode_idm   = IDM_OPCODE_UNLOCK;
	(*request_idm)->res_ver_type = (char)IDM_RES_VER_UPDATE_NO_VALID;
	memcpy((*request_idm)->lvb, lvb, lvb_size);

	return ret;
}

/**
 * _memory_free_idm_request - Convenience function for freeing memory for all
 * the data structures used during the NVMe command sequence.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
void _memory_free_idm_request(struct idm_nvme_request *request_idm) {

	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	if (request_idm) {
		if (request_idm->arg_async_nvme) {
			free(request_idm->arg_async_nvme);
			request_idm->arg_async_nvme = NULL;
		}
		if (request_idm->cmd_nvme_passthru) {
			free(request_idm->cmd_nvme_passthru);
			request_idm->cmd_nvme_passthru = NULL;
		}
		if (request_idm->data_idm) {
			free(request_idm->data_idm);
			request_idm->data_idm = NULL;
		}

		free(request_idm);
		request_idm = NULL;
	}
}

/**
 * _memory_init_idm_request - Convenience function for allocating memory for
 * all the data structures used during the NVMe command sequence.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *               Note: **request_idm is due to malloc() needing access to
 *               the original pointer.
 * @data_num:    Number of data payload instances that need memory allocation.
 *               Also corresponds to the number of mutexes on the drive.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _memory_init_idm_request(struct idm_nvme_request **request_idm,
                             unsigned int data_num)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int data_len;

	*request_idm = malloc(sizeof(struct idm_nvme_request));
	if (!request_idm) {
		ilm_log_err("%s: request memory allocate fail", __func__);
		return -ENOMEM;
	}
	memset((*request_idm), 0, sizeof(**request_idm));

	data_len                 = sizeof(struct idm_data) * data_num;
	(*request_idm)->data_idm = malloc(data_len);
	if (!(*request_idm)->data_idm) {
		_memory_free_idm_request((*request_idm));
		ilm_log_err("%s: request data memory allocate fail", __func__);
		return -ENOMEM;
	}
	memset((*request_idm)->data_idm, 0, data_len);

	//Cache memory-specifc info.
	(*request_idm)->data_len = data_len;
	(*request_idm)->data_num = data_num;

	return SUCCESS;
}

/**
 * _parse_host_state - Convenience function for parsing the host state out of
 * the returned data payload.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 * @host_state:  Returned host state.
 *               Referenced value set to -1 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _parse_host_state(struct idm_nvme_request *request_idm, int *host_state)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_data    *data_idm;
	char               bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
	char               bswap_host_id[IDM_HOST_ID_LEN_BYTES];
	int                i;
	unsigned int       mutex_num = request_idm->data_num;

	bswap_char_arr(bswap_lock_id, request_idm->lock_id,
	               IDM_LOCK_ID_LEN_BYTES);
	bswap_char_arr(bswap_host_id, request_idm->host_id,
	               IDM_HOST_ID_LEN_BYTES);

	data_idm = request_idm->data_idm;
	for (i = 0; i < mutex_num; i++) {
		//TODO: Layout improvement over adhereing to 80 char limit??
		ilm_log_array_dbg("resource_id",  data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
		ilm_log_array_dbg("lock_id",      bswap_lock_id,           IDM_LOCK_ID_LEN_BYTES);
		ilm_log_array_dbg("data host_id", data_idm[i].host_id,     IDM_HOST_ID_LEN_BYTES);
		ilm_log_array_dbg("host_id",      bswap_host_id,           IDM_HOST_ID_LEN_BYTES);

		/* Skip for other locks */
		if (memcmp(data_idm[i].resource_id, bswap_lock_id,
		           IDM_LOCK_ID_LEN_BYTES))
			continue;

		if (memcmp(data_idm[i].host_id, bswap_host_id,
		           IDM_HOST_ID_LEN_BYTES))
			continue;

		*host_state = __bswap_64(data_idm[i].state);
		break;
	}

	return SUCCESS;
}

/**
 * _parse_lock_count - Convenience function for parsing the lock count out of
 * the returned data payload.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 * @count:       Returned lock count.
 * @self:        Returned self count.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _parse_lock_count(struct idm_nvme_request *request_idm, int *count,
                      int *self)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_data    *data_idm;
	char               bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
	char               bswap_host_id[IDM_HOST_ID_LEN_BYTES];
	int                i;
	uint64_t           state, locked;
	unsigned int       mutex_num = request_idm->data_num;
	int                ret       = FAILURE;

	bswap_char_arr(bswap_lock_id, request_idm->lock_id,
	               IDM_LOCK_ID_LEN_BYTES);
	bswap_char_arr(bswap_host_id, request_idm->host_id,
	               IDM_HOST_ID_LEN_BYTES);

	data_idm = request_idm->data_idm;
	for (i = 0; i < mutex_num; i++) {

		state  = __bswap_64(data_idm[i].state);
		locked = (state == IDM_STATE_LOCKED) ||
		         (state == IDM_STATE_MULTIPLE_LOCKED);

		if (!locked)
			continue;

		ilm_log_array_dbg("resource_id", data_idm[i].resource_id,
		                  IDM_LOCK_ID_LEN_BYTES);
		ilm_log_array_dbg("lock_id", bswap_lock_id,
		                  IDM_LOCK_ID_LEN_BYTES);
		ilm_log_array_dbg("data host_id", data_idm[i].host_id,
		                  IDM_HOST_ID_LEN_BYTES);
		ilm_log_array_dbg("host_id", bswap_host_id,
		                  IDM_HOST_ID_LEN_BYTES);

		/* Skip for other locks */
		if (memcmp(data_idm[i].resource_id, bswap_lock_id,
		           IDM_LOCK_ID_LEN_BYTES))
			continue;

		if (!memcmp(data_idm[i].host_id, bswap_host_id,
		           IDM_HOST_ID_LEN_BYTES)) {
			if (*self) {
				/* Must be wrong if self has been accounted */
				ilm_log_err("%s: account self %d > 1",
					__func__, *self);
				goto EXIT;
			}
			*self = 1;
		}
		else {
			*count += 1;
		}
	}

	ret = SUCCESS;
EXIT:
	return ret;
}

/**
 * _parse_lock_mode - Convenience function for parsing the lock mode out of the
 * returned data payload.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 * @mode:        Returned lock mode (unlock, shareable, exclusive).
 *               Referenced value set to -1 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _parse_lock_mode(struct idm_nvme_request *request_idm, int *mode)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_data    *data_idm;
	char               bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
	int                i;
	uint64_t           state, class;
	unsigned int       mutex_num = request_idm->data_num;
	int                ret       = FAILURE;

	bswap_char_arr(bswap_lock_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);

	data_idm = request_idm->data_idm;
	for (i = 0; i < mutex_num; i++) {

		ilm_log_array_dbg("resource_id", data_idm[i].resource_id,
		                  IDM_LOCK_ID_LEN_BYTES);
		ilm_log_array_dbg("lock_id(bswap)", bswap_lock_id,
		                  IDM_LOCK_ID_LEN_BYTES);

		/* Skip for other locks */
		if (memcmp(data_idm[i].resource_id, bswap_lock_id,
		           IDM_LOCK_ID_LEN_BYTES))
			continue;

		state = __bswap_64(data_idm[i].state);
		class = __bswap_64(data_idm[i].class);

		ilm_log_dbg("%s: state=%lx class=%lx", __func__, state, class);

		if (state == IDM_STATE_UNINIT ||
		    state == IDM_STATE_UNLOCKED ||
		    state == IDM_STATE_TIMEOUT) {
			*mode = IDM_MODE_UNLOCK;
		} else if (class == IDM_CLASS_EXCLUSIVE) {
			*mode = IDM_MODE_EXCLUSIVE;
		} else if (class == IDM_CLASS_SHARED_PROTECTED_READ) {
			*mode = IDM_MODE_SHAREABLE;
		} else if (class == IDM_CLASS_PROTECTED_WRITE) {
			ilm_log_err("%s: PROTECTED_WRITE is not unsupported", __func__);
			ret = -EFAULT;
			goto EXIT;
		}

		ilm_log_dbg("%s: mode=%d", __func__, *mode);

		if (*mode == IDM_MODE_EXCLUSIVE || *mode == IDM_MODE_SHAREABLE)
			break;
	}

	ret = SUCCESS;
	//If the mutex is not found in drive fimware,
	// simply return success and mode is unlocked.
	if (*mode == -1)
		*mode = IDM_MODE_UNLOCK;
EXIT:
	return ret;
}

/**
 * _parse_lvb - Convenience function for parsing the lock value block (lvb)
 * from the returned data payload.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 * @lvb:         Lock value block pointer.
 *               Pointer's memory cleared on error.
 * @lvb_size:    Lock value block size.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _parse_lvb(struct idm_nvme_request *request_idm, char *lvb, int lvb_size)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_data    *data_idm;
	char               bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
	char               bswap_host_id[IDM_HOST_ID_LEN_BYTES];
	int                i;
	unsigned int       mutex_num = request_idm->data_num;
	int                ret;

	bswap_char_arr(bswap_lock_id, request_idm->lock_id,
	               IDM_LOCK_ID_LEN_BYTES);
	bswap_char_arr(bswap_host_id, request_idm->host_id,
	               IDM_HOST_ID_LEN_BYTES);

	ret = -ENOENT;
	data_idm = request_idm->data_idm;
	for (i = 0; i < mutex_num; i++) {
		/* Skip for other locks */
		if (memcmp(data_idm[i].resource_id, bswap_lock_id,
		           IDM_LOCK_ID_LEN_BYTES))
			continue;

		if (memcmp(data_idm[i].host_id, bswap_host_id,
		           IDM_HOST_ID_LEN_BYTES))
			continue;

		bswap_char_arr(lvb, data_idm[i].resource_ver, lvb_size);
		ret = SUCCESS;
		break;
	}

	return ret;
}

/**
 * _parse_mutex_group - Convenience function for parsing struct idm_info data
 * out of the returned data payload.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 * @info_ptr:    Returned pointer for info list.
 *               Referenced pointer set to NULL on error.
 * @info_num:    Returned pointer for info num.
 *               Referenced value set to 0 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _parse_mutex_group(struct idm_nvme_request *request_idm,
                       struct idm_info **info_ptr, int *info_num)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct idm_data    *data_idm;
	unsigned int       mutex_num = request_idm->data_num;
	int                i, ret    = FAILURE;
	uint64_t           state, class;
	struct idm_info    *info_list, *info;

	info_list = malloc(sizeof(struct idm_info) * mutex_num);
	if (!info_list) {
		ret = -ENOMEM;
		goto EXIT;
	}

	//Find info
	data_idm = request_idm->data_idm;
	for (i = 0; i < mutex_num; i++) {

		state = __bswap_64(data_idm[i].state);
		if (state == IDM_STATE_DEAD)
		break;

		info = info_list + i;

		/* Copy host ID */
		bswap_char_arr(info->id, data_idm[i].resource_id,
		               IDM_LOCK_ID_LEN_BYTES);
		bswap_char_arr(info->host_id, data_idm[i].host_id,
		               IDM_HOST_ID_LEN_BYTES);

		class = __bswap_64(data_idm[i].class);

		switch (class) {
		case IDM_CLASS_EXCLUSIVE:
			info->mode = IDM_MODE_EXCLUSIVE;
			break;
		case IDM_CLASS_SHARED_PROTECTED_READ:
			info->mode = IDM_MODE_SHAREABLE;
			break;
		default:
			ilm_log_err("%s: IDM class is not unsupported %ld",
			            __func__, class);
			ret = -EFAULT;
			goto EXIT;
		}

		switch (state) {
		case IDM_STATE_UNINIT:
			info->state = -1;   //TODO: hardcoded state. what is this? add to enum?
			break;
		case IDM_STATE_UNLOCKED: //intended fall-through
		case IDM_STATE_TIMEOUT:
			info->state = IDM_MODE_UNLOCK;
			break;
		default:
			info->state = 1;    //TODO: hardcoded state. what is this? add to enum?
		}

		info->last_renew_time = __bswap_64(data_idm[i].time_now);
	}

	*info_ptr = info_list;
	*info_num = i;
	ret = SUCCESS;

EXIT:
	if (ret != 0 && info_list)
		free(info_list);
	return ret;
}

/**
 * _parse_mutex_num - Convenience function for parsing the number of mutexes
 * out of the returned data payload.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the
 *                  requested IDM action.
 * @mutex_num:      Returned number of mutexes present on the drive.
 */
void _parse_mutex_num(struct idm_nvme_request *request_idm,
                      unsigned int *mutex_num)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	/*
	NOTE: Propeller firmware returns the mutex counts in a unique format.
	From the Propeller spec, the order of the counts are as follows:
		data[1:0], Number of Mutexes (Total)
		data[3:2], Number of Exclusive Mutexes
		data[5:4], Number of Protected Write Mutexes
		data[7:6], Number of Shared Mutexes
		data[9:8], Number of Exclusive LOCKED Mutexes
		data[11:10], Number of Exclusive UNLOCKED Mutexes
		data[13:12], Number of Exclusive MULTILOCK Mutexes (not a valid state)
		data[15:14], Number of Exclusive TIMEOUT Mutexes
		data[17:16], Number of Protected Write LOCKED Mutexes
		data[19:18], Number of Protected Write UNLOCKED Mutexes
		data[21:20], Number of Protected Write MULTILOCK Mutexes
		data[23:22], Number of Protected Write TIMEOUT Mutexes
		data[25:24], Number of Shared LOCKED Mutexes
		data[27:26], Number of Shared UNLOCKED Mutexes
		data[29:28], Number of Shared MULTILOCK Mutexes
		data[31:30], Number of Shared TIMEOUT Mutexes

	Each count is 16-bits and is read here as 2 unsigned char's.
	NOTE: the byte-order is REVERSED for each count.
	Currently, only the total mutex count is used.
	*/
	unsigned char  *data = (unsigned char *)request_idm->data_idm;

	*mutex_num = ((data[1]) & 0xff);
	*mutex_num |= ((data[0]) & 0xff) << 8;

	ilm_log_dbg("%s: data[0]=%u data[1]=%u mutex mutex_num=%u",
	            __func__, data[0], data[1], *mutex_num);
}

/**
 * _validate_input_common - Convenience function for validating the most common
 * IDM API input parameters.
 *
 * @lock_id:        Lock ID (64 bytes).
 * @host_id:        Host ID (32 bytes).
 * @drive:          Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _validate_input_common(char *lock_id, char *host_id, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	return SUCCESS;
}

/**
 * _validate_input_write - Convenience function for validating IDM API input
 * parameters used during an IDM write cmd.
 *
 * @lock_id:        Lock ID (64 bytes).
 * @mode:           Lock mode (unlock, shareable, exclusive).
 * @host_id:        Host ID (32 bytes).
 * @drive:          Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _validate_input_write(char *lock_id, int mode, char *host_id, char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int ret = FAILURE;

	ret = _validate_input_common(lock_id, host_id, drive);

	if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
		return -EINVAL;

	return ret;
}









////////////////////////////////////////////////////////////////////////////////
// DEBUG MAIN
////////////////////////////////////////////////////////////////////////////////
#if DBG__NVME_API_MAIN_ENABLE
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1"

#define ASYNC_SLEEP_TIME_SEC	1

//To compile:
//gcc idm_nvme_io.c idm_nvme_api.c -o idm_nvme_api
int main(int argc, char *argv[])
{
	printf("main - idm_nvme_api\n");

	char drive[PATH_MAX];
	int  ret = 0;

	if(argc >= 3){
		strcpy(drive, argv[2]);
	}
	else {
		strcpy(drive, DRIVE_DEFAULT_DEVICE);
	}

	ret = nvme_idm_environ_init();
	if (ret) goto INIT_FAIL_EXIT;

	//cli usage: idm_nvme_api lock
	if(argc >= 2){
		char            lock_id[IDM_LOCK_ID_LEN_BYTES] = "0000000000000000000000000000000000000000000000000000000000000000";
		int             mode                           = IDM_MODE_EXCLUSIVE;
		char            host_id[IDM_HOST_ID_LEN_BYTES] = "00000000000000000000000000000000";
		uint64_t        timeout                        = 10000;
		char            lvb[IDM_LVB_LEN_BYTES]         = "lvb";
		int             lvb_size                       = 5;
		uint64_t        handle;
		int             result=0;
		unsigned int    mutex_num;
		int             info_num;
		struct idm_info *info_list;
		int             host_state;
		int             count;
		int             self;
		int             version;

		if(strcmp(argv[1], "async_lock") == 0){
			ret = nvme_idm_async_lock(lock_id, mode, host_id, drive, timeout, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result(handle, &result);
				printf("'%s:' ret=%d, result=0x%X\n", argv[1], ret, result);
			}
		}
		else if(strcmp(argv[1], "async_lock_break") == 0){
			ret = nvme_idm_async_lock_break(lock_id, mode, host_id, drive, timeout, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result(handle, &result);
				printf("'%s:' ret=%d, result=0x%X\n", argv[1], ret, result);
			}
		}
		else if(strcmp(argv[1], "async_lock_convert") == 0){
			ret = nvme_idm_async_lock_convert(lock_id, mode, host_id, drive, timeout, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result(handle, &result);
				printf("'%s:' ret=%d, result=0x%X\n", argv[1], ret, result);
			}
		}
		else if(strcmp(argv[1], "async_lock_destroy") == 0){
			ret = nvme_idm_async_lock_destroy(lock_id, mode, host_id, drive, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result(handle, &result);
				printf("'%s:' ret=%d, result=0x%X\n", argv[1], ret, result);
			}
		}
		else if(strcmp(argv[1], "async_lock_refresh") == 0){
			ret = nvme_idm_async_lock_refresh(lock_id, mode, host_id, drive, timeout, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result(handle, &result);
				printf("'%s:' ret=%d, result=0x%X\n", argv[1], ret, result);
			}
		}
		else if(strcmp(argv[1], "async_lock_renew") == 0){
			ret = nvme_idm_async_lock_renew(lock_id, mode, host_id, drive, timeout, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result(handle, &result);
				printf("'%s:' ret=%d, result=0x%X\n", argv[1], ret, result);
			}
		}
		else if(strcmp(argv[1], "async_unlock") == 0){
			ret = nvme_idm_async_unlock(lock_id, mode, host_id, lvb, lvb_size, drive, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result(handle, &result);
				printf("'%s:' ret=%d, result=0x%X\n", argv[1], ret, result);
			}
		}
		else if(strcmp(argv[1], "async_read_count") == 0){
			ret = nvme_idm_async_read_lock_count(lock_id, host_id, drive, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result_lock_count(handle, &count, &self, &result);
				printf("'%s:' ret=%d, result=0x%X, count:%d, self:%d\n",
				       argv[1], ret, result, count, self);
			}
		}
		else if(strcmp(argv[1], "async_read_mode") == 0){
			ret = nvme_idm_async_read_lock_mode(lock_id, drive, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result_lock_mode(handle, &mode, &result);
				printf("'%s:' ret=%d, result=0x%X, mode:%d\n",
				       argv[1], ret, result, mode);
			}
		}
		else if(strcmp(argv[1], "async_read_lvb") == 0){
			ret = nvme_idm_async_read_lvb(lock_id, host_id, drive, &handle);
			if (ret){
				printf("%s: send cmd fail: %d\n", argv[1], ret);
				nvme_idm_async_free_result(handle);
			}
			else{
				printf("%s: sent cmd pass:\n", argv[1]);
				sleep(ASYNC_SLEEP_TIME_SEC);
				ret = nvme_idm_async_get_result_lvb(handle, lvb, lvb_size, &result);
				printf("'%s:' ret=%d, result=0x%X, lvb:%s, size:%d\n",
				       argv[1], ret, result, lvb, lvb_size);
			}
		}
		else if(strcmp(argv[1], "sync_lock") == 0){
			ret = nvme_idm_sync_lock(lock_id, mode, host_id, drive, timeout);
		}
		else if(strcmp(argv[1], "sync_lock_break") == 0){
			ret = nvme_idm_sync_lock_break(lock_id, mode, host_id, drive, timeout);
		}
		else if(strcmp(argv[1], "sync_lock_convert") == 0){
			ret = nvme_idm_sync_lock_convert(lock_id, mode, host_id, drive, timeout);
		}
		else if(strcmp(argv[1], "sync_lock_destroy") == 0){
			ret = nvme_idm_sync_lock_destroy(lock_id, mode, host_id, drive);
		}
		else if(strcmp(argv[1], "sync_lock_refresh") == 0){
			ret = nvme_idm_sync_lock_refresh(lock_id, mode, host_id, drive, timeout);
		}
		else if(strcmp(argv[1], "sync_lock_renew") == 0){
			ret = nvme_idm_sync_lock_renew(lock_id, mode, host_id, drive, timeout);
		}
		else if(strcmp(argv[1], "sync_unlock") == 0){
			ret = nvme_idm_sync_unlock(lock_id, mode, host_id, lvb, lvb_size, drive);
		}
		else if(strcmp(argv[1], "sync_read_state") == 0){
			ret = nvme_idm_sync_read_host_state(lock_id, host_id, &host_state, drive);
			printf("output: host_state=%d\n", host_state);
		}
		else if(strcmp(argv[1], "sync_read_count") == 0){
			ret = nvme_idm_sync_read_lock_count(lock_id, host_id, &count, &self, drive);
			printf("output: lock_count=%d, self=%d\n", count, self);
		}
		else if(strcmp(argv[1], "sync_read_mode") == 0){
			ret = nvme_idm_sync_read_lock_mode(lock_id, &mode, drive);
			printf("output: lock_mode=%d\n", mode);
		}
		else if(strcmp(argv[1], "sync_read_lvb") == 0){
			ret = nvme_idm_sync_read_lvb(lock_id, host_id, lvb, lvb_size, drive);
		}
		else if(strcmp(argv[1], "sync_read_group") == 0){
			ret = nvme_idm_sync_read_mutex_group(drive, &info_list, &info_num);
			printf("output: info_num=%u\n", info_num);
		}
		else if(strcmp(argv[1], "sync_read_num") == 0){
			ret = nvme_idm_sync_read_mutex_num(drive, &mutex_num);
			printf("output: mutex_num=%u\n", mutex_num);
		}
		else if(strcmp(argv[1], "version") == 0){
			ret = nvme_idm_read_version(&version, drive);
			printf("output: version=%d\n", version);
		}
		else {
			printf("%s: invalid command option!\n", argv[1]);
			return -1;
		}
		printf("'%s' exiting with %d\n", argv[1], ret);
	}
	else{
		printf("No command option given\n");
	}

	nvme_idm_environ_destroy();
INIT_FAIL_EXIT:
	return ret;
}
#endif//DBG__NVME_API_MAIN_ENABLE
