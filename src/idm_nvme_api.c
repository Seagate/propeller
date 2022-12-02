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


//////////////////////////////////////////
// COMPILE FLAGS
//////////////////////////////////////////
//TODO: DELETE THESE 2 (AND ALL CORRESPONDING CODE) AFTER NVME FILES COMPILE WITH THE REST OF PROPELLER.
#define COMPILE_STANDALONE
#define MAIN_ACTIVATE

#define FUNCTION_ENTRY_DEBUG    //TODO: Remove this entirely???


//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * nvme_async_idm_get_result - Retreive the result for normal async operations.

 * @handle:      NVMe request handle.
 * @result:      Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_get_result(uint64_t handle, int *result) {

    nvmeIdmRequest *request_idm = (nvmeIdmRequest *)handle;
    int ret = SUCCESS;

    ret = nvme_async_idm_data_rcv(request_idm, result);

    _memory_free_idm_request(request_idm);

    return ret;
}

/**
 * nvme_async_idm_lock - Asynchronously acquire an IDM on a specified NVMe drive.
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

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;  //TODO: Should ALL of these be initialized to FAILURE, instead of SUCCESS???  Probably going to be case-by-case

    ret = _init_lock(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock fail %d", __func__, ret);
        #else
        printf("%s: _init_lock fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        _memory_free_idm_request(request_idm);
        return ret;
    }

    *handle = (uint64_t)request_idm;
    return ret;
}

/**
 * nvme_async_idm_lock_break - Asynchronously break an IDM lock if before other
 * hosts have acquired this IDM.  This function is to allow a host_id to take
 * over the ownership if other hosts of the IDM is timeout, or the countdown
 * value is -1UL.
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

    nvmeIdmRequest *request_idm;
    int ret = SUCCESS;

    ret = _init_lock_break(lock_id, mode, host_id, drive, timeout, &request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _init_lock_break fail %d", __func__, ret);
        #else
        printf("%s: _init_lock_break fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        _memory_free_idm_request(request_idm);
        return ret;
    }

    *handle = (uint64_t)request_idm;
    return ret;
}

/**
 * nvme_async_idm_lock_convert - Asynchronously convert the lock mode for an IDM.
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
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
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
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    *handle = (uint64_t)request_idm;
    return ret;
}

/**
 * nvme_async_idm_lock_refresh - Asynchronously refreshes the host's membership for an IDM.
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
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        _memory_free_idm_request(request_idm);
        return ret;
    }

    *handle = (uint64_t)request_idm;
    return ret;
}

/**
 * nvme_async_idm_lock_renew - Asynchronously renew host's membership for an IDM.
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
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
 * nvme_async_idm_unlock - Asynchronously release an IDM on a specified drive.
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @lvb:        Lock value block pointer.
 * @lvb_size:   Lock value block size.
 * @drive:      Drive path name.
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
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_async_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_async_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_async_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    *handle = (uint64_t)request_idm;
    return ret;
}















/**
 * nvme_sync_idm_lock - Synchronously acquire an IDM on a specified NVMe drive.
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
        goto EXIT;
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
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * idm_drive_convert_lock - Synchronously convert the lock mode for an IDM.
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
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_lock_refresh - Synchronously refreshes the host's membership for an IDM.
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
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_lock_renew - Synchronously renew host's membership for an IDM.
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
 * nvme_idm_read_host_state - Read back the host's state for an specific IDM.
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @host_state: Returned host state's pointer.
 *              Referenced value set to -1 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_read_host_state(char *lock_id, char *host_id, int *host_state, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    idmData        *data_idm;
    unsigned int   mutex_num = 0;
    int            ret = SUCCESS;

    ret = _validate_input_common(lock_id, host_id, drive);
    if (ret < 0)
        return ret;

    // Initialize the output
//TODO: Does ALWAYS setting to -1 on failure make sense?
//          Refer to scsi-side if removed.
//          Was being set to -1 in a couple locations.
    *host_state = -1;    //TODO: hardcoded state. add an "error" state to the enum?

    ret = nvme_idm_read_mutex_num(drive, &mutex_num);
    if (ret < 0)
        return -ENOENT;
    else if (!mutex_num)
        return SUCCESS;

    ret = _memory_init_idm_request(&request_idm, mutex_num);
    if (ret < 0)
        return ret;

    nvme_idm_read_init(drive, request_idm);

    //API-specific code
    request_idm->group_idm = IDM_GROUP_DEFAULT;

    ret = nvme_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    //Extract value from data struct
    bswap_char_arr(request_idm->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);
    bswap_char_arr(request_idm->host_id, host_id, IDM_HOST_ID_LEN_BYTES);

    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {
        #ifndef COMPILE_STANDALONE
        ilm_log_array_dbg("resource_id",  data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("lock_id",      request->lock_id,        IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("data host_id", data_idm[i].host_id,     IDM_HOST_ID_LEN_BYTES);
        ilm_log_array_dbg("host_id",      request->host_id,        IDM_HOST_ID_LEN_BYTES);
        #endif //COMPILE_STANDALONE

        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES))
            continue;

        if (memcmp(data_idm[i].host_id, request_idm->host_id, IDM_HOST_ID_LEN_BYTES))
            continue;

        *host_state = data_idm[i].state;
        break;
    }

EXIT:
    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: host_state=%d", __func__, *host_state);
    #endif //COMPILE_STANDALONE
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_idm_read_lock_count - Read the host count for an IDM.
 * @lock_id:    Lock ID (64 bytes).
 * @host_id:    Host ID (32 bytes).
 * @count:      Returned lock count value's pointer.
 *              Referenced value set to 0 on error.
 * @self:       Returned self count value's pointer.
 *              Referenced value set to 0 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_read_lock_count(char *lock_id, char *host_id, int *count, int *self, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    idmData        *data_idm;
    unsigned int   mutex_num = 0;
    int            ret = SUCCESS;
    uint64_t       state, locked;

    ret = _validate_input_common(lock_id, host_id, drive);
    if (ret < 0)
        return ret;

    // Initialize the output
    *count = 0;
    *self = 0;

    ret = nvme_idm_read_mutex_num(drive, &mutex_num);
    if (ret < 0)
        return -ENOENT;
    else if (!mutex_num)
        return SUCCESS;

    ret = _memory_init_idm_request(&request_idm, mutex_num);
    if (ret < 0)
        return ret;

    nvme_idm_read_init(drive, request_idm);

    //API-specific code
    request_idm->group_idm = IDM_GROUP_DEFAULT;

    ret = nvme_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    //Generate counts
    bswap_char_arr(request_idm->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);
    bswap_char_arr(request_idm->host_id, host_id, IDM_HOST_ID_LEN_BYTES);

    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {

        state  = __bswap_64(data_idm[i].state);
        locked = (state == IDM_STATE_LOCKED) || (state == IDM_STATE_MULTIPLE_LOCKED);

        if (!locked)
            continue;

        #ifndef COMPILE_STANDALONE
        ilm_log_array_dbg("resource_id",  data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("lock_id",      request->lock_id,        IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("data host_id", data_idm[i].host_id,     IDM_HOST_ID_LEN_BYTES);
        ilm_log_array_dbg("host_id",      request->host_id,        IDM_HOST_ID_LEN_BYTES);
        #endif //COMPILE_STANDALONE

        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES))
            continue;

        if (memcmp(data_idm[i].host_id, request_idm->host_id, IDM_HOST_ID_LEN_BYTES)) {
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
    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: lock_count=%d, self_count=%d", __func__, *count, *self);
    #endif //COMPILE_STANDALONE
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_idm_read_lock_mode - Read back an IDM's current mode.
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Returned lock mode's pointer.
 *              Referenced value set to -1 on error.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_read_lock_mode(char *lock_id, int *mode, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    idmData        *data_idm;
    unsigned int   mutex_num = 0;
    int            ret = SUCCESS;
    uint64_t       state, class;

    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif //COMPILE_STANDALONE

    if (!lock_id || !drive)
        return -EINVAL;

    // Initialize the output
    *mode = -1;    //TODO: hardcoded state. add an "error" state to the enum?

    ret = nvme_idm_read_mutex_num(drive, &mutex_num);
    if (ret < 0)
        return -ENOENT;
    else if (!mutex_num) {
        *mode = IDM_MODE_UNLOCK;
        return SUCCESS;
    }

    ret = _memory_init_idm_request(&request_idm, mutex_num);
    if (ret < 0)
        return ret;

    nvme_idm_read_init(drive, request_idm);

    //API-specific code
    request_idm->group_idm = IDM_GROUP_DEFAULT;

    ret = nvme_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    //Find mode
    bswap_char_arr(request_idm->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);

    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {

        state = __bswap_64(data_idm[i].state);
        class = __bswap_64(data_idm[i].class);

        #ifndef COMPILE_STANDALONE
        ilm_log_dbg("%s: state=%lx class=%lx", __func__, state, class);
        #endif //COMPILE_STANDALONE

        #ifndef COMPILE_STANDALONE
        ilm_log_array_dbg("resource_id",  data_idm[i].resource_id, IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("lock_id",      request_idm->lock_id,    IDM_LOCK_ID_LEN_BYTES);
        ilm_log_array_dbg("data host_id", data_idm[i].host_id,     IDM_HOST_ID_LEN_BYTES);
        ilm_log_array_dbg("host_id",      request_idm->host_id,    IDM_HOST_ID_LEN_BYTES);
        #endif //COMPILE_STANDALONE

        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES))
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

    //If the mutex is not found in drive fimware, simply return success and mode is unlocked.
    if (*mode == -1)
        *mode = IDM_MODE_UNLOCK;

EXIT:
    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: found: lock_mode=%d", __func__, *mode);
    #endif //COMPILE_STANDALONE
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_idm_read_lvb - Read value block which is associated to an IDM.
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
int nvme_idm_read_lvb(char *lock_id, char *host_id, char *lvb, int lvb_size, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    idmData        *data_idm;
    unsigned int   mutex_num = 0;
    int            ret = SUCCESS;

    ret = _validate_input_common(lock_id, host_id, drive);
    if (ret < 0)
        return ret;

    //TODO: The -ve check below should go away cuz lvb_size should be of unsigned type.
    //However, this requires an IDM API parameter type change.
    if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
        return -EINVAL;

    // Initialize the output
    memset(lvb, 0x0, lvb_size); //TODO: Does this make sense here?? (sightly different from scsi)

    ret = nvme_idm_read_mutex_num(drive, &mutex_num);
    if (ret < 0)
        return -ENOENT;
    else if (!mutex_num)
        return SUCCESS;

    ret = _memory_init_idm_request(&request_idm, mutex_num);
    if (ret < 0)
        return ret;

    nvme_idm_read_init(drive, request_idm);

    //API-specific code
    request_idm->group_idm = IDM_GROUP_DEFAULT;

    ret = nvme_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    //Get lvb
    bswap_char_arr(request_idm->lock_id, lock_id, IDM_LOCK_ID_LEN_BYTES);
    bswap_char_arr(request_idm->host_id, host_id, IDM_HOST_ID_LEN_BYTES);

    ret = -ENOENT;
    data_idm = request_idm->data_idm;
    for (int i = 0; i < mutex_num; i++) {
        /* Skip for other locks */
        if (memcmp(data_idm[i].resource_id, request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES))
            continue;

        if (memcmp(data_idm[i].host_id, request_idm->host_id, IDM_HOST_ID_LEN_BYTES))
            continue;

        bswap_char_arr(lvb, data_idm[i].resource_ver, lvb_size);
        ret = SUCCESS;
        break;
    }

EXIT:
    #ifndef COMPILE_STANDALONE
    ilm_log_array_dbg("lvb", lvb, lvb_size);
    #endif //COMPILE_STANDALONE
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_idm_read_mutex_group - Read back mutex group for all IDM in the drives
 * @drive:      Drive path name.
 * @info_ptr:   Returned pointer for info list.
 *              Referenced pointer set to NULL on error.
 * @info_num:   Returned pointer for info num.
 *              Referenced value set to 0 on error.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_read_mutex_group(char *drive, idmInfo **info_ptr, int *info_num)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    idmData        *data_idm;
    unsigned int   mutex_num = 0;
    int            i, ret = SUCCESS;
    uint64_t       state, class;
    idmInfo        *info_list, *info;

    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif //COMPILE_STANDALONE

    if (!drive)
        return -EINVAL;

    // Initialize the output
    *info_ptr = NULL;
    *info_num = 0;

    ret = nvme_idm_read_mutex_num(drive, &mutex_num);
    if (ret < 0)
        return -ENOENT;
    else if (!mutex_num)
        return SUCCESS;

    ret = _memory_init_idm_request(&request_idm, mutex_num);
    if (ret < 0)
        return ret;

    nvme_idm_read_init(drive, request_idm);

    //API-specific code
    request_idm->group_idm = IDM_GROUP_DEFAULT;

    ret = nvme_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_read fail %d", __func__, ret);
        #else
        printf("%s: nvme_idm_read fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

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
    #ifndef COMPILE_STANDALONE
    //ilm_log_array_dbg("*info_ptr", *info_ptr, sizeof(idmInfo));
    ilm_log_dbg("%s: found: info_num=%d", __func__, *info_num);
    #endif //COMPILE_STANDALONE
    _memory_free_idm_request(request_idm);
    if (ret != 0 && info_list)
        free(info_list);
    return ret;
}

/**
 * nvme_idm_read_mutex_num - retrieves the number of mutex's present on the drive.
 * @drive:       Drive path name.
 * @mutex_num:   The number of mutex's present on the drive.
//TODO: Does ALWAYS setting to 0 on failure make sense?
 *               Set to 0 on failure.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_read_mutex_num(char *drive, unsigned int *mutex_num)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    unsigned char  *data;
    int            ret = SUCCESS;

    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif //COMPILE_STANDALONE

//TODO: Does ALWAYS setting to 0 on failure make sense?
    *mutex_num = 0;

    ret = _memory_init_idm_request(&request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0)
        return ret;

    nvme_idm_read_init(drive, request_idm);

    //API-specific code
    request_idm->group_idm = IDM_GROUP_INQUIRY;

    ret = nvme_idm_read(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_idm_read fail %d", __func__, ret);
        goto EXIT;
        #else
//TODO: This is temp debug hack code for the stand alone, to get the reads working.
        printf("%s: nvme_idm_read fail %d, Continuing\n", __func__, ret);
        ret = SUCCESS;
        #endif //COMPILE_STANDALONE
    }

    //Extract value from data struct
//TODO: Ported from scsi-side as-is.  Need to verify if this even makes sense for nvme.
    data = (unsigned char *)request_idm->data_idm;
    #ifndef COMPILE_STANDALONE
    *mutex_num = ((data[1]) & 0xff);
    *mutex_num |= ((data[0]) & 0xff) << 8;
    #else
//TODO: This can't stay. Necessary for stand-alone code
    *mutex_num = 2;     //For debug. This func called by many others.
    #endif //COMPILE_STANDALONE

    #ifndef COMPILE_STANDALONE
    ilm_log_dbg("%s: data[0]=%u data[1]=%u mutex num=%u",
                __func__, data[0], data[1], *mutex_num);
    #else
    printf("%s: data[0]=%u data[1]=%u mutex num=%u\n",
           __func__, data[0], data[1], *mutex_num);
    #endif //COMPILE_STANDALONE

EXIT:
    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_sync_idm_unlock - Synchronously release an IDM on a specified drive.
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
        _memory_free_idm_request(request_idm);
        return ret;
    }

    ret = nvme_sync_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: nvme_sync_idm_write fail %d", __func__, ret);
        #else
        printf("%s: nvme_sync_idm_write fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * _init_lock - Convenience function containing common code for the IDM lock action.
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
 * _init_unlock - Convenience function containing common code for the
 * IDM unlock action.
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

    ret = _memory_init_idm_request(request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _memory_init_idm_request fail %d", __func__, ret);
        #else
        printf("%s: _memory_init_idm_request fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //TODO: The -ve check here should go away cuz lvb_size should be of unsigned type.
    //However, this requires an IDM API parameter type change.
    if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_LEN_BYTES))
        return -EINVAL;

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
 *                            data structures used during the NVMe command sequence.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
void _memory_free_idm_request(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    free(request_idm->data_idm);
    free(request_idm);
}

/**
 * _memory_init_idm_request - Convenience function for allocating memory for all the
 *                            data structures used during the NVMe command sequence.
 *
 * @request_idm: Struct containing all NVMe-specific command info for the requested IDM action.
 *               Note: **request_idm is due to malloc() needing access to the original pointer.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _memory_init_idm_request(nvmeIdmRequest **request_idm, unsigned int data_num) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int data_len;
    int ret = SUCCESS;

//TODO: Temp code for aligned memory allocation.  Remove if not needed.
    // ret = posix_memalign((void **)request_idm, 4096, sizeof(nvmeIdmRequest));
    // if (ret) {
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
//TODO: Temp code for aligned memory allocation.  Remove if not needed.
    // ret = posix_memalign((void **)&((*request_idm)->data_idm), 4096, data_len);
    // if (ret) {
    (*request_idm)->data_idm = malloc(data_len);
    if (!(*request_idm)->data_idm) {
        free((*request_idm));
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: request data memory allocate fail", __func__);
        #else
        printf("%s: request data memory allocate fail\n", __func__);
        #endif //COMPILE_STANDALONE
        return -ENOMEM;
    }
    memset((*request_idm)->data_idm, 0, data_len);

//TODO: Remove these debug prints
    // printf("%s: nvmeIdmRequest size\n", __func__);
    // printf("%s: size=%lu\n", __func__, sizeof(nvmeIdmRequest));
    // printf("%s: (*request_idm)=%p\n", __func__, (*request_idm));
    // printf("%s: size=%lu\n", __func__, sizeof(**request_idm));
    // printf("%s: (*request_idm)->data_idm=%p\n", __func__, (*request_idm)->data_idm);
    // printf("%s: size=%lu\n", __func__, sizeof(*(*request_idm)->data_idm));

    //Cache params.  Not really related to func, but convenient.
    (*request_idm)->data_len = data_len;
    (*request_idm)->data_num = data_num;

    return ret;
}

/**
 * _validate_input_common - Convenience function for validating the most common
 *                          IDM API input parameters.
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
 *                         used during an IDM write cmd.
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
        char        lock_id[IDM_LOCK_ID_LEN_BYTES] = "0000000000000000000000000000000000000000000000000000000000000000";
        int         mode                           = IDM_MODE_EXCLUSIVE;
        char        host_id[IDM_HOST_ID_LEN_BYTES] = "00000000000000000000000000000000";
        uint64_t    timeout                        = 10;
        char        lvb[IDM_LVB_LEN_BYTES]         = "lvb";
        int         lvb_size                       = 5;
        uint64_t    handle;
        int         result;

        if(strcmp(argv[1], "get_result") == 0){
            nvme_async_idm_lock(lock_id, mode, host_id, drive, timeout, &handle); // dummy call, ignore error.
            ret = nvme_async_idm_get_result(handle, &result);
            printf("'%s' result=%d\n", argv[1], ret);
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
        else if(strcmp(argv[1], "read_state") == 0){
            int host_state;
            ret = nvme_idm_read_host_state(lock_id, host_id, &host_state, drive);
            printf("output: host_state=%d\n", host_state);
        }
        else if(strcmp(argv[1], "read_count") == 0){
            int count, self;
            ret = nvme_idm_read_lock_count(lock_id, host_id, &count, &self, drive);
            printf("output: lock_count=%d, self=%d\n", count, self);
        }
        else if(strcmp(argv[1], "read_mode") == 0){
            int mode;
            ret = nvme_idm_read_lock_mode(lock_id, &mode, drive);
            printf("output: lock_mode=%d\n", mode);
        }
        else if(strcmp(argv[1], "read_lvb") == 0){
            ret = nvme_idm_read_lvb(lock_id, host_id, lvb, lvb_size, drive);
        }
        else if(strcmp(argv[1], "read_group") == 0){
            unsigned int info_num;
            idmInfo *info_list;
            ret = nvme_idm_read_mutex_group(drive, &info_list, &info_num);
            printf("output: info_num=%u\n", info_num);
        }
        else if(strcmp(argv[1], "read_num") == 0){
            unsigned int mutex_num;
            ret = nvme_idm_read_mutex_num(drive, &mutex_num);
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
