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
 * idm_nvme_drive_break_lock - Break an IDM if before other hosts have
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
int idm_nvme_drive_break_lock(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_nvme;
    idmData          data_idm;
    int              ret = SUCCESS;

    ret = nvme_idm_write_init(&request_idm, &cmd_nvme, &data_idm, lock_id,
                              mode, host_id, drive, timeout, 0, 0);
    if(ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: fail %d", __func__, ret);
        #else
        printf("%s: fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    request_idm.opcode_idm   = IDM_OPCODE_BREAK;
    request_idm.res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;

    ret = nvme_idm_write(&request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }
}

//TODO: should this function be deprecated\removed???
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
int idm_nvme_drive_convert_lock(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t timeout)
{
    return idm_nvme_drive_refresh_lock(lock_id, mode, host_id,
                                       drive, timeout);
}

/**
 * idm_nvme_drive_lock - acquire an IDM on a specified NVMe drive
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @timeout:     Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_nvme_drive_lock(char *lock_id, int mode, char *host_id,
                        char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_nvme;
    idmData          data_idm;
    int              ret = SUCCESS;

    ret = nvme_idm_write_init(&request_idm, &cmd_nvme, &data_idm, lock_id,
                              mode, host_id, drive, timeout, 0, 0);
    if(ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: fail %d", __func__, ret);
        #else
        printf("%s: fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    request_idm.opcode_idm   = IDM_OPCODE_TRYLOCK;
    request_idm.res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;

    ret = nvme_idm_write(&request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    return ret;
}

/**
 * idm_nvme_drive_refresh_lock - Refreshes the host's membership for an IDM
 * @lock_id:    Lock ID (64 bytes).
 * @mode:       Lock mode (unlock, shareable, exclusive).
 * @host_id:    Host ID (32 bytes).
 * @drive:      Drive path name.
 * @timeout:    Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */static int idm_nvme_drive_refresh_lock(char *lock_id, int mode, char *host_id,
                                       char *drive, uint64_t timeout)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_nvme;
    idmData          data_idm;
    int              ret = SUCCESS;

    ret = nvme_idm_write_init(&request_idm, &cmd_nvme, &data_idm, lock_id,
                              mode, host_id, drive, timeout, 0, 0);
    if(ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: fail %d", __func__, ret);
        #else
        printf("%s: fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    request_idm.opcode_idm   = IDM_OPCODE_REFRESH;
    request_idm.res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;

    ret = nvme_idm_write(&request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    return ret;
}

//TODO: should this function be deprecated\removed???
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
int idm_nvme_drive_renew_lock(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout)
{
    return idm_nvme_drive_refresh_lock(lock_id, mode, host_id,
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
int idm_nvme_drive_unlock(char *lock_id, int mode, char *host_id,
                          char *lvb, int lvb_size, char *drive)
{
    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_nvme;
    idmData          data_idm;
    int              ret = SUCCESS;

     //TODO: Why 0 timeout here (ported as-is from scsi-side)?
    ret = nvme_idm_write_init(&request_idm, &cmd_nvme, &data_idm, lock_id,
                              mode, host_id, drive, 0, lvb, lvb_size);
    if(ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: fail %d", __func__, ret);
        #else
        printf("%s: fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    request_idm.opcode_idm   = IDM_OPCODE_UNLOCK;
    request_idm.res_ver_type = IDM_RES_VER_UPDATE_NO_VALID;

    ret = nvme_idm_write(&request_idm);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: command fail %d", __func__, ret);
        #else
        printf("%s: command fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

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
            ret = idm_nvme_drive_break_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "lock") == 0){
            ret = idm_nvme_drive_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "refresh") == 0){
            ret = idm_nvme_drive_refresh_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "unlock") == 0){
            ret = idm_nvme_drive_unlock((char*)lock_id, mode, (char*)host_id, (char*)lvb, lvb_size, drive);
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
