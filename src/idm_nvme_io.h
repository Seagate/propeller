/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_io.h - Contains the lowest-level function that allow the IDM In-drive Mutex (IDM)
 *                  to talk to the Linux kernel (via ioctl(), read() or write())
*/

#ifndef __IDM_NVME_IO_H__
#define __IDM_NVME_IO_H__

#include <stdint.h>

#include "idm_cmd_common.h"


#define VENDOR_CMD_TIMEOUT_DEFAULT 15000     //TODO: Duplicated from SCSI. Uncertain behavior

//////////////////////////////////////////
// Enums
//////////////////////////////////////////
//Custom (vendor-specific) NVMe opcodes sent via CDW0[7:0] of nvmeIdmVendorCmd
typedef enum _eNvmeIdmVendorCmdOpcodes {
    NVME_IDM_VENDOR_CMD_OP_WRITE = 0xC1,
    NVME_IDM_VENDOR_CMD_OP_READ  = 0xC2,
} eNvmeIdmVendorCmdOpcodes;

typedef enum _eNvmeIdmErrorCodes {
    NVME_IDM_ERR_MUTEX_OP_FAILURE                  = 0xC0,    //SCSI Equivalent: 0x04042200
    NVME_IDM_ERR_MUTEX_REVERSE_OWNER_CHECK_FAILURE = 0xC1,    //SCSI Equivalent: 0x04042201
    NVME_IDM_ERR_MUTEX_OP_FAILURE_STATE            = 0xC2,    //SCSI Equivalent: 0x04042202
    NVME_IDM_ERR_MUTEX_OP_FAILURE_CLASS            = 0xC3,    //SCSI Equivalent: 0x04042203
    NVME_IDM_ERR_MUTEX_OP_FAILURE_OWNER            = 0xC4,    //SCSI Equivalent: 0x04042204
    NVME_IDM_ERR_MUTEX_OPCODE_INVALID              = 0xC5,    //SCSI Equivalent: 0x0520000A
    NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED              = 0xC6,    //SCSI Equivalent: 0x0B550300
    NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_HOST         = 0xC7,    //SCSI Equivalent: 0x0B550301
    NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_SHARED_HOST  = 0xC8,    //SCSI Equivalent: 0x0B550302
    NVME_IDM_ERR_MUTEX_CONFLICT                    = 0xC9,    //SCSI Equivalent: Res Conf
    NVME_IDM_ERR_MUTEX_HELD_ALREADY                = 0xCA,    //SCSI Equivalent: Terminated
    NVME_IDM_ERR_MUTEX_HELD_BY_ANOTHER             = 0xCB,    //SCSI Equivalent: Busy
}eNvmeIdmErrorCodes;


//////////////////////////////////////////
// Structs
//////////////////////////////////////////
//TODO: Add struct description HERE
typedef struct _nvmeIdmVendorCmd {
//TODO: change to "opcode_nvme" (to clarify against "opcode_idm" (change below))
    uint8_t             opcode_nvme;  //CDW0
    uint8_t             flags;        //CDW0
    uint16_t            command_id;   //CDW0
    uint32_t            nsid;         //CDW1
    uint32_t            cdw2;         //CDW2
    uint32_t            cdw3;         //CDW3
    uint64_t            metadata;     //CDW4 & 5
//CDW 6 - 9: Used when talking to the kernel layer (via ioctl()).
    uint64_t            addr;         //CDW6 & 7
    uint32_t            metadata_len; //CDW8
    uint32_t            data_len;     //CDW9
//CDW 6 - 9: Used when talking directly to the drive firmware layer, I think.
    // uint64_t            prp1;        //CDW6 & 7
    // uint64_t            prp2;        //CDW8 & 9
    uint32_t            ndt;          //CDW10
    uint32_t            ndm;          //CDW11
//TODO: Move bit fields in CDW12. (saves a bit shift, at least for me)
//  idm_group[7:0]
//  idm_opcode_bits11_8[11:8]
    uint8_t             opcode_idm_bits7_4;   //CDW12   // bits[7:4].  Lower nibble reserved.
    uint8_t             group_idm;    //CDW12
    uint16_t            rsvd2;        //CDW12
    uint32_t            cdw13;        //CDW13
    uint32_t            cdw14;        //CDW14
    uint32_t            cdw15;        //CDW15
    uint32_t            timeout_ms;   //Same as nvme_admin_cmd when using ioctl()??
    uint32_t            result;       //Same as nvme_admin_cmd when using ioctl()??
}nvmeIdmVendorCmd;



// //This struct represents the pieces of the status word that is returned from ioctl()
// // This bit field definitions of the status are defined in DWord3[31:17] of the NVMe spec's Common
// // Completion Queue Entry (CQE).
// //TODO: What does ioctl() return?  Just status code OR the entire DW3 status field.

// //Note that bits [14:0] of ioctl()'s return status word contain bits[31/24:17] above.
// typedef struct _eCqeStatusFields {
//     uint8_t     dnr;        //Do Not Retry          (CQE DWord3[31])
//     uint8_t     more;       //More                  (CQE DWord3[30])
//     uint8_t     crd;        //Command Retry Delay   (CQE DWord3[29:28])
//     uint8_t     sct;        //Status Code Type      (CQE DWord3[27:25])
//     uint8_t     sc;         //Status Code           (CQE DWord3[24:17])
// }eCqeStatusFields;


//TODO: Using a bunch of pointers here.  SCSI was using COPIES of everything.  Any issues with this?? (string lengths??, kernel vs user space memory??)
//TODO: Add struct description HERE
typedef struct _nvmeIdmRequest {
    //Cached "IDM API" input params
    char                *drive;
    char                *lock_id;
    char                *host_id;
    int                 mode_idm;
    //uint64_t            fd_async;
    uint64_t            timeout;
    char                *lvb;
    int                 lvb_size;   //TODO: should be unsigned, but public API setup with int.  size_t anyway?

    //IDM core structs
    nvmeIdmVendorCmd    cmd_nvme;
    idmData             *data_idm;

    //Misc collection area for kludgy smuggling of parameters.
    //These values are cached here in the API-level, and then
    //stored in their final location in the IO-level.
    //TODO: Convert these to function params and explictly pass around?
    uint8_t             opcode_idm;
    uint8_t             group_idm;
    char                res_ver_type;  //TODO: How is this being used?  What does it represent in the NVMe CDW block?  What type should this be?
    int                 data_len;      //TODO: should be unsigned.  size_t?
    unsigned int        data_num;       //TODO: uint64_t?
    uint64_t            class_idm;

}nvmeIdmRequest;



//////////////////////////////////////////
// Functions
//////////////////////////////////////////

int nvme_idm_write(nvmeIdmRequest *request_idm);
int nvme_idm_write_init(char *lock_id, int mode, char *host_id, char *drive,
                        uint64_t timeout, nvmeIdmRequest *request_idm);

int _nvme_idm_cmd_check_status(int status, int opcode_idm);
int _nvme_idm_cmd_init(nvmeIdmRequest *request_idm, uint8_t opcode_nvme);
int _nvme_idm_cmd_send(nvmeIdmRequest *request_idm);
int _nvme_idm_data_init_wrt(nvmeIdmRequest *request_idm);

#endif /*__IDM_NVME_IO_H__ */
