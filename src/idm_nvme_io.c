/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_io.c - Contains the lowest-level function that allow the IDM In-drive Mutex (IDM)
 *                  to talk to the Linux kernel (via ioctl(), read() or write())
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/nvme_ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>       //TODO: Do I need BOTH ioctl includes?
#include <unistd.h>

#include "idm_nvme_io.h"
#include "idm_nvme_utils.h"


//////////////////////////////////////////
// COMPILE FLAGS
//////////////////////////////////////////
//TODO: Keep this (and the corresponding #ifdef's)???
#define COMPILE_STANDALONE
// #define MAIN_ACTIVATE

#define FUNCTION_ENTRY_DEBUG    //TODO: Remove this entirely???


//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * nvme_idm_write - Issues a custom (vendor-specific) NVMe write command to the IDM.
 *                  Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_write(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmVendorCmd *cmd_nvme = request_idm->cmd_nvme;
    idmData *data_idm          = request_idm->data_idm;
    int ret                    = SUCCESS;

    ret = _nvme_idm_cmd_init_wrt(request_idm);
    if(ret < 0) {
        goto EXIT_NVME_IDM_WRITE;
    }

    ret = _nvme_idm_data_init_wrt(request_idm);
    if(ret < 0) {
        goto EXIT_NVME_IDM_WRITE;
    }

    #ifndef COMPILE_STANDALONE
	ilm_log_array_dbg("resource_ver", data_idm->resource_ver, IDM_DATA_RESOURCE_VER_LEN_BYTES);
    #endif

    ret = _nvme_idm_cmd_send(request_idm);
    if(ret < 0) {
        goto EXIT_NVME_IDM_WRITE;
    }

//TODO: what do with this debug code?
    printf("%s: data_idm_write.resource_id = %s\n", __func__, data_idm->resource_id);
    printf("%s: data_idm_write.time_now = %s\n"   , __func__, data_idm->time_now);

EXIT_NVME_IDM_WRITE:
    return ret;
}

/**
 * nvme_idm_write_init - Initializes an NVMe write to the IDM by validating and then collecting all
*                        the IDM API input params and storing them in the "request_idm" data struct
                         for use later (but before the NVMe write command is sent to the OS kernel).
 *                       Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 * @cmd_nvme:       Data structure for NVMe Vendor Specific Commands.
 * @data_idm:       Data structure for sending and receiving IDM-specifc data.
 * @lock_id:        Lock ID (64 bytes).
 * @mode:           Lock mode (unlock, shareable, exclusive).
 * @host_id:        Host ID (32 bytes).
 * @drive:          Drive path name.
 * @timeout:        Timeout for membership (unit: millisecond).
 * @lvb:            Lock value block pointer.
 *                  If not used, set to 0.      //kludge
 * @lvb_size:       Lock value block size.
 *                  If not used, set to 0.      //kludge
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_write_init(nvmeIdmRequest *request_idm, nvmeIdmVendorCmd *cmd_nvme,
                        idmData *data_idm, char *lock_id, int mode, char *host_id,
                        char *drive, uint64_t timeout, char *lvb, int lvb_size) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    //TODO: change name to _nvme_idm_check_common_input() ??
    ret = _nvme_idm_write_input_check(lock_id, mode, host_id, drive, lvb_size);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: input validation fail %d", __func__, ret);
        #else
        printf("%s: input validation fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

    memset(request_idm, 0, sizeof(nvmeIdmRequest));
    memset(cmd_nvme,    0, sizeof(nvmeIdmVendorCmd));
    memset(data_idm,    0, sizeof(idmData));

    switch(mode) {
        case IDM_MODE_EXCLUSIVE:
            request_idm->class_idm = IDM_CLASS_EXCLUSIVE;
        case IDM_MODE_SHAREABLE:
            request_idm->class_idm = IDM_CLASS_SHARED_PROTECTED_READ;
        default:
//TODO: This case is the resultant default behavior of the equivalent scsi code.  Does this make sense???
//          Talk to Tom about this.
//          Feels like this should be an error
            request_idm->class_idm = mode;
    }

    request_idm->lock_id  = lock_id;
    request_idm->mode_idm = mode;
    request_idm->host_id  = host_id;
    request_idm->drive    = drive;

    request_idm->cmd_nvme = cmd_nvme;
    request_idm->data_idm = data_idm;
    request_idm->data_len = sizeof(idmData);    // Constant for NVMe writes (only) to the IDM
    request_idm->timeout  = timeout;

//TODO: IDM API dependent variables: Leave here -OR- move up 1 level?
    //kludge for inconsistent IDM API input params
    if(lvb)
        request_idm->lvb = lvb;
    if(lvb_size)
        request_idm->lvb_size  = lvb_size;

    return ret;
}

/**
 * _nvme_idm_cmd_init -  Initializes the NVMe Vendor Specific Command command struct.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 * @opcode_nvme:    NVMe-specific opcode specifying the desired NVMe action to perform on the IDM.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _nvme_idm_cmd_init(nvmeIdmRequest *request_idm, uint8_t opcode_nvme) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmVendorCmd *cmd_nvme = request_idm->cmd_nvme;
    idmData *data_idm          = request_idm->data_idm;
    int ret                    = SUCCESS;

    cmd_nvme->opcode_nvme        = opcode_nvme;
    cmd_nvme->addr               = (uint64_t)(uintptr_t)data_idm;
    cmd_nvme->data_len           = IDM_VENDOR_CMD_DATA_LEN_BYTES;  //Should be: sizeof(idmData) which should always be 512
    cmd_nvme->ndt                = IDM_VENDOR_CMD_DATA_LEN_DWORDS;
//TODO: Change spec so don't have to do this 4-bit shift
    cmd_nvme->opcode_idm_bits7_4 = request_idm->opcode_idm << 4;
    cmd_nvme->group_idm          = request_idm->group_idm;       //TODO: This isn't yet getting set anywhere for lock
    cmd_nvme->timeout_ms         = VENDOR_CMD_TIMEOUT_DEFAULT;

    return ret;
}

/**
 * _nvme_idm_cmd_init_rd - Convenience function (during an NVMe read of the IDM) for initializing
 *                         the NVMe Vendor Specific Command command struct.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
//TODO: Bring this back in when doing "read" side.
// int _nvme_idm_cmd_init_rd(nvmeIdmRequest *request_idm) {
//     return _nvme_idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_READ);
// }

/**
 * _nvme_idm_cmd_init_wrt - Convenience function (during an NVMe write of the IDM) for initializing
 *                          the NVMe Vendor Specific Command command struct.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _nvme_idm_cmd_init_wrt(nvmeIdmRequest *request_idm) {
    printf("%s: START\n", __func__);

    return _nvme_idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_WRITE);
}

/**
 * _nvme_idm_cmd_send -  Sends the completed NVMe command data structure to the OS kernel.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _nvme_idm_cmd_send(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int nvme_fd;
    int status_ioctl;
    int ret = SUCCESS;

    if ((nvme_fd = open(request_idm->drive, O_RDWR | O_NONBLOCK)) < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: error opening drive %s fd %d",
                __func__, request_idm->drive, nvme_fd);
        #else
        printf("%s: error opening drive %s fd %d\n",
                __func__, request_idm->drive, nvme_fd);
        #endif //COMPILE_STANDALONE
        return nvme_fd;
    }

    //TODO: Put this under a debug flag of some kind??
    dumpNvmeCmdStruct(request_idm->cmd_nvme, 1, 1);
    dumpIdmDataStruct(request_idm->data_idm);

    status_ioctl = ioctl(nvme_fd, NVME_IOCTL_IO_CMD, request_idm->cmd_nvme);
    if(status_ioctl) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: ioctl failed: %d", __func__, status_ioctl);
        #else
        printf("%s: ioctl failed: %d\n", __func__, status_ioctl);
        #endif //COMPILE_STANDALONE
        goto out;
    }

//TODO: Keep this debug??
    printf("%s: status_ioctl=%d\n", __func__, status_ioctl);
    printf("%s: ioctl cmd_nvme->result=%d\n", __func__, request_idm->cmd_nvme->result);

//TODO: Delete this eventually
//Completion Queue Entry (CQE) SIDE-NOTE:
//  CQE DW3[31:17] - Status Bit Field Definitions
//      DW3[31]:    Do Not Retry (DNR)
//      DW3[30]:    More (M)
//      DW3[29:28]: Command Retry Delay (CRD)
//      DW3[27:25]: Status Code Type (SCT)
//      DW3[24:17]: Status Code (SC)

//TODO: General result questions
//  Is "cmd_nvme->result" equivalent to "CQE DW0[31:0]" ?
//  Is "status_ioctl"    equivalent to "CQE DW3[31:17]" ?        //TODO:?? is "status_ioctl" just [24:17]??

//TODO: Which of the above 2 "result" params should I be using HERE??  Both??
    ret = _nvme_idm_cmd_status_check(status_ioctl, request_idm->opcode_idm);

out:
//TODO: Possible ASYNC flag HERE??  -OR-  do I need to use write() and read() for async?? (like scsi)
    // if(async) {
    //     request_idm->fd = nvme_fd;  //async, so save nvme_fd for later
    // }
    // else {
    //     close(nvme_fd);              //sunc, so done with nvme_fd.
    // }
    close(nvme_fd);
    return ret;
}

/**
 * _nvme_idm_cmd_status_check -  Checks the NVMe command status code returned from the OS kernel.
 *
 * @status:     The status code returned by the OS kernel after the completed NVMe command request.
 * @opcode_idm: IDM-specific opcode specifying the desired IDM action performed.
*
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _nvme_idm_cmd_status_check(int status, int opcode_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret;

//TODO: Unable to decipher hardcoded SCSI "Sense" data to translate to Propeller NVMe spec
    switch(status) {
        case NVME_IDM_ERR_MUTEX_OP_FAILURE:
            ret = -EINVAL;
            break;
        case NVME_IDM_ERR_MUTEX_REVERSE_OWNER_CHECK_FAILURE:
            ret = -EINVAL;
            break;
        case NVME_IDM_ERR_MUTEX_OP_FAILURE_STATE:
            ret = -EINVAL;
            break;
        case NVME_IDM_ERR_MUTEX_OP_FAILURE_CLASS:
            ret = -EINVAL;
            break;
        case NVME_IDM_ERR_MUTEX_OP_FAILURE_OWNER:
            ret = -EINVAL;
            break;
        case NVME_IDM_ERR_MUTEX_OPCODE_INVALID:
            ret = -EINVAL;
            break;
        case NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED:
            ret = -ENOMEM;
            break;
        case NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_HOST:
            ret = -ENOMEM;
            break;
        case NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_SHARED_HOST:
            ret = -ENOMEM;
            break;
        case NVME_IDM_ERR_MUTEX_CONFLICT:   //SCSI Equivalent: Reservation Conflict
            switch(opcode_idm) {
                case IDM_OPCODE_REFRESH:
                    ret = -ETIME;
                    break;
                case IDM_OPCODE_UNLOCK:
                    ret = -ENOENT;
                    break;
                default:
                    ret = -EBUSY;
            }
            break;
        case NVME_IDM_ERR_MUTEX_HELD_ALREADY:   //SCSI Equivalent: Terminated
            switch(opcode_idm) {
                case IDM_OPCODE_REFRESH:
                    ret = -EPERM;
                    break;
                case IDM_OPCODE_UNLOCK:
                    ret = -EINVAL;
                    break;
                default:
                    ret = -EAGAIN;
            }
            break;
        case NVME_IDM_ERR_MUTEX_HELD_BY_ANOTHER:    //SCSI Equivalent: Busy
            ret = -EBUSY;
        default:
            #ifndef COMPILE_STANDALONE
            ilm_log_err("%s: unknown status %d", __func__, status);
            #else
            printf("%s: unknown status %d\n", __func__, status);
            #endif //COMPILE_STANDALONE
            ret = -EINVAL;
    }

    return ret;
}

/**
 * _nvme_idm_data_init_wrt -  Initializes the IDM's data struct prior to an IDM write.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _nvme_idm_data_init_wrt(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmVendorCmd *cmd_nvme = request_idm->cmd_nvme;
    idmData *data_idm          = request_idm->data_idm;
    int ret                    = SUCCESS;

//TODO: ?? reverse bit order of next 3 destination values ??  (on scsi-side, using __bswap_64())
    #ifndef COMPILE_STANDALONE
  	data_idm->time_now  = ilm_read_utc_time();
    #else
  	data_idm->time_now  = 0;
    #endif //COMPILE_STANDALONE
	data_idm->countdown = request_idm->timeout;
	data_idm->class_idm = request_idm->class_idm;

//TODO: ?? reverse bit order of next 3 destination arrays ??  (on scsi-side, using _scsi_data_swap())
    memcpy(data_idm->host_id,      request_idm->host_id, IDM_HOST_ID_LEN_BYTES);
    memcpy(data_idm->resource_id,  request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);
    memcpy(data_idm->resource_ver, request_idm->lvb,     request_idm->lvb_size);  //TODO: On scsi-side, inconsistent use of lvb_size vs IDM_VALUE_LEN when copying lvb around
                                                                                  //TODO: Aslo, minor inefficiency. Not always needed.  Copy anyway?  Conditional IF?

	data_idm->resource_ver[0] = request_idm->res_ver_type;   //TODO: On scsi-side, why are "lvb" AND "res_ver_type" going into the same char array

    return ret;
}

/**
 * _nvme_idm_write_input_check - Common method for checking core IDM input parameters
 *
 * @lock_id:        Lock ID (64 bytes).
 * @mode:           Lock mode (unlock, shareable, exclusive).
 * @host_id:        Host ID (32 bytes).
 * @drive:          Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _nvme_idm_write_input_check(char *lock_id, int mode, char *host_id,
                                char *drive, int lvb_size) {

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

    if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
        return -EINVAL;

    if (lvb_size > IDM_LVB_SIZE_MAX)
        return -EINVAL;

    return ret;
}


#ifdef MAIN_ACTIVATE
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
            char        drive[256]  = "/dev/nvme0n1";
            uint64_t    timeout     = 10;

            //Create required input structs the IDM API would normally create
            nvmeIdmRequest   request_idm;
            nvmeIdmVendorCmd cmd_nvme;
            idmData          data_idm;
            int              ret = SUCCESS;

            ret = nvme_idm_write_init(&request_idm, &cmd_nvme, &data_idm,
                                      lock_id, mode, host_id,drive, timeout, 0, 0);
            printf("%s exiting with %d\n", argv[1], ret);

            ret = nvme_idm_write(&request_idm);
            printf("%s exiting with %d\n", argv[1], ret);
        }

    }


    return 0;
}
#endif//MAIN_ACTIVATE
