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
 * idm_nvme_drive_lock - acquire an IDM on a specified NVMe drive
 * @lock_id:     Lock ID (64 bytes).
 * @mode:        Lock mode (unlock, shareable, exclusive).
 * @host_id:     Host ID (32 bytes).
 * @drive:       Drive path name.
 * @timeout:     Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_nvme_drive_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout) {

    printf("%s: START\n", __func__);

    nvmeIdmRequest   request_idm;
    nvmeIdmVendorCmd cmd_idm;
    idmData          data_idm;
    int              ret = SUCCESS;

//TODO: Common function for init error checking???
    #ifndef COMPILE_STANDALONE
    if (ilm_inject_fault_is_hit())
        return -EIO;
    #endif

    if (!lock_id || !host_id || !drive)
        return -EINVAL;

    if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
        return -EINVAL;

    nvme_idm_write_init(&request_idm, &cmd_idm, &data_idm,
                        lock_id, mode, host_id,drive, timeout, 0, 0);

//TODO: Remove this comment
	// if (mode == IDM_MODE_EXCLUSIVE)
	// 	mode = IDM_CLASS_EXCLUSIVE;
	// else if (mode == IDM_MODE_SHAREABLE)
	// 	mode = IDM_CLASS_SHARED_PROTECTED_READ;

//Start API-specific settings
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
    request_idm.data_len     = sizeof(idmData);         //TODO: review this.  This can happen later

    ret = nvme_idm_write(&request_idm);

    return ret;
}




#ifdef MAIN_ACTIVATE
/*#########################################################################################
########################### STAND-ALONE MAIN ##############################################
#########################################################################################*/
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1";

//To compile:
//gcc idm_nvme_api.c -o idm_nvme_api
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
        if(strcmp(argv[1], "lock") == 0){
            char        lock_id[64] = "lock_id";
            int         mode        = IDM_MODE_EXCLUSIVE;
            char        host_id[32] = "host_id";
            uint64_t    timeout     = 10;
            ret = idm_nvme_drive_lock((char*)lock_id, mode, (char*)host_id, drive, timeout);
            printf("%s exiting with %d\n", argv[1], ret);
        }

    }

    return 0;
}
#endif//MAIN_ACTIVATE
