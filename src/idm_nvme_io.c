/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_io.c - Contains the lowest-level function that allow the IDM In-drive Mutex (IDM)
 *                  to talk to the Linux kernel (via ioctl(), read() or write())
 */

#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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
//TODO: DELETE THESE 2 (AND ALL CORRESPONDING CODE) AFTER NVME FILES COMPILE WITH THE REST OF PROPELLER.
#define COMPILE_STANDALONE
// #define MAIN_ACTIVATE

#define FUNCTION_ENTRY_DEBUG    //TODO: Remove this entirely???


//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * nvme_idm_read - Issues a custom (vendor-specific) NVMe read command to the IDM.
 *                 Intended to be called by higher level read IDM API's.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_read(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _nvme_idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_READ);
    if(ret < 0) {
        return ret;
    }

    // ret = _nvme_idm_data_init_rd(request_idm);   TODO: Needed?
    // if(ret < 0) {
    //     return ret;
    // }

    ret = _nvme_idm_cmd_send(request_idm);
    if(ret < 0) {
        return ret;
    }

    return ret;
}

/**
 * nvme_idm_read_init - Initializes an NVMe reade to the IDM by validating and then collecting all
 *                      the IDM API input params and storing them in the "request_idm" data struct
 *                      for use later (but before the NVMe write command is sent to the OS kernel).
 *                      Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @drive:          Drive path name.
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 */
void nvme_idm_read_init(char *drive, nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    request_idm->opcode_idm = IDM_OPCODE_INIT;  //Ignored, but default for all idm reads.
    strncpy(request_idm->drive, drive, PATH_MAX);
}

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

    int ret = SUCCESS;

    ret = _nvme_idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_WRITE);
    if(ret < 0) {
        return ret;
    }

    ret = _nvme_idm_data_init_wrt(request_idm);
    if(ret < 0) {
        return ret;
    }

    ret = _nvme_idm_cmd_send(request_idm);
    if(ret < 0) {
        return ret;
    }

    return ret;
}

/**
 * nvme_idm_write_init - Initializes an NVMe write to the IDM by validating and then collecting all
 *                       the IDM API input params and storing them in the "request_idm" data struct
 *                       for use later (but before the NVMe write command is sent to the OS kernel).
 *                       Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @lock_id:        Lock ID (64 bytes).
 * @mode:           Lock mode (unlock, shareable, exclusive).
 * @host_id:        Host ID (32 bytes).
 * @drive:          Drive path name.
 * @timeout:        Timeout for membership (unit: millisecond).
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_write_init(char *lock_id, int mode, char *host_id, char *drive,
                        uint64_t timeout, nvmeIdmRequest *request_idm) {

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

//TODO: This is variable for NVMe reads.  How handle?  MAY be variable for writes too (future).
    request_idm->group_idm = IDM_GROUP_DEFAULT;   //Currently fixed for NVME writes

    switch(mode) {
        case IDM_MODE_EXCLUSIVE:
            request_idm->class = IDM_CLASS_EXCLUSIVE;
        case IDM_MODE_SHAREABLE:
            request_idm->class = IDM_CLASS_SHARED_PROTECTED_READ;
        default:
            //Other modes not supported at this time
            return -EINVAL;
    }

    return SUCCESS;
}

/**
 * _nvme_idm_cmd_check_status -  Checks the NVMe command status code returned from the OS kernel.
 *
 * @status:     The status code returned by the OS kernel after the completed NVMe command request.
 * @opcode_idm: IDM-specific opcode specifying the desired IDM action performed.
*
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _nvme_idm_cmd_check_status(int status, int opcode_idm) {

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

    nvmeIdmVendorCmd *cmd_nvme = &request_idm->cmd_nvme;
    int ret                    = SUCCESS;

    cmd_nvme->opcode_nvme        = opcode_nvme;
    cmd_nvme->addr               = (uint64_t)(uintptr_t)request_idm->data_idm;
    cmd_nvme->data_len           = request_idm->data_len;
    cmd_nvme->ndt                = request_idm->data_len / 4;
//TODO: Change spec so don't have to do this 4-bit shift
    cmd_nvme->opcode_idm_bits7_4 = request_idm->opcode_idm << 4;
    cmd_nvme->group_idm          = request_idm->group_idm;
    cmd_nvme->timeout_ms         = VENDOR_CMD_TIMEOUT_DEFAULT;

    return ret;
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

    struct nvme_passthru_cmd cmd_nvme_passthru;
    struct nvme_passthru_cmd *c = &cmd_nvme_passthru;
    int nvme_fd;
    int status_ioctl;
    int ret = SUCCESS;

    //TODO: Put this under a debug flag of some kind??
    dumpNvmeCmdStruct(&request_idm->cmd_nvme, 1, 1);
    dumpIdmDataStruct(request_idm->data_idm);

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

    memset(&cmd_nvme_passthru, 0, sizeof(struct nvme_passthru_cmd));

    //TODO: Leave this commentted out section for nsid in-place for now.  May be needed in near future.
    // nsid_ioctl = ioctl(nvme_fd, NVME_IOCTL_ID);
    // if (nsid_ioctl <= 0)
    // {
    //     printf("%s: nsid ioctl fail: %d\n", __func__, nsid_ioctl);
    //     return nsid_ioctl;
    // }

    cmd_nvme_passthru.opcode       = request_idm->cmd_nvme.opcode_nvme;
    cmd_nvme_passthru.flags        = request_idm->cmd_nvme.flags;
    cmd_nvme_passthru.rsvd1        = request_idm->cmd_nvme.command_id;
    cmd_nvme_passthru.nsid         = request_idm->cmd_nvme.nsid;
    cmd_nvme_passthru.cdw2         = request_idm->cmd_nvme.cdw2;
    cmd_nvme_passthru.cdw3         = request_idm->cmd_nvme.cdw3;
    cmd_nvme_passthru.metadata     = request_idm->cmd_nvme.metadata;
    cmd_nvme_passthru.addr         = request_idm->cmd_nvme.addr;
    cmd_nvme_passthru.metadata_len = request_idm->cmd_nvme.metadata_len;
    cmd_nvme_passthru.data_len     = request_idm->cmd_nvme.data_len;
    cmd_nvme_passthru.cdw10        = request_idm->cmd_nvme.ndt;
    cmd_nvme_passthru.cdw11        = request_idm->cmd_nvme.ndm;
    cmd_nvme_passthru.cdw12        = ((uint32_t)request_idm->cmd_nvme.rsvd2 << 16) |
                                     ((uint32_t)request_idm->cmd_nvme.group_idm << 8) |
                                     (uint32_t)request_idm->cmd_nvme.opcode_idm_bits7_4;
    cmd_nvme_passthru.cdw13        = request_idm->cmd_nvme.cdw13;
    cmd_nvme_passthru.cdw14        = request_idm->cmd_nvme.cdw14;
    cmd_nvme_passthru.cdw15        = request_idm->cmd_nvme.cdw15;
    cmd_nvme_passthru.timeout_ms   = request_idm->cmd_nvme.timeout_ms;

    //TODO: Keep?  Refactor into debug func?
    printf("nvme_passthru_cmd Struct: Fields\n");
    printf("================================\n");
    printf("opcode_nvme  (CDW0[ 7:0])  = 0x%.2X (%u)\n", c->opcode,       c->opcode);
    printf("flags        (CDW0[15:8])  = 0x%.2X (%u)\n", c->flags,        c->flags);
    printf("rsvd1        (CDW0[32:16]) = 0x%.4X (%u)\n", c->rsvd1,        c->rsvd1);
    printf("nsid         (CDW1[32:0])  = 0x%.8X (%u)\n", c->nsid,         c->nsid);
    printf("cdw2         (CDW2[32:0])  = 0x%.8X (%u)\n", c->cdw2,         c->cdw2);
    printf("cdw3         (CDW3[32:0])  = 0x%.8X (%u)\n", c->cdw3,         c->cdw3);
    printf("metadata     (CDW5&4[64:0])= 0x%.16llX (%llu)\n",c->metadata, c->metadata);
    printf("addr         (CDW7&6[64:0])= 0x%.16llX (%llu)\n",c->addr,     c->addr);
    printf("metadata_len (CDW8[32:0])  = 0x%.8X (%u)\n", c->metadata_len, c->metadata_len);
    printf("data_len     (CDW9[32:0])  = 0x%.8X (%u)\n", c->data_len,     c->data_len);
    printf("cdw10        (CDW10[32:0]) = 0x%.8X (%u)\n", c->cdw10,        c->cdw10);
    printf("cdw11        (CDW11[32:0]) = 0x%.8X (%u)\n", c->cdw11,        c->cdw11);
    printf("cdw12        (CDW12[32:0]) = 0x%.8X (%u)\n", c->cdw12,        c->cdw12);
    printf("cdw13        (CDW13[32:0]) = 0x%.8X (%u)\n", c->cdw13,        c->cdw13);
    printf("cdw14        (CDW14[32:0]) = 0x%.8X (%u)\n", c->cdw14,        c->cdw14);
    printf("cdw15        (CDW15[32:0]) = 0x%.8X (%u)\n", c->cdw15,        c->cdw15);
    printf("timeout_ms   (CDW16[32:0]) = 0x%.8X (%u)\n", c->timeout_ms,   c->timeout_ms);
    printf("result       (CDW17[32:0]) = 0x%.8X (%u)\n", c->result,       c->result);
    printf("\n");

    status_ioctl = ioctl(nvme_fd, NVME_IOCTL_ADMIN_CMD, &cmd_nvme_passthru);
    if(status_ioctl) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: ioctl failed: %d", __func__, status_ioctl);
        #else
        printf("%s: ioctl failed: %d\n", __func__, status_ioctl);
        printf("%s: ioctl cmd_nvme_passthru.result=%d\n", __func__, cmd_nvme_passthru.result);
        #endif //COMPILE_STANDALONE
        return status_ioctl;
    }

    printf("%s: status_ioctl=%d\n", __func__, status_ioctl);
    printf("%s: ioctl cmd_nvme_passthru.result=%d\n", __func__, cmd_nvme_passthru.result);

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

    ret = _nvme_idm_cmd_check_status(status_ioctl, request_idm->opcode_idm);

out:
    close(nvme_fd);
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

    nvmeIdmVendorCmd *cmd_nvme = &request_idm->cmd_nvme;
    idmData *data_idm          = request_idm->data_idm;
    int ret                    = SUCCESS;

    #ifndef COMPILE_STANDALONE
  	data_idm->time_now  = __bswap_64(ilm_read_utc_time());
    #else
  	data_idm->time_now  = __bswap_64(1234567890);
    #endif //COMPILE_STANDALONE
    data_idm->countdown = __bswap_64(request_idm->timeout);
    data_idm->class     = __bswap_64(request_idm->class);

    bswap_char_arr(data_idm->host_id,      request_idm->host_id, IDM_HOST_ID_LEN_BYTES);
    bswap_char_arr(data_idm->resource_id,  request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);
    bswap_char_arr(data_idm->resource_ver, request_idm->lvb    , IDM_LVB_LEN_BYTES);
    data_idm->resource_ver[0] = request_idm->res_ver_type;

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
            nvmeIdmRequest *request_idm;
            int            ret = SUCCESS;

            ret = nvme_idm_write_init(lock_id, mode, host_id,drive, timeout, 0, 0,
                                      request_idm);
            printf("%s exiting with %d\n", argv[1], ret);

            ret = nvme_idm_write(request_idm);
            printf("%s exiting with %d\n", argv[1], ret);
        }

    }


    return 0;
}
#endif//MAIN_ACTIVATE
