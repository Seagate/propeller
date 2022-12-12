/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_api.c - Primary NVMe interface for In-drive Mutex (IDM)
 *
 *
 * NOTES on IDM API:
 * The term IDM API is defined here as a "top" level interface consisting of functions that
 * interact with or manipulate the IDM.  The functions in this file represent the NVMe-specific versions
 * of the IDM APIs.  These IDM APIs are then intended to be called externally by programs such as linux's lvm2.
 */

#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "idm_nvme_api.h"
#include "idm_nvme_io.h"
#include "idm_nvme_utils.h"


//////////////////////////////////////////
// COMPILE FLAGS
//////////////////////////////////////////
//TODO: DELETE THESE 2 (AND ALL CORRESPONDING CODE) AFTER NVME FILES COMPILE WITH THE REST OF PROPELLER.
#define COMPILE_STANDALONE
#define MAIN_ACTIVATE
#define FORCE_MUTEX_NUM    //TODO: HACK!!  This MUST be remove!!

#define FUNCTION_ENTRY_DEBUG    //TODO: Remove this entirely???

//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * nvme_async_idm_get_result - Retreive the result for normal async operations.

 * @handle:      NVMe request handle for the previously sent NVMe cmd.
 * @result:      Returned result (0 or -ve value) for the previously sent NVMe command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_get_result(uint64_t handle, int *result) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm = (nvmeIdmRequest *)handle;
    int            ret          = SUCCESS;

    ret = nvme_async_idm_data_rcv(request_idm, result);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_data_rcv fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_data_rcv fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

//TODO: How should I handle the error code from the current VS the previous command??
//          Especially as compared to what the SCSI code does.
//          Talk to Tom.
    if (*result < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: previous async cmd fail result=%d", __func__, *result);
        #else
        printf("%s: previous async cmd fail: result=%d\n", __func__, *result);
        #endif //COMPILE_STANDALONE
    }

EXIT:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_async_idm_get_result_lock_count - Asynchronously retreive the result for a previous
 * asynchronous read lock count request.
 *
 * @handle:     NVMe request handle for the previously sent NVMe cmd.
 * @count:      Returned lock count.
 *              Referenced value set to 0 on error.
 * @self:       Returned self count.
 *              Referenced value set to 0 on error.
 * @result:     Returned result (0 or -ve value) for the previously sent NVMe command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_get_result_lock_count(uint64_t handle, int *count, int *self, int *result) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm = (nvmeIdmRequest *)handle;
    int            ret          = SUCCESS;

    // Initialize the return parameters
    *count = 0;
    *self  = 0;

    ret = nvme_async_idm_data_rcv(request_idm, result);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_data_rcv fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_data_rcv fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    if (*result < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: previous async cmd fail: result=%d", __func__, *result);
        #else
        printf("%s: previous async cmd fail: result=%d\n", __func__, *result);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_lock_count(request_idm, count, self);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_lock_count fail %d", __func__, ret);
        #else
        printf("%s: _parse_lock_count fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: lock_count=%d, self_count=%d", __func__, *count, *self);
    #else
    printf("%s: found: lock_count=%d, self_count=%d\n", __func__, *count, *self);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_async_idm_get_result_lock_mode - Asynchronously retreive the result for a previous
 * asynchronous read lock mode request.
 *
 * @handle:     NVMe request handle for the previously sent NVMe cmd.
 * @mode:       Returned lock mode (unlock, shareable, exclusive).
 *              Referenced value set to -1 on error.
 * @result:     Returned result (0 or -ve value) for the previously sent NVMe command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_get_result_lock_mode(uint64_t handle, int *mode, int *result) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm = (nvmeIdmRequest *)handle;
    int            ret          = SUCCESS;

    // Initialize the return parameter
    *mode = -1;    //TODO: hardcoded state. add an "error" state to the enum?

    ret = nvme_async_idm_data_rcv(request_idm, result);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_data_rcv fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_data_rcv fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    if (*result < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: previous async cmd fail: result=%d", __func__, *result);
        #else
        printf("%s: previous async cmd fail: result=%d\n", __func__, *result);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_lock_mode(request_idm, mode);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_lock_mode fail %d", __func__, ret);
        #else
        printf("%s: _parse_lock_mode fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: mode=%d", __func__, *mode);
    #else
    printf("%s: found: mode=%d\n", __func__, *mode);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_async_idm_get_result_lvb - Asynchronously retreive the result for a previous
 * asynchronous read lvb request.
 *
 * @handle:     NVMe request handle for the previously sent NVMe cmd.
 * @mode:       Returned lock mode (unlock, shareable, exclusive).
 *              Referenced value set to -1 on error.
 * @lvb_size:   Lock value block size.
 * @result:     Returned result (0 or -ve value) for the previously sent NVMe command.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_get_result_lvb(uint64_t handle, char *lvb, int lvb_size, int *result) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm = (nvmeIdmRequest *)handle;
    int            ret          = SUCCESS;

    //TODO: The -ve check below should go away cuz lvb_size should be of unsigned type.
    //However, this requires an IDM API parameter type change.
    if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
        return -EINVAL;

    ret = nvme_async_idm_data_rcv(request_idm, result);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_data_rcv fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_data_rcv fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    if (*result < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: previous async cmd fail: result=%d", __func__, *result);
        #else
        printf("%s: previous async cmd fail: result=%d\n", __func__, *result);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_lvb(request_idm, lvb, lvb_size);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_lvb fail %d", __func__, ret);
        #else
        printf("%s: _parse_lvb fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    #ifndef COMPILE_STANDALONE
    ilm_log_array_dbg(" found: lvb", lvb, lvb_size);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_async_idm_lock - Asynchronously acquire an IDM on a specified NVMe drive.
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
int nvme_async_idm_lock(char *lock_id, int mode, char *host_id,
                        char *drive, uint64_t timeout, uint64_t *handle) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;  //TODO: Should ALL of these be initialized to FAILURE, instead of SUCCESS???  Probably going to be case-by-case

    ret = _init_lock(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock fail %d", __func__, ret);
        #else
        printf("%s: _init_lock fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_async_idm_lock_break - Asynchronously break an IDM lock if before other
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
int nvme_async_idm_lock_break(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout, uint64_t *handle) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;

    ret = _init_lock_break(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock_break fail %d", __func__, ret);
        #else
        printf("%s: _init_lock_break fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_async_idm_lock_convert - Asynchronously convert the lock mode for an IDM.
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
int nvme_async_idm_lock_convert(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t timeout, uint64_t *handle)
{
    return nvme_async_idm_lock_refresh(lock_id, mode, host_id,
                                       drive, timeout, handle);
}

/**
 * nvme_async_idm_lock_destroy - Asynchronously destroy an IDM and release all associated resource.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_lock_destroy(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t *handle)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;

    ret = _init_lock_destroy(lock_id, mode, host_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock_destroy fail %d", __func__, ret);
        #else
        printf("%s: _init_lock_destroy fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_async_idm_lock_refresh - Asynchronously refreshes the host's membership for an IDM.
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
int nvme_async_idm_lock_refresh(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t timeout, uint64_t *handle)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;

    ret = _init_lock_refresh(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock_refresh fail %d", __func__, ret);
        #else
        printf("%s: _init_lock_refresh fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_async_idm_lock_renew - Asynchronously renew host's membership for an IDM.
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
int nvme_async_idm_lock_renew(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout, uint64_t *handle)
{
    return nvme_async_idm_lock_refresh(lock_id, mode, host_id,
                                       drive, timeout, handle);
}

/**
 * nvme_async_idm_read_lock_count - Asynchrnously read the lock count for an IDM.
 * This command only issues the request.  It does NOT retrieve the data.
 * The desired data is returned during a separate call to nvme_async_idm_get_result_lock_count().
 *
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_read_lock_count(char *lock_id, char *host_id, char *drive, uint64_t *handle) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _init_read_lock_count(ASYNC_ON, lock_id, host_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_lock_count fail %d", __func__, ret);
        #else
        printf("%s: _init_read_lock_count fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_async_idm_read_lock_mode - Asynchronously read back an IDM's current mode.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_read_lock_mode(char *lock_id, char *drive, uint64_t *handle) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _init_read_lock_mode(ASYNC_ON, lock_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_lock_mode fail %d", __func__, ret);
        #else
        printf("%s: _init_read_lock_mode fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_async_idm_read_lvb - Asynchronously read the lock value block(lvb) which is
 * associated to an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @handle:     Returned NVMe request handle.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int nvme_async_idm_read_lvb(char *lock_id, char *host_id, char *drive, uint64_t *handle)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _init_read_lvb(ASYNC_ON, lock_id, host_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_lvb fail %d", __func__, ret);
        #else
        printf("%s: _init_read_lvb fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_async_idm_unlock - Asynchronously release an IDM on a specified drive.
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
int nvme_async_idm_unlock(char *lock_id, int mode, char *host_id,
                          char *lvb, int lvb_size, char *drive, uint64_t *handle)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _init_unlock(lock_id, mode, host_id, lvb, lvb_size, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_unlock fail %d", __func__, ret);
        #else
        printf("%s: _init_unlock fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * nvme_sync_idm_lock - Synchronously acquire an IDM on a specified NVMe drive.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_lock(char *lock_id, int mode, char *host_id,
                       char *drive, uint64_t timeout) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;

    ret = _init_lock(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock fail %d", __func__, ret);
        #else
        printf("%s: _init_lock fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

EXIT:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_lock_break - Synchronously break an IDM lock if before other
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
int nvme_sync_idm_lock_break(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _init_lock_break(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock_break fail %d", __func__, ret);
        #else
        printf("%s: _init_lock_break fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

EXIT:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_lock_convert - Synchronously convert the lock mode for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_lock_convert(char *lock_id, int mode, char *host_id,
                               char *drive, uint64_t timeout)
{
    return nvme_sync_idm_lock_refresh(lock_id, mode, host_id,
                                      drive, timeout);
}

/**
 * nvme_sync_idm_lock_destroy - Synchronously destroy an IDM and release all associated resource.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_lock_destroy(char *lock_id, int mode, char *host_id, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;

    ret = _init_lock_destroy(lock_id, mode, host_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock_destroy fail %d", __func__, ret);
        #else
        printf("%s: _init_lock_destroy fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

EXIT:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_lock_refresh - Synchronously refreshes the host's membership for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_lock_refresh(char *lock_id, int mode, char *host_id,
                                  char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _init_lock_refresh(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock_refresh fail %d", __func__, ret);
        #else
        printf("%s: _init_lock_refresh fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

EXIT:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_lock_renew - Synchronously renew host's membership for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int nvme_sync_idm_lock_renew(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout)
{
    return nvme_sync_idm_lock_refresh(lock_id, mode, host_id,
                                      drive, timeout);
}

/**
 * nvme_sync_idm_read_host_state - Read back the host's state for an specific IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @host_state: Returned host state.
 *              Referenced value set to -1 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_read_host_state(char *lock_id, char *host_id, int *host_state, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    // Initialize the output
//TODO: Does ALWAYS setting to -1 on failure make sense?
//          Refer to scsi-side if removed.
//          Was being set to -1 in a couple locations.
    *host_state = -1;    //TODO: hardcoded state. add an "error" state to the enum?

    ret = _init_read_host_state(lock_id, host_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_host_state fail %d", __func__, ret);
        #else
        printf("%s: _init_read_host_state fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_sync_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_host_state(request_idm, host_state);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_host_state fail %d", __func__, ret);
        #else
        printf("%s: _parse_host_state fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: host_state=0x%X (%d)", __func__, *host_state, *host_state);
    #else
    printf("%s: found: host_state=0x%X (%d)\n", __func__, *host_state, *host_state);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_read_lock_count - Synchrnously read the lock count for an IDM.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @count:      Returned lock count.
 *              Referenced value set to 0 on error.
 * @self:       Returned self count.
 *              Referenced value set to 0 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_read_lock_count(char *lock_id, char *host_id, int *count, int *self, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    // Initialize the output
    *count = 0;
    *self  = 0;

    ret = _init_read_lock_count(ASYNC_OFF, lock_id, host_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_lock_count fail %d", __func__, ret);
        #else
        printf("%s: _init_read_lock_count fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    if (!request_idm) {         //Occurs when mutex_num=0
        return SUCCESS;
    }

    ret = nvme_sync_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_lock_count(request_idm, count, self);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_lock_count fail %d", __func__, ret);
        #else
        printf("%s: _parse_lock_count fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: lock_count=%d, self_count=%d", __func__, *count, *self);
    #else
    printf("%s: found: lock_count=%d, self_count=%d\n", __func__, *count, *self);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_read_lock_mode - Synchronously read back an IDM's current mode.
 *
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Returned lock mode (unlock, shareable, exclusive).
 *              Referenced value set to -1 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_read_lock_mode(char *lock_id, int *mode, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    // Initialize the output
    *mode = -1;    //TODO: hardcoded state. add an "error" state to the enum?

    ret = _init_read_lock_mode(ASYNC_OFF, lock_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_lock_mode fail %d", __func__, ret);
        #else
        printf("%s: _init_read_lock_mode fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    if (!request_idm) {         //Occurs when mutex_num=0
        *mode = IDM_MODE_UNLOCK;
        return SUCCESS;
    }

    ret = nvme_sync_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_lock_mode(request_idm, mode);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_lock_mode fail %d", __func__, ret);
        #else
        printf("%s: _parse_lock_mode fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: mode=%d", __func__, *mode);
    #else
    printf("%s: found: mode=%d\n", __func__, *mode);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_read_lvb - Synchronously read the lock value block(lvb) which is
 * associated to an IDM.
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
int nvme_sync_idm_read_lvb(char *lock_id, char *host_id, char *lvb, int lvb_size, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    //TODO: The -ve check below should go away cuz lvb_size should be of unsigned type.
    //However, this requires an IDM API parameter type change.
    if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
        return -EINVAL;

    ret = _init_read_lvb(ASYNC_OFF, lock_id, host_id, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_lvb fail %d", __func__, ret);
        #else
        printf("%s: _init_read_lvb fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    if (!request_idm) {         //Occurs when mutex_num=0
        memset(lvb, 0x0, lvb_size);
        return SUCCESS;
    }

    ret = nvme_sync_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_lvb(request_idm, lvb, lvb_size);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_lvb fail %d", __func__, ret);
        #else
        printf("%s: _parse_lvb fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    #ifndef COMPILE_STANDALONE
    ilm_log_array_dbg(" found: lvb", lvb, lvb_size);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_read_mutex_group - Synchronously read back mutex group info for all IDM in the drives
 *
 * @drive:      Drive path name.
 * @info_ptr:   Returned pointer for info list.
 *              Referenced pointer set to NULL on error.
 * @info_num:   Returned pointer for info num.
 *              Referenced value set to 0 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_read_mutex_group(char *drive, idmInfo **info_ptr, int *info_num)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            i, ret = SUCCESS;

    // Initialize the output
    *info_ptr = NULL;
    *info_num = 0;

    ret = _init_read_mutex_group(drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_mutex_group fail %d", __func__, ret);
        #else
        printf("%s: _init_read_mutex_group fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_sync_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = _parse_mutex_group(request_idm, info_ptr, info_num);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _parse_mutex_group fail %d", __func__, ret);
        #else
        printf("%s: _parse_mutex_group fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    //TODO: Keep -OR- Add debug flag??
    dumpIdmInfoStruct(*info_ptr);

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: info_num=%d", __func__, *info_num);
    #else
    printf("%s: found: info_num=%d\n", __func__, *info_num);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_read_mutex_num - Synchronously retrieves the number of mutexes
 * present on the drive.
 *
 * @drive:       Drive path name.
 * @mutex_num:   Returned number of mutexes present on the drive.
//TODO: Does ALWAYS setting to 0 on failure make sense?
 *               Set to 0 on failure.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_read_mutex_num(char *drive, unsigned int *mutex_num)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

//TODO: The SCSI-side code tends to set this to 0 on certain failures.
//          Does ALWAYS setting to 0 on failure HERE make sense (this is a bit different from scsi-side?
    *mutex_num = 0;

    ret = _init_read_mutex_num(drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_read_mutex_num fail %d", __func__, ret);
        #else
        printf("%s: _init_read_mutex_num fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT_FAIL;
    }

    ret = nvme_sync_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_read fail %d", __func__, ret);
        goto EXIT_FAIL;
        #else
//TODO: !!!! Temp HACK!!!!! For stand alone, to get the reads working.
        printf("%s: nvme_sync_idm_read fail %d, Continuing\n", __func__, ret);
        ret = SUCCESS;
        #endif //COMPILE_STANDALONE
    }

    _parse_mutex_num(request_idm, mutex_num);

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: mutex_num=%d", __func__, *mutex_num);
    #else
    printf("%s: found: mutex_num=%d\n", __func__, *mutex_num);
    #endif //COMPILE_STANDALONE
EXIT_FAIL:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_unlock - Synchronously release an IDM on a specified drive.
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
int nvme_sync_idm_unlock(char *lock_id, int mode, char *host_id,
                         char *lvb, int lvb_size, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _init_unlock(lock_id, mode, host_id, lvb, lvb_size, drive, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_unlock fail %d", __func__, ret);
        #else
        printf("%s: _init_unlock fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

EXIT:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * _init_lock - Convenience function containing common code for the IDM lock action.
 *
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @timeout:     Timeout for membership (unit: millisecond).
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock(char *lock_id, int mode, char *host_id, char *drive,
               uint64_t timeout, nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _validate_input_write fail %d", __func__, ret);
        #else
        printf("%s: _validate_input_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _memory_init_idm_request fail %d", __func__, ret);
        #else
        printf("%s: _memory_init_idm_request fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = nvme_idm_write_init(lock_id, mode, host_id, drive, timeout, (*request_idm));
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_write_init fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock_break(char *lock_id, int mode, char *host_id, char *drive,
                     uint64_t timeout, nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _validate_input_write fail %d", __func__, ret);
        #else
        printf("%s: _validate_input_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _memory_init_idm_request fail %d", __func__, ret);
        #else
        printf("%s: _memory_init_idm_request fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = nvme_idm_write_init(lock_id, mode, host_id, drive, timeout, (*request_idm));
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_write_init fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock_destroy(char *lock_id, int mode, char *host_id, char *drive,
                       nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _validate_input_write fail %d", __func__, ret);
        #else
        printf("%s: _validate_input_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _memory_init_idm_request fail %d", __func__, ret);
        #else
        printf("%s: _memory_init_idm_request fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = nvme_idm_write_init(lock_id, mode, host_id, drive, 0, (*request_idm));
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_write_init fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_lock_refresh(char *lock_id, int mode, char *host_id, char *drive,
                       uint64_t timeout, nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _validate_input_write fail %d", __func__, ret);
        #else
        printf("%s: _validate_input_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _memory_init_idm_request fail %d", __func__, ret);
        #else
        printf("%s: _memory_init_idm_request fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    ret = nvme_idm_write_init(lock_id, mode, host_id, drive, timeout, (*request_idm));
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_write_init fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    (*request_idm)->opcode_idm   = IDM_OPCODE_REFRESH;
    (*request_idm)->res_ver_type = (char)IDM_RES_VER_NO_UPDATE_NO_VALID;

    return ret;
}

/**
 * _init_read_host_state - Convenience function containing common code for the retrieval
 * of the IDM host state from the drive.
 *
 * @lock_id:     Lock ID (64 bytes).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_host_state(char *lock_id, char *host_id, char *drive,
                          nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    unsigned int   mutex_num = 0;
    int            ret       = SUCCESS;

    ret = _validate_input_common(lock_id, host_id, drive);
    if (ret < 0)
        return ret;

    ret = nvme_sync_idm_read_mutex_num(drive, &mutex_num);
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
 * _init_read_lock_count - Convenience function containing common code for the retrieval
 * of the IDM lock count from the drive.
 *
 * @async_on:    Boolean flag indicating async(1) or sync(0) wrapping function.
 *               Async and sync usage have different control flows.
 * @lock_id:     Lock ID (64 bytes).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_lock_count(int async_on, char *lock_id, char *host_id, char *drive,
                          nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    unsigned int   mutex_num = 0;
    int            ret       = SUCCESS;

    ret = _validate_input_common(lock_id, host_id, drive);
    if (ret < 0)
        return ret;

    ret = nvme_sync_idm_read_mutex_num(drive, &mutex_num);
    if (ret < 0)
        return -ENOENT;
    else if (!mutex_num)
        return SUCCESS;

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
 * _init_read_lock_mode - Convenience function containing common code for the retrieval
 * of the IDM lock mode from the drive.
 *
 * @async_on:    Boolean flag indicating async(1) or sync(0) wrapping function.
 *               Async and sync usage have different control flows.
 * @lock_id:     Lock ID (64 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_lock_mode(int async_on, char *lock_id, char *drive,
                         nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    unsigned int   mutex_num = 0;
    int            ret       = SUCCESS;

    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif //COMPILE_STANDALONE

    if (!lock_id || !drive)
        return -EINVAL;

    ret = nvme_sync_idm_read_mutex_num(drive, &mutex_num);
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
 * _init_read_lvb - Convenience function containing common code for the retrieval
 * of the lock value block(lvb) from the drive.
 *
 * @async_on:    Boolean flag indicating async(1) or sync(0) wrapping function.
 *               Async and sync usage have different control flows.
 * @lock_id:     Lock ID (64 bytes).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_lvb(int async_on, char *lock_id, char *host_id, char *drive,
                   nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    unsigned int   mutex_num = 0;
    int            ret = SUCCESS;

    ret = _validate_input_common(lock_id, host_id, drive);
    if (ret < 0)
        return ret;

    ret = nvme_sync_idm_read_mutex_num(drive, &mutex_num);
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
 * _init_read_mutex_group - Convenience function containing common code for the retrieval
 * of IDM group information from the drive.
 *
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_mutex_group(char *drive, nvmeIdmRequest **request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    unsigned int mutex_num = 0;
    int          ret       = SUCCESS;

    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif //COMPILE_STANDALONE

    if (!drive)
        return -EINVAL;

    ret = nvme_sync_idm_read_mutex_num(drive, &mutex_num);
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
 * _init_read_mutex_num - Convenience function containing common code for the retrieval
 * of the number of lock mutexes available on the device.
 *
 * @drive:       Drive path name.
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_read_mutex_num(char *drive, nvmeIdmRequest **request_idm)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif //COMPILE_STANDALONE

    ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _memory_init_idm_request fail %d", __func__, ret);
        #else
        printf("%s: _memory_init_idm_request fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
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
 * @request_idm: Returned struct containing all NVMe-specific command info for the
 *               requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _init_unlock(char *lock_id, int mode, char *host_id, char *lvb, int lvb_size,
                 char *drive, nvmeIdmRequest **request_idm)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _validate_input_write fail %d", __func__, ret);
        #else
        printf("%s: _validate_input_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //TODO: The -ve check here should go away cuz lvb_size should be of unsigned type.
    //However, this requires an IDM API parameter type change.
    if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
        return -EINVAL;

    ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _memory_init_idm_request fail %d", __func__, ret);
        #else
        printf("%s: _memory_init_idm_request fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //TODO: Why 0 timeout here (ported as-is from scsi-side)?
    ret = nvme_idm_write_init(lock_id, mode, host_id, drive, 0, (*request_idm));
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_write_init fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_write_init fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    (*request_idm)->opcode_idm   = IDM_OPCODE_UNLOCK;
    (*request_idm)->res_ver_type = (char)IDM_RES_VER_UPDATE_NO_VALID;
    memcpy((*request_idm)->lvb, lvb, lvb_size);

    return ret;
}

/**
 * _memory_free_idm_request - Convenience function for freeing memory for all the
 * data structures used during the NVMe command sequence.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
void _memory_free_idm_request(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    if (request_idm->data_idm) {
        free(request_idm->data_idm);
        request_idm->data_idm = NULL;
    }
    if (request_idm) {
        free(request_idm);
        request_idm = NULL;
    }
}

/**
 * _memory_init_idm_request - Convenience function for allocating memory for all the
 *  data structures used during the NVMe command sequence.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the requested IDM action.
 *               Note: **request_idm is due to malloc() needing access to the original pointer.
 * @data_num:    Number of data payload instances that need memory allocation.
 *               Also corresponds to the number of mutexes on the drive.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _memory_init_idm_request(nvmeIdmRequest **request_idm, unsigned int data_num) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int data_len;
    int ret = SUCCESS;

    *request_idm = malloc(sizeof(nvmeIdmRequest));
    if (!request_idm) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: request memory allocate fail", __func__);
        #else
        printf("%s: request memory allocate fail\n", __func__);
        #endif //COMPILE_STANDALONE
        return -ENOMEM;
    }
    memset((*request_idm), 0, sizeof(**request_idm));

    data_len                 = sizeof(idmData) * data_num;
    (*request_idm)->data_idm = malloc(data_len);
    if (!(*request_idm)->data_idm) {
        _memory_free_idm_request((*request_idm));
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: request data memory allocate fail", __func__);
        #else
        printf("%s: request data memory allocate fail\n", __func__);
        #endif //COMPILE_STANDALONE
        return -ENOMEM;
    }
    memset((*request_idm)->data_idm, 0, data_len);

    //Cache memory-specifc info.
    (*request_idm)->data_len = data_len;
    (*request_idm)->data_num = data_num;

    return ret;
}

/**
 * _parse_host_state - Convenience function for parsing the host state out of the
 * returned data payload.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 * @host_state:  Returned host state.
 *               Referenced value set to -1 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _parse_host_state(nvmeIdmRequest *request_idm, int *host_state) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    idmData        *data_idm;
    char           bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
    char           bswap_host_id[IDM_HOST_ID_LEN_BYTES];
    unsigned int   mutex_num = request_idm->data_num;
    int            ret       = SUCCESS;

    bswap_char_arr(bswap_lock_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);
    bswap_char_arr(bswap_host_id, request_idm->host_id, IDM_HOST_ID_LEN_BYTES);

    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {
        #ifndef COMPILE_STANDALONE
        ilm_log_array_dbg("resource_id",  data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("lock_id",      bswap_lock_id,           IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("data host_id", data_idm[i].host_id,     IDM_HOST_ID_LEN_BYTES);
        ilm_log_array_dbg("host_id",      bswap_host_id,           IDM_HOST_ID_LEN_BYTES);
        #else
        printf("bswap_lock_id    = '");
        _print_char_arr(bswap_lock_id, IDM_LOCK_ID_LEN_BYTES);
        printf("data resource_id = '");
        _print_char_arr(data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        printf("bswap_host_id = '");
        _print_char_arr(bswap_host_id, IDM_HOST_ID_LEN_BYTES);
        printf("data host_id  = '");
        _print_char_arr(data_idm[i].host_id, IDM_HOST_ID_LEN_BYTES);
        #endif //COMPILE_STANDALONE

        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, bswap_lock_id, IDM_LOCK_ID_LEN_BYTES))
            continue;

        if (memcmp(data_idm[i].host_id, bswap_host_id, IDM_HOST_ID_LEN_BYTES))
            continue;

        *host_state = __bswap_64(data_idm[i].state);
        break;
    }

    return ret;
}

/**
 * _parse_lock_count - Convenience function for parsing the lock count out of the
 * returned data payload.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the
 *               requested IDM action.
 * @count:       Returned lock count.
 *               Referenced value set to 0 on error.
 * @self:        Returned self count.
 *               Referenced value set to 0 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _parse_lock_count(nvmeIdmRequest *request_idm, int *count, int *self) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    idmData        *data_idm;
    char           bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
    char           bswap_host_id[IDM_HOST_ID_LEN_BYTES];
    unsigned int   mutex_num = request_idm->data_num;
    int            ret = SUCCESS;
    uint64_t       state, locked;

    bswap_char_arr(bswap_lock_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);
    bswap_char_arr(bswap_host_id, request_idm->host_id, IDM_HOST_ID_LEN_BYTES);

    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {

        state  = __bswap_64(data_idm[i].state);
        locked = (state == IDM_STATE_LOCKED) || (state == IDM_STATE_MULTIPLE_LOCKED);

        if (!locked)
            continue;

        #ifndef COMPILE_STANDALONE
        ilm_log_array_dbg("resource_id",  data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("lock_id",      bswap_lock_id,           IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("data host_id", data_idm[i].host_id,     IDM_HOST_ID_LEN_BYTES);
        ilm_log_array_dbg("host_id",      bswap_host_id,           IDM_HOST_ID_LEN_BYTES);
        #else
        printf("bswap_lock_id    = '");
        _print_char_arr(bswap_lock_id, IDM_LOCK_ID_LEN_BYTES);
        printf("data resource_id = '");
        _print_char_arr(data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        printf("bswap_host_id = '");
        _print_char_arr(bswap_host_id, IDM_HOST_ID_LEN_BYTES);
        printf("data host_id  = '");
        _print_char_arr(data_idm[i].host_id, IDM_HOST_ID_LEN_BYTES);
        #endif //COMPILE_STANDALONE

        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, bswap_lock_id, IDM_LOCK_ID_LEN_BYTES))
            continue;

        if (memcmp(data_idm[i].host_id, bswap_host_id, IDM_HOST_ID_LEN_BYTES)) {
            /* Must be wrong if self has been accounted */
            if (*self) {
                #ifndef COMPILE_STANDALONE
                ilm_log_err("%s: account self %d > 1", __func__, *self);
                #else
                printf("%s: account self %d > 1\n", __func__, *self);
                #endif //COMPILE_STANDALONE
                goto EXIT;
            }
            *self = 1;
        }
        else {
            *count += 1;
        }
    }

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
int _parse_lock_mode(nvmeIdmRequest *request_idm, int *mode) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    idmData        *data_idm;
    char           bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
    unsigned int   mutex_num = request_idm->data_num;
    int            ret       = SUCCESS;
    uint64_t       state, class;

    bswap_char_arr(bswap_lock_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);

    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {

        #ifndef COMPILE_STANDALONE
        ilm_log_array_dbg("resource_id",   data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("lock_id(bswap)", bswap_lock_id,           IDM_LOCK_ID_LEN_BYTES);
        #else
        printf("lock_id(bswap) = '");
        _print_char_arr(bswap_lock_id, IDM_LOCK_ID_LEN_BYTES);
        printf("resource_id    = '");
        _print_char_arr(data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        #endif //COMPILE_STANDALONE

        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, bswap_lock_id, IDM_LOCK_ID_LEN_BYTES))
            continue;

        state = __bswap_64(data_idm[i].state);
        class = __bswap_64(data_idm[i].class);

        #ifndef COMPILE_STANDALONE
        ilm_log_dbg("%s: state=%lx class=%lx", __func__, state, class);
        #else
        printf("%s: state=%lx class=%lx\n", __func__, state, class);
        #endif //COMPILE_STANDALONE

        if (state == IDM_STATE_UNINIT ||
            state == IDM_STATE_UNLOCKED ||
            state == IDM_STATE_TIMEOUT) {
            *mode = IDM_MODE_UNLOCK;
        } else if (class == IDM_CLASS_EXCLUSIVE) {
            *mode = IDM_MODE_EXCLUSIVE;
        } else if (class == IDM_CLASS_SHARED_PROTECTED_READ) {
            *mode = IDM_MODE_SHAREABLE;
        } else if (class == IDM_CLASS_PROTECTED_WRITE) {
            #ifndef COMPILE_STANDALONE
            ilm_log_err("%s: PROTECTED_WRITE is not unsupported", __func__);
            #endif //COMPILE_STANDALONE
            ret = -EFAULT;
        }

        #ifndef COMPILE_STANDALONE
        ilm_log_dbg("%s: mode=%d", __func__, *mode);
        #endif //COMPILE_STANDALONE

        if (*mode == IDM_MODE_EXCLUSIVE || *mode == IDM_MODE_SHAREABLE)
            break;
    }

    //If the mutex is not found in drive fimware,
    // simply return success and mode is unlocked.
    if (*mode == -1)
        *mode = IDM_MODE_UNLOCK;

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
int _parse_lvb(nvmeIdmRequest *request_idm, char *lvb, int lvb_size) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    idmData        *data_idm;
    char           bswap_lock_id[IDM_LOCK_ID_LEN_BYTES];
    char           bswap_host_id[IDM_HOST_ID_LEN_BYTES];
    unsigned int   mutex_num = request_idm->data_num;
    int            ret       = SUCCESS;

    bswap_char_arr(bswap_lock_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);
    bswap_char_arr(bswap_host_id, request_idm->host_id, IDM_HOST_ID_LEN_BYTES);

    ret = -ENOENT;
    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {
        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, bswap_lock_id, IDM_LOCK_ID_LEN_BYTES))
            continue;

        if (memcmp(data_idm[i].host_id, bswap_host_id, IDM_HOST_ID_LEN_BYTES))
            continue;

        bswap_char_arr(lvb, data_idm[i].resource_ver, lvb_size);
        ret = SUCCESS;
        break;
    }

    return ret;
}

/**
 * _parse_mutex_group - Convenience function for parsing idmInfo data out of the
 * returned data payload.
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
int _parse_mutex_group(nvmeIdmRequest *request_idm, idmInfo **info_ptr, int *info_num) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    idmData        *data_idm;
    unsigned int   mutex_num = request_idm->data_num;
    int            i, ret    = SUCCESS;
    uint64_t       state, class;
    idmInfo        *info_list, *info;

    info_list = malloc(sizeof(idmInfo) * mutex_num);
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
        bswap_char_arr(info->id,      data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        bswap_char_arr(info->host_id, data_idm[i].host_id,     IDM_HOST_ID_LEN_BYTES);

        class = __bswap_64(data_idm[i].class);

        switch (class) {
            case IDM_CLASS_EXCLUSIVE:
                info->mode = IDM_MODE_EXCLUSIVE;
                break;
            case IDM_CLASS_SHARED_PROTECTED_READ:
                info->mode = IDM_MODE_SHAREABLE;
                break;
            default:
                #ifndef COMPILE_STANDALONE
                ilm_log_err("%s: IDM class is not unsupported %ld", __func__, class);
                #endif //COMPILE_STANDALONE
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

EXIT:
    if (ret != 0 && info_list)
        free(info_list);
    return ret;
}

/**
 * _parse_mutex_num - Convenience function for parsing the number of mutexes out of the
 * returned data payload.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 * @mutex_num:      Returned number of mutexes present on the drive.
 */
void _parse_mutex_num(nvmeIdmRequest *request_idm, int *mutex_num) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    unsigned char  *data;

//TODO: Ported from scsi-side as-is.  Need to verify if this even makes sense for nvme.
//TODO: Why is "unsigned char" being used here??
    data = (unsigned char *)request_idm->data_idm;
    #ifdef FORCE_MUTEX_NUM
//TODO: This can't stay. Necessary for stand-alone code
    *mutex_num = 1;     //For debug. This func called by many others.
    #else
    *mutex_num = ((data[1]) & 0xff);
    *mutex_num |= ((data[0]) & 0xff) << 8;
    #endif //FORCE_MUTEX_NUM

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: data[0]=%u data[1]=%u mutex mutex_num=%u",
                __func__, data[0], data[1], *mutex_num);
    #else
    printf("%s: data[0]=%u data[1]=%u mutex mutex_num=%u\n",
           __func__, data[0], data[1], *mutex_num);
    #endif //COMPILE_STANDALONE
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
int _validate_input_common(char *lock_id, char *host_id, char *drive) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif //COMPILE_STANDALONE

    if (!lock_id || !host_id || !drive)
        return -EINVAL;

    return ret;
}

/**
 * _validate_input_write - Convenience function for validating IDM API input parameters
 * used during an IDM write cmd.
 *
 * @lock_id:        Lock ID (64 bytes).
 * @mode:           Lock mode (unlock, shareable, exclusive).
 * @host_id:        Host ID (32 bytes).
 * @drive:          Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _validate_input_write(char *lock_id, int mode, char *host_id, char *drive) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _validate_input_common(lock_id, host_id, drive);

    if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
        return -EINVAL;

    return ret;
}









#ifdef MAIN_ACTIVATE
/*#########################################################################################
########################### STAND-ALONE MAIN ##############################################
#########################################################################################*/
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1"

//To compile:
//gcc idm_nvme_io.c idm_nvme_api.c -o idm_nvme_api
int main(int argc, char *argv[])
{
    char drive[PATH_MAX];
    int  ret = 0;

    if(argc >= 3){
        strcpy(drive, argv[2]);
    }
    else {
        strcpy(drive, DRIVE_DEFAULT_DEVICE);
    }

    //cli usage: idm_nvme_api lock
    if(argc >= 2){
        char            lock_id[IDM_LOCK_ID_LEN_BYTES] = "0000000000000000000000000000000000000000000000000000000000000000";
        int             mode                           = IDM_MODE_EXCLUSIVE;
        char            host_id[IDM_HOST_ID_LEN_BYTES] = "00000000000000000000000000000000";
        uint64_t        timeout                        = 10;
        char            lvb[IDM_LVB_LEN_BYTES]         = "lvb";
        int             lvb_size                       = 5;
        uint64_t        handle;
        int             result=0;
        unsigned int    mutex_num;
        unsigned int    info_num;
        idmInfo         *info_list;
        int             host_state;
        int             count;
        int             self;

        if(strcmp(argv[1], "async_get") == 0){
            ret = nvme_async_idm_lock(lock_id, mode, host_id, drive, timeout, &handle); // dummy call, ignore error.
            printf("'inserted lock' ret=%d\n", ret);
            ret = nvme_async_idm_get_result(handle, &result);
            printf("'%s' ret=%d, result=0x%X\n", argv[1], ret, result);
        }
        else if(strcmp(argv[1], "async_get_count") == 0){
            ret = nvme_async_idm_read_lock_count(lock_id, host_id, drive, &handle); // dummy call, ignore error.
            printf("'inserted read lock count' ret=%d\n", ret);
            ret = nvme_async_idm_get_result_lock_count(handle, &count, &self, &result);
            printf("'%s' ret=%d, result=0x%X\n", argv[1], ret, result);
        }
        else if(strcmp(argv[1], "async_get_mode") == 0){
            ret = nvme_async_idm_read_lock_mode(lock_id, drive, &handle); // dummy call, ignore error.
            printf("'inserted read lock mode' ret=%d\n", ret);
            ret = nvme_async_idm_get_result_lock_mode(handle, &mode, &result);
            printf("'%s' ret=%d, result=0x%X\n", argv[1], ret, result);
        }
        else if(strcmp(argv[1], "async_get_lvb") == 0){
            ret = nvme_async_idm_read_lvb(lock_id, host_id, drive, &handle); // dummy call, ignore error.
            printf("'inserted read lvb' ret=%d\n", ret);
            ret = nvme_async_idm_get_result_lvb(handle, lvb, lvb_size, &result);
            printf("'%s' ret=%d, result=0x%X\n", argv[1], ret, result);
        }
        else if(strcmp(argv[1], "async_lock") == 0){
            ret = nvme_async_idm_lock(lock_id, mode, host_id, drive, timeout, &handle);
        }
        else if(strcmp(argv[1], "async_lock_break") == 0){
            ret = nvme_async_idm_lock_break(lock_id, mode, host_id, drive, timeout, &handle);
        }
        else if(strcmp(argv[1], "async_lock_convert") == 0){
            ret = nvme_async_idm_lock_convert(lock_id, mode, host_id, drive, timeout, &handle);
        }
        else if(strcmp(argv[1], "async_lock_destroy") == 0){
            ret = nvme_async_idm_lock_destroy(lock_id, mode, host_id, drive, &handle);
        }
        else if(strcmp(argv[1], "async_lock_refresh") == 0){
            ret = nvme_async_idm_lock_refresh(lock_id, mode, host_id, drive, timeout, &handle);
        }
        else if(strcmp(argv[1], "async_lock_renew") == 0){
            ret = nvme_async_idm_lock_renew(lock_id, mode, host_id, drive, timeout, &handle);
        }
        else if(strcmp(argv[1], "async_read_count") == 0){
            ret = nvme_async_idm_read_lock_count(lock_id, host_id, drive, &handle);
        }
        else if(strcmp(argv[1], "async_read_mode") == 0){
            ret = nvme_async_idm_read_lock_mode(lock_id, drive, &handle);
        }
        else if(strcmp(argv[1], "async_read_lvb") == 0){
            ret = nvme_async_idm_read_lvb(lock_id, host_id, drive, &handle);
        }
        else if(strcmp(argv[1], "async_unlock") == 0){
            ret = nvme_async_idm_unlock(lock_id, mode, host_id, lvb, lvb_size, drive, &handle);
        }
        else if(strcmp(argv[1], "sync_lock") == 0){
            ret = nvme_sync_idm_lock(lock_id, mode, host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "sync_lock_break") == 0){
            ret = nvme_sync_idm_lock_break(lock_id, mode, host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "sync_lock_convert") == 0){
            ret = nvme_sync_idm_lock_convert(lock_id, mode, host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "sync_lock_destroy") == 0){
            ret = nvme_sync_idm_lock_destroy(lock_id, mode, host_id, drive);
        }
        else if(strcmp(argv[1], "sync_lock_refresh") == 0){
            ret = nvme_sync_idm_lock_refresh(lock_id, mode, host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "sync_lock_renew") == 0){
            ret = nvme_sync_idm_lock_renew(lock_id, mode, host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "sync_read_state") == 0){
            ret = nvme_sync_idm_read_host_state(lock_id, host_id, &host_state, drive);
            printf("output: host_state=%d\n", host_state);
        }
        else if(strcmp(argv[1], "sync_read_count") == 0){
            ret = nvme_sync_idm_read_lock_count(lock_id, host_id, &count, &self, drive);
            printf("output: lock_count=%d, self=%d\n", count, self);
        }
        else if(strcmp(argv[1], "sync_read_mode") == 0){
            ret = nvme_sync_idm_read_lock_mode(lock_id, &mode, drive);
            printf("output: lock_mode=%d\n", mode);
        }
        else if(strcmp(argv[1], "sync_read_lvb") == 0){
            //TODO: Should "lvb" be passed in with "&lvb" instead???
            //TODO: Technically, shouldn't ALL these param be passed in like that (ie - &lock_id, &host_id, etc)??
            ret = nvme_sync_idm_read_lvb(lock_id, host_id, lvb, lvb_size, drive);
        }
        else if(strcmp(argv[1], "sync_read_group") == 0){
            ret = nvme_sync_idm_read_mutex_group(drive, &info_list, &info_num);
            printf("output: info_num=%u\n", info_num);
        }
        else if(strcmp(argv[1], "sync_read_num") == 0){
            ret = nvme_sync_idm_read_mutex_num(drive, &mutex_num);
            printf("output: mutex_num=%u\n", mutex_num);
        }
        else if(strcmp(argv[1], "sync_unlock") == 0){
            ret = nvme_sync_idm_unlock(lock_id, mode, host_id, lvb, lvb_size, drive);
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

    return ret;
}
#endif//MAIN_ACTIVATE
