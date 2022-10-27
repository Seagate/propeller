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
int idm_nvme_drive_break_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout)
{
    printf("%s: START\n", __func__);

//TODO: Should I be using malloc() instead?
    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_idm;
    idmData          data_idm;
    int              ret = SUCCESS;

    ret = nvme_idm_write_init(&request_idm, &cmd_idm, &data_idm,
                              lock_id, mode, host_id, drive, timeout, 0, 0);
    if(ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: fail %d", __func__, ret);
        #else
        printf("%s: fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    switch(mode) {
        case IDM_MODE_EXCLUSIVE:
            request_idm.class_idm = IDM_CLASS_EXCLUSIVE;
        case IDM_MODE_SHAREABLE:
            request_idm.class_idm = IDM_CLASS_SHARED_PROTECTED_READ;
        default:
//TODO: This case is the resultant default behavior of the equivalent scsi code.  Does this make sense???
//          Talk to Tom about this.
//          Feels like this should be an error
            request_idm.class_idm = mode;
    }

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
int idm_nvme_drive_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout)
{
    printf("%s: START\n", __func__);

//TODO: Should I be using malloc() instead?
    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_idm;
    idmData          data_idm;
    int              ret = SUCCESS;

    ret = nvme_idm_write_init(&request_idm, &cmd_idm, &data_idm,
                              lock_id, mode, host_id, drive, timeout, 0, 0);
    if(ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: fail %d", __func__, ret);
        #else
        printf("%s: fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    switch(mode) {
        case IDM_MODE_EXCLUSIVE:
            request_idm.class_idm = IDM_CLASS_EXCLUSIVE;
        case IDM_MODE_SHAREABLE:
            request_idm.class_idm = IDM_CLASS_SHARED_PROTECTED_READ;
        default:
//TODO: This case is the resultant default behavior of the equivalent scsi code.  Does this make sense???
//          Talk to Tom about this.
//          Feels like this should be an error
            request_idm.class_idm = mode;
    }

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



//TODO: docstring
static int idm_nvme_drive_refresh_lock(char *lock_id, int mode, char *host_id,
                                       char *drive, uint64_t timeout)
{
    printf("%s: START\n", __func__);

//TODO: Should I be using malloc() instead?
    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_idm;
    idmData          data_idm;
    int              ret = SUCCESS;

    ret = nvme_idm_write_init(&request_idm, &cmd_idm, &data_idm,
                              lock_id, mode, host_id, drive, timeout, 0, 0);
    if(ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: fail %d", __func__, ret);
        #else
        printf("%s: fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    //API-specific code
    switch(mode) {              //TODO: Can this be push down into the common init for ALL idm write sync apis??
        case IDM_MODE_EXCLUSIVE:
            request_idm.class_idm = IDM_CLASS_EXCLUSIVE;
        case IDM_MODE_SHAREABLE:
            request_idm.class_idm = IDM_CLASS_SHARED_PROTECTED_READ;
        default:
//TODO: This case is the resultant default behavior of the equivalent scsi code.  Does this make sense???
//          Talk to Tom about this.
//          Feels like this should be an error
            request_idm.class_idm = mode;
    }

    request_idm.opcode_idm   = IDM_OPCODE_REFRESH;
    request_idm.res_ver_type = IDM_RES_VER_NO_UPDATE_NO_VALID;  //TODO: Can this be push down into the common init for ALL idm write sync apis??

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
















// /**
//  * idm_drive_unlock - release an IDM on a specified drive
//  * @lock_id:    Lock ID (64 bytes).
//  * @mode:       Lock mode (unlock, shareable, exclusive).
//  * @host_id:    Host ID (32 bytes).
//  * @lvb:        Lock value block pointer.
//  * @lvb_size:   Lock value block size.
//  * @drive:      Drive path name.
//  *
//  * Returns zero or a negative error (ie. EINVAL, ETIME).
//  */
// int idm_drive_unlock(char *lock_id, int mode, char *host_id,
//              char *lvb, int lvb_size, char *drive)
// {
//     struct idm_scsi_request *request;
//     int ret;

//     if (ilm_inject_fault_is_hit())
//         return -EIO;

//     if (!lock_id || !host_id || !drive)
//         return -EINVAL;

//     if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
//         return -EINVAL;

//     if (lvb_size > IDM_VALUE_LEN)
//         return -EINVAL;

//     request = malloc(sizeof(struct idm_scsi_request));
//     if (!request) {
//         ilm_log_err("%s: fail to allocat scsi request", __func__);
//         return -ENOMEM;
//     }
//     memset(request, 0x0, sizeof(struct idm_scsi_request));

//     request->data = malloc(sizeof(struct idm_data));
//     if (!request->data) {
//         free(request);
//         ilm_log_err("%s: fail to allocat scsi data", __func__);
//         return -ENOMEM;
//     }
//     memset(request->data, 0x0, sizeof(struct idm_data));

//     if (mode == IDM_MODE_EXCLUSIVE)
//         mode = IDM_CLASS_EXCLUSIVE;
//     else if (mode == IDM_MODE_SHAREABLE)
//         mode = IDM_CLASS_SHARED_PROTECTED_READ;

//     strncpy(request->drive, drive, PATH_MAX);
//     request->op = IDM_MUTEX_OP_UNLOCK;
//     request->mode = mode;
//     request->timeout = 0;
//     request->data_len = sizeof(struct idm_data);
//     request->res_ver_type = IDM_RES_VER_UPDATE_NO_VALID;
//     memcpy(request->lock_id, lock_id, IDM_LOCK_ID_LEN);
//     memcpy(request->host_id, host_id, IDM_HOST_ID_LEN);
//     memcpy(request->lvb, lvb, lvb_size);

//     ret = _scsi_xfer_sync(request);
//     if (ret < 0)
//         ilm_log_err("%s: command fail %d", __func__, ret);

//     free(request->data);
//     free(request);
//     return ret;
// }









// /**
//  * idm_drive_convert_lock - Convert the lock mode for an IDM
//  * @lock_id:    Lock ID (64 bytes).
//  * @mode:       Lock mode (unlock, shareable, exclusive).
//  * @host_id:    Host ID (32 bytes).
//  * @drive:      Drive path name.
//  * @timeout:    Timeout for membership (unit: millisecond).
//  *
//  * Returns zero or a negative error (ie. EINVAL, ETIME).
//  */
// int idm_drive_convert_lock(char *lock_id, int mode, char *host_id,
//                char *drive, uint64_t timeout)
// {
//     return idm_drive_refresh_lock(lock_id, mode, host_id,
//                       drive, timeout);
// }



// /**
//  * idm_drive_renew_lock - Renew host's membership for an IDM
//  * @lock_id:    Lock ID (64 bytes).
//  * @mode:       Lock mode (unlock, shareable, exclusive).
//  * @host_id:    Host ID (32 bytes).
//  * @drive:      Drive path name.
//  * @timeout:    Timeout for membership (unit: millisecond).
//  *
//  * Returns zero or a negative error (ie. EINVAL, ETIME).
//  */
// int idm_drive_renew_lock(char *lock_id, int mode, char *host_id,
//              char *drive, uint64_t timeout)
// {
//     return idm_drive_refresh_lock(lock_id, mode, host_id,
//                       drive, timeout);
// }




























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
            char        lock_id[64] = "lock_id";
            int         mode        = IDM_MODE_EXCLUSIVE;
            char        host_id[32] = "host_id";
            uint64_t    timeout     = 10;
        if(strcmp(argv[1], "break") == 0){
            ret = idm_nvme_drive_break_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "lock") == 0){
            ret = idm_nvme_drive_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        else if(strcmp(argv[1], "refresh") == 0){
            ret = idm_nvme_drive_refresh_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
        }
        printf("%s exiting with %d\n", argv[1], ret);
    }
    else{
        printf("No command option given\n");
    }

    return ret;
}
#endif//MAIN_ACTIVATE
