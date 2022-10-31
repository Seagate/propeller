/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_api.c - Primary NVMe interface for In-drive Mutex (IDM)
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/nvme_ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>       //TODO: Do I need BOTH ioctl includes?
#include <unistd.h>

#include "idm_nvme_api.h"
#include "idm_nvme_io.h"


//////////////////////////////////////////
// COMPILE FLAGS
//////////////////////////////////////////
//TODO: Keep this (and the corresponding #ifdef's)???
#define COMPILE_STANDALONE
#define MAIN_ACTIVATE

#define FUNCTION_ENTRY_DEBUG    //TODO: Remove this entirely???

//TODO: Should I be using malloc() instead of declaring the struct objects at the top of each idm api???

//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * nvme_idm_break_lock - Break an IDM if before other hosts have
 * acquired this IDM.  This function is to allow a host_id to take
 * over the ownership if other hosts of the IDM is timeout, or the
 * countdown value is -1UL.
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int nvme_idm_break_lock(char *lock_id, int mode, char *host_id,
                        char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0)
        return ret;

    ret = _memory_init_idm_request(&request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0)
        return ret;

    nvme_idm_write_init(lock_id, mode, host_id, drive, timeout, request_idm);

    //API-specific code
    request_idm->opcode_idm   = IDM_OPCODE_BREAK;
    request_idm->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;

    ret = nvme_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * idm_drive_convert_lock - Convert the lock mode for an IDM
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int nvme_idm_convert_lock(char *lock_id, int mode, char *host_id,
                          char *drive, uint64_t timeout)
{
    return nvme_idm_refresh_lock(lock_id, mode, host_id,
                                 drive, timeout);
}

/**
 * nvme_idm_lock - acquire an IDM on a specified NVMe drive
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @timeout:     Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_lock(char *lock_id, int mode, char *host_id,
                  char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0)
        return ret;

    ret = _memory_init_idm_request(&request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0)
        return ret;

    nvme_idm_write_init(lock_id, mode, host_id, drive, timeout, request_idm);

    //API-specific code
    request_idm->opcode_idm   = IDM_OPCODE_TRYLOCK;
    request_idm->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;

    ret = nvme_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * nvme_idm_refresh_lock - Refreshes the host's membership for an IDM
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */int nvme_idm_refresh_lock(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0)
        return ret;

    ret = _memory_init_idm_request(&request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0)
        return ret;

    nvme_idm_write_init(lock_id, mode, host_id, drive, timeout, request_idm);

    //API-specific code
    request_idm->opcode_idm   = IDM_OPCODE_REFRESH;
    request_idm->res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;

    ret = nvme_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
    return ret;
}

/**
 * idm_drive_renew_lock - Renew host's membership for an IDM
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int nvme_idm_renew_lock(char *lock_id, int mode, char *host_id,
                        char *drive, uint64_t timeout)
{
    return nvme_idm_refresh_lock(lock_id, mode, host_id,
                                 drive, timeout);
}

/**
 * idm_drive_unlock - release an IDM on a specified drive
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @lvb:        Lock value block pointer.
 * @lvb_size:   Lock value block size.
 * @drive:      Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int nvme_idm_unlock(char *lock_id, int mode, char *host_id,
                    char *lvb, int lvb_size, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest *request_idm;
    int            ret = SUCCESS;

    ret = _validate_input_write(lock_id, mode, host_id, drive);
    if (ret < 0)
        return ret;

    //TODO: The -ve check here should go away cuz lvb_size should be of unsigned type.
    //However, this requires an IDM API parameter type change.
    if ((!lvb) || (lvb_size <= 0) || (lvb_size > IDM_LVB_SIZE_MAX))
        return -EINVAL;

    ret = _memory_init_idm_request(&request_idm, DFLT_NUM_IDM_DATA_BLOCKS);
    if (ret < 0)
        return ret;

     //TODO: Why 0 timeout here (ported as-is from scsi-side)?
    nvme_idm_write_init(lock_id, mode, host_id, drive, 0, request_idm);

    //API-specific code
    request_idm->lvb          = lvb;
    request_idm->lvb_size     = lvb_size;
    request_idm->opcode_idm   = IDM_OPCODE_UNLOCK;
    request_idm->res_ver_type = IDM_RES_VER_UPDATE_NO_VALID;

    ret = nvme_idm_write(request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    _memory_free_idm_request(request_idm);
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
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _memory_init_idm_request(nvmeIdmRequest **request_idm, unsigned int data_num) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int data_len;

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
        free((*request_idm));
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: request data memory allocate fail", __func__);
        #else
        printf("%s: request data memory allocate fail\n", __func__);
        #endif //COMPILE_STANDALONE
        return -ENOMEM;
    }
    memset((*request_idm)->data_idm, 0, data_len);

    printf("%s: (*request_idm)=%u\n", __func__, (*request_idm));
    printf("%s: size=%u\n", __func__, sizeof(**request_idm));
    printf("%s: (*request_idm)->data_idm=%u\n", __func__, (*request_idm)->data_idm);
    printf("%s: size=%u\n", __func__, sizeof(*(*request_idm)->data_idm));
    // printf("%s: (*request_idm).cmd_nvme=%u\n", __func__, (*request_idm).cmd_nvme);
    // printf("%s: size=%u\n", __func__, sizeof((*request_idm)->cmd_nvme));

    //Cache params.  Not really related to func, but convenient.
    (*request_idm)->data_len = data_len;
    (*request_idm)->data_num = data_num;
    printf("%s: (*request_idm)->data_len=%u\n", __func__, (*request_idm)->data_len);

    return SUCCESS;
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

//TODO: Add general check for "mode" here? (ie: a valid enum value)

    return ret;
}

/**
 * _validate_input_common - Convenience function for validating the most common
 *                          IDM API input parameters used during an IDM write cmd.
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
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1";

//To compile:
//gcc idm_nvme_io.c idm_nvme_api.c -o idm_nvme_api
int main(int argc, char *argv[])
{
    char *drive;
    int  ret = 0;

    if(argc >= 3){
        drive = argv[2];
    }
    else {
        drive = DRIVE_DEFAULT_DEVICE;
    }

    //cli usage: idm_nvme_api lock
    if(argc >= 2){
        char        lock_id[IDM_HOST_ID_LEN_BYTES] = "lock_id";
        int         mode                           = IDM_MODE_EXCLUSIVE;
        char        host_id[IDM_LOCK_ID_LEN_BYTES] = "host_id";
        uint64_t    timeout                        = 10;
        char        lvb[IDM_LVB_SIZE_MAX]          = "lvb";
        int         lvb_size                       = 5;

        if(strcmp(argv[1], "break") == 0){
            ret = nvme_idm_break_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "convert") == 0){
            ret = nvme_idm_convert_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "lock") == 0){
            ret = nvme_idm_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "refresh") == 0){
            ret = nvme_idm_refresh_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "renew") == 0){
            ret = nvme_idm_renew_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "unlock") == 0){
            ret = nvme_idm_unlock((char*)lock_id, mode, (char*)host_id, (char*)lvb, lvb_size, drive);
        }
        else {
            printf("%s: invalid command option!\n", argv[1]);
            return -1;
        }
        printf("%s exiting with %d\n", argv[1], ret);
    }
    else{
        printf("No command option given\n");
    }

    return ret;
}
#endif//MAIN_ACTIVATE
