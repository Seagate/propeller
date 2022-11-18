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
 * nvme_async_idm_data_rcv - Asynchronously retrieves the status word and (as needed) data from
 * a specified device for a previously issued NVMe IDM command.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 * @result:         Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_data_rcv(nvmeIdmRequest *request_idm, int *result) {

    int ret = SUCCESS;

    //TODO: This is wrong.  SCSI specifies "direction" on the bus, NVMe doesn't have that.
    //      It uses opcode_nvme to specify that.  HOWEVER, THAT is NOT getting reset here.
    //      The SCSI code re-uses the exact same request obejct WITHOUT ANY MODIFICATIONS
    //      to retrieve the async status code.  While the SCSI code updates sg_io_hdr_t "direction" bit,
    //      during the result retrieval, it writes the same direction value that is already present.
    //      Does NVMe need to reset the opcode_nvme to 0xC2 (idm read) to get status (or returned
    //      data for that matter)???
    ret = _async_idm_data_rcv(request_idm, result);
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: _async_idm_data_rcv fail %d", __func__, ret);
        #else
        printf("%s: _async_idm_data_rcv fail %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
    }

    if (request_idm->fd_nvme)
        close(request_idm->fd_nvme);

    return ret;
}

/**
 * nvme_async_idm_read - Issues a custom (vendor-specific) NVMe command to the drive that then
 * executes a READ action on the IDM.  The specific action performed on the IDM is defined
 * by the IDM opcode set within the NVMe command structure.
 * However, this function issues this NVMe command asychronously.  Therefore, a separate
 * async command must be called to determine the success\failure of the specified IDM opcode.
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_read(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_READ);
    if (ret < 0) {
        return ret;
    }

    // ret = _idm_data_init_rd(request_idm);   TODO: Needed?
    // if (ret < 0) {
    //     return ret;
    // }

    ret = _async_idm_cmd_send(request_idm);
    if (ret < 0) {
        return ret;
    }

    return ret;
}

/**
 * nvme_async_idm_write - Issues a custom (vendor-specific) NVMe command to the drive that then
 * executes a WRITE action on the IDM.  The specific action performed on the IDM is defined
 * by the IDM opcode set within the NVMe command structure.
 * However, this function issues this NVMe command asychronously.  Therefore, a separate
 * async command must be called to determine the success\failure of the specified IDM opcode.
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_async_idm_write(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

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
 * nvme_idm_read_init - Initializes an NVMe READ to the IDM by validating and then collecting
 * all the IDM API input params and storing them in the "request_idm" data struct for use later
 * (but before the NVMe write command is sent to the OS kernel).
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
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
 * nvme_idm_write_init - Initializes an NVMe WRITE to the IDM by validating and then collecting
 * all the IDM API input params and storing them in the "request_idm" data struct for use later
 * (but before the NVMe write command is sent to the OS kernel).
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
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
 * nvme_sync_idm_read - Issues a custom (vendor-specific) NVMe command to the drive that then
 * executes a READ action on the IDM.  The specific action performed on the IDM is defined
 * by the IDM opcode set within the NVMe command structure.
 * Intended to be called by higher level read IDM API's.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_read(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

    ret = _idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_READ);
    if (ret < 0) {
        return ret;
    }

    // ret = _idm_data_init_rd(request_idm);   TODO: Needed?
    // if (ret < 0) {
    //     return ret;
    // }

    ret = _sync_idm_cmd_send(request_idm);
    if (ret < 0) {
        return ret;
    }

    return ret;
}

/**
 * nvme_sync_idm_write - Issues a custom (vendor-specific) NVMe command to the device that then
 * executes a WRITE action on the IDM.  The specific action performed on the IDM is defined
 * by the IDM opcode set within the NVMe command structure.
 * Intended to be called by higher level IDM API's (i.e.: lock, unlock, etc).
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_sync_idm_write(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

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
 * _async_idm_cmd_send - Forms and then sends an NVMe IDM command to the system device,
 * but does so asynchronously.
 * Does so by transfering data from the "request_idm" struct to the final "nvme_passthru_cmd"
 * struct used by the system.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _async_idm_cmd_send(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    struct nvme_passthru_cmd cmd_nvme_passthru;
    int fd_nvme;
    int nsid;
    int ret = SUCCESS;

    //TODO: Put this under a debug flag of some kind??
    dumpNvmeCmdStruct(&request_idm->cmd_nvme, 1, 1);
    dumpIdmDataStruct(request_idm->data_idm);

    if ((fd_nvme = open(request_idm->drive, O_RDWR | O_NONBLOCK)) < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: error opening drive %s fd %d", __func__, request_idm->drive, fd_nvme);
        #else
        printf("%s: error opening drive %s fd %d\n", __func__, request_idm->drive, fd_nvme);
        #endif //COMPILE_STANDALONE
        return fd_nvme;
    }

    //TODO: !!! Does anything need to be added\changed to cmd_nvme_passthru for use in write()?!!!

    nsid = ioctl(fd_nvme, NVME_IOCTL_ID);
    if (nsid <= 0)
    {
        printf("%s: nsid ioctl fail: %d\n", __func__, nsid);
        return nsid;
    }
    request_idm->cmd_nvme.nsid = nsid;

    memset(&cmd_nvme_passthru, 0, sizeof(struct nvme_passthru_cmd));

    // Transfer all the data in the "request" structure to the prefined system NVMe
    // command structure used by the system commands (like ioctl())
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

    //TODO: Keep?  Add debug flag?
    dumpNvmePassthruCmd(&cmd_nvme_passthru);

    //TODO: Don't know exactly how to do this async communication yet via NVMe.
    //      This write() call is just a duplication of what scsi is doing
    // ret = write(fd_nvme, &cmd_nvme_passthru, sizeof(cmd_nvme_passthru));
    if (ret) {
        close(fd_nvme);
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: write failed: %d", __func__, ret);
        #else
        printf("%s: write failed: %d\n", __func__, ret, ret);
        #endif //COMPILE_STANDALONE
        return ret;
    }

EXIT:
    request_idm->fd_nvme = fd_nvme;  //async, so save fd_nvme for later
    return ret;
}

/**
 * _async_idm_data_rcv - Asynchronously retrieves the status word and (as needed) data from
 * a specified device for a previously issued NVMe IDM command.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 * @result:         Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _async_idm_data_rcv(nvmeIdmRequest *request_idm, int *result) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    struct nvme_passthru_cmd cmd_nvme_passthru;
    int nsid;
    int status_async_cmd;    //The status of the PREVIOUSLY issued async cmd
    int ret = SUCCESS;

    //TODO: Put this under a debug flag of some kind??
    dumpNvmeCmdStruct(&request_idm->cmd_nvme, 1, 1);
    dumpIdmDataStruct(request_idm->data_idm);

    if (!request_idm->fd_nvme) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: invalid device handle\n", __func__);
        #else
        printf("%s: invalid device handle\n", __func__);
        #endif //COMPILE_STANDALONE
        ret = FAILURE;
        goto EXIT;
    }

    //TODO: !!! Does anything need to be added\changed to cmd_nvme_passthru for use in read()?!!!

    //TODO: This MAY be redundant, IF the async request is NOT modified (from when it was originally sent)
    //          Although, we could use it as a check here to make sure that the fd_nvme is for the SAME device
    //              ie - if (!nsid == request_idm->cmd_nvme.nsid) {return -1;}
    nsid = ioctl(request_idm->fd_nvme, NVME_IOCTL_ID);
    if (nsid <= 0)
    {
        printf("%s: nsid ioctl fail: %d\n", __func__, nsid);
        return nsid;
    }
    request_idm->cmd_nvme.nsid = nsid;

    memset(&cmd_nvme_passthru, 0, sizeof(struct nvme_passthru_cmd));

    // Transfer all the data in the "request" structure to the prefined system NVMe
    // command structure used by the system commands (like ioctl())
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

    //TODO: Keep?  Add debug flag?
    dumpNvmePassthruCmd(&cmd_nvme_passthru);

    //TODO: Don't know exactly how to do this async communication yet via NVMe.
    //      This read() call is just a duplication of what scsi is doing
    // ret = read(request_idm->fd_nvme, &cmd_nvme_passthru, sizeof(cmd_nvme_passthru));
    if (ret < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: read failed: %d", __func__, ret);
        #else
        printf("%s: read failed: %d\n", __func__, ret);
        #endif //COMPILE_STANDALONE
        goto EXIT;
    }

    //TODO: Review _scsi_read() code for this part here

    //TODO: Where does "status_async_cmd" come from in NVMe?   cmd_nvme_passthru.result?? Somewhere in data_idm (.addr)?
    //      There is no NVMe equivalent to:
    //              1.) SCSI's "io_hdr.info & SG_INFO_OK_MASK" check, which determines Pass\Fail of the PREVIOUS async cmd, OR
    //              2.) SCSI's "io_hdr.masked_status", which, on Fail, is used to determine the final "result" (ie - error\return code) from the PREVIOUS async cmd
    // status_async_cmd = ?????????????????????????????????????????????????????????????????????????????

    //TODO: Broken until it's determined how to retrieve the P\F of PREVIOUS async cmd AND where the status code is passed back.
    *result = _idm_cmd_check_status(status_async_cmd, request_idm->opcode_idm);

    //TODO: Review these.
    printf("%s: async result=%d\n", __func__, *result);
    printf("%s: write cmd_nvme_passthru.result=%d\n", __func__, cmd_nvme_passthru.result);

EXIT:
    return ret;
}

/**
 * _idm_cmd_check_status -  Evaluates the NVMe command status word returned from a device.
 *
 * @status:     The status code returned by a system device after the completed NVMe command request.
 * @opcode_idm: IDM-specific opcode specifying the desired IDM action performed by the device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _idm_cmd_check_status(int status, int opcode_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    int ret = SUCCESS;

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
 * _idm_cmd_init -  Initializes the NVMe Vendor Specific Command command struct.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 * @opcode_nvme:    NVMe-specific opcode specifying the desired NVMe action to perform on the IDM.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _idm_cmd_init(nvmeIdmRequest *request_idm, uint8_t opcode_nvme) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    nvmeIdmVendorCmd *cmd_nvme = &request_idm->cmd_nvme;
    int ret = SUCCESS;

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
 * _idm_data_init_wrt -  Initializes the IDM's data struct prior to an IDM write.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _idm_data_init_wrt(nvmeIdmRequest *request_idm) {

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

/**
 * _sync_idm_cmd_send - Forms and then sends an NVMe IDM command to the system device.
 * Does so by transfering data from the "request_idm" struct to the final "nvme_passthru_cmd"
 * struct used by the system.
 *
 * @request_idm:    Struct containing all NVMe-specific command info for the requested IDM action.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int _sync_idm_cmd_send(nvmeIdmRequest *request_idm) {

    #ifdef FUNCTION_ENTRY_DEBUG
    printf("%s: START\n", __func__);
    #endif //FUNCTION_ENTRY_DEBUG

    struct nvme_passthru_cmd cmd_nvme_passthru;
    int fd_nvme;
    int nsid;
    int status_ioctl;
    int ret = SUCCESS;

    //TODO: Put this under a debug flag of some kind??
    dumpNvmeCmdStruct(&request_idm->cmd_nvme, 1, 1);
    dumpIdmDataStruct(request_idm->data_idm);

    if ((fd_nvme = open(request_idm->drive, O_RDWR | O_NONBLOCK)) < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: error opening drive %s fd %d", __func__, request_idm->drive, fd_nvme);
        #else
        printf("%s: error opening drive %s fd %d\n", __func__, request_idm->drive, fd_nvme);
        #endif //COMPILE_STANDALONE
        return fd_nvme;
    }

    nsid = ioctl(fd_nvme, NVME_IOCTL_ID);
    if (nsid <= 0)
    {
        printf("%s: nsid ioctl fail: %d\n", __func__, nsid);
        return nsid;
    }
    request_idm->cmd_nvme.nsid = nsid;

    memset(&cmd_nvme_passthru, 0, sizeof(struct nvme_passthru_cmd));

    // Transfer all the data in the "request" structure to the prefined system NVMe
    // command structure used by the system commands (like ioctl())
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

    //TODO: Keep?  Add debug flag?
    dumpNvmePassthruCmd(&cmd_nvme_passthru);

    // ioctl() notes
    //  "status_ioctl" equivalent to "CQE DW3[31:17]"
    //
    // NVMe Completion Queue Entry (CQE):
    //  CQE DW3[31:17]  Status Bit Field Definitions
    //      DW3[31]:    Do Not Retry (DNR)
    //      DW3[30]:    More (M)
    //      DW3[29:28]: Command Retry Delay (CRD)
    //      DW3[27:25]: Status Code Type (SCT)
    //      DW3[24:17]: Status Code (SC)
    // Related CQE note: "cmd_nvme_passthru->result" equivalent to "NVMe CQE DW0[31:0]"

    status_ioctl = ioctl(fd_nvme, NVME_IOCTL_IO_CMD, &cmd_nvme_passthru);
    if (status_ioctl) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: ioctl failed: %d", __func__, status_ioctl);
        #else
        printf("%s: ioctl failed: %d\n", __func__, status_ioctl);
        #endif //COMPILE_STANDALONE
        ret = status_ioctl;
        goto EXIT;
    }

    ret = _idm_cmd_check_status(status_ioctl, request_idm->opcode_idm);

EXIT:
    close(fd_nvme);
    return ret;
}




//START - OLD

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
    int nsid_ioctl;
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

    nsid_ioctl = ioctl(nvme_fd, NVME_IOCTL_ID);
    if (nsid_ioctl <= 0)
    {
        printf("%s: nsid ioctl fail: %d\n", __func__, nsid_ioctl);
        return nsid_ioctl;
    }

    cmd_nvme_passthru.opcode       = request_idm->cmd_nvme.opcode_nvme;
    cmd_nvme_passthru.flags        = request_idm->cmd_nvme.flags;
    cmd_nvme_passthru.rsvd1        = request_idm->cmd_nvme.command_id;
    // cmd_nvme_passthru.nsid         = request_idm->cmd_nvme.nsid;
    cmd_nvme_passthru.nsid         = nsid_ioctl;
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
    printf("opcode_nvme  (CDW0[ 7:0])  = 0x%0.2X (%u)\n", c->opcode,       c->opcode);
    printf("flags        (CDW0[15:8])  = 0x%0.2X (%u)\n", c->flags,        c->flags);
    printf("rsvd1        (CDW0[32:16]) = 0x%0.4X (%u)\n", c->rsvd1,        c->rsvd1);
    printf("nsid         (CDW1[32:0])  = 0x%0.8X (%u)\n", c->nsid,         c->nsid);
    printf("cdw2         (CDW2[32:0])  = 0x%0.8X (%u)\n", c->cdw2,         c->cdw2);
    printf("cdw3         (CDW3[32:0])  = 0x%0.8X (%u)\n", c->cdw3,         c->cdw3);
    printf("metadata     (CDW5&4[64:0])= 0x%0.16"PRIX64" (%u)\n",c->metadata, c->metadata);
    printf("addr         (CDW7&6[64:0])= 0x%0.16"PRIX64" (%u)\n",c->addr, c->addr);
    printf("metadata_len (CDW8[32:0])  = 0x%0.8X (%u)\n", c->metadata_len, c->metadata_len);
    printf("data_len     (CDW9[32:0])  = 0x%0.8X (%u)\n", c->data_len,     c->data_len);
    printf("cdw10        (CDW10[32:0]) = 0x%0.8X (%u)\n", c->cdw10,        c->cdw10);
    printf("cdw11        (CDW11[32:0]) = 0x%0.8X (%u)\n", c->cdw11,        c->cdw11);
    printf("cdw12        (CDW12[32:0]) = 0x%0.8X (%u)\n", c->cdw12,        c->cdw12);
    printf("cdw13        (CDW13[32:0]) = 0x%0.8X (%u)\n", c->cdw13,        c->cdw13);
    printf("cdw14        (CDW14[32:0]) = 0x%0.8X (%u)\n", c->cdw14,        c->cdw14);
    printf("cdw15        (CDW15[32:0]) = 0x%0.8X (%u)\n", c->cdw15,        c->cdw15);
    printf("timeout_ms   (CDW16[32:0]) = 0x%0.8X (%u)\n", c->timeout_ms,   c->timeout_ms);
    printf("result       (CDW17[32:0]) = 0x%0.8X (%u)\n", c->result,       c->result);
    printf("\n");

    status_ioctl = ioctl(nvme_fd, NVME_IOCTL_IO_CMD, &cmd_nvme_passthru);
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
