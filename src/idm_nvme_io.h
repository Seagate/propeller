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

#include <limits.h>
#include <stdint.h>
#include <linux/nvme_ioctl.h>
#include <sys/ioctl.h>

#include "async_nvme_thread_pool.h"
#include "idm_cmd_common.h"


#define VENDOR_CMD_TIMEOUT_DEFAULT 15000     //TODO: Duplicated from SCSI. Uncertain behavior

//////////////////////////////////////////
// Enums
//////////////////////////////////////////
//Custom (vendor-specific) NVMe opcodes sent via CDW0[7:0] of struct nvme_idm_vendor_cmd
enum nvme_idm_vendor_cmd_opcodes {
	NVME_IDM_VENDOR_CMD_OP_WRITE = 0xC1,
	NVME_IDM_VENDOR_CMD_OP_READ  = 0xC2,
};

enum nvme_idm_error_codes {//ioctl() idm-specific status codes(sc) of status code type 1(sct1)
	NVME_IDM_ERR_MUTEX_OP_FAILURE                  = 0xC0,  // Mutex operation failed                                                   //SCSI Equivalent: 0x04042200
	NVME_IDM_ERR_MUTEX_REVERSE_OWNER_CHECK_FAILURE = 0xC1,  // Mutex failed reverse owner check                                         //SCSI Equivalent: 0x04042201
	NVME_IDM_ERR_MUTEX_OP_FAILURE_STATE            = 0xC2,  // Mutex operation failed for state error                                   //SCSI Equivalent: 0x04042202
	NVME_IDM_ERR_MUTEX_OP_FAILURE_CLASS            = 0xC3,  // Mutex operation failed for class error                                   //SCSI Equivalent: 0x04042203
	NVME_IDM_ERR_MUTEX_OP_FAILURE_OWNER            = 0xC4,  // Mutex operation failed for owner error                                   //SCSI Equivalent: 0x04042204
	NVME_IDM_ERR_MUTEX_OPCODE_INVALID              = 0xC5,  // Mutex failed for invalid mutex opcode                                    //SCSI Equivalent: 0x0520000A
	NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED              = 0xC6,  // Mutex limit exceeded                                                     //SCSI Equivalent: 0x0B550300
	NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_HOST         = 0xC7,  // Mutex host limit exceeded                                                //SCSI Equivalent: 0x0B550301
	NVME_IDM_ERR_MUTEX_LIMIT_EXCEEDED_SHARED_HOST  = 0xC8,  // Mutex number of shared hosts exceeded                                    //SCSI Equivalent: 0x0B550302
	NVME_IDM_ERR_MUTEX_GROUP_INVALID               = 0xC9,  // Mutex failed for invalid mutex group (LBA out of range, Mutex not found) //SCSI Equivalent: 0x05210001
	NVME_IDM_ERR_MUTEX_RESERVATION_CONFLICT        = 0xCA,  // Mutex reservation conflict status (mutex conflict)                       //SCSI Equivalent: Res Conf
	NVME_IDM_ERR_MUTEX_COMMAND_TERMINATED          = 0xCB,  // Mutex command terminated status (mutex initiator holds the lock)         //SCSI Equivalent: Terminated
	NVME_IDM_ERR_MUTEX_BUSY                        = 0xCC,  // Mutex busy status (mutex held by other)                                  //SCSI Equivalent: Busy
};

//////////////////////////////////////////
// Structs
//////////////////////////////////////////
//TODO: Add struct description HERE
struct nvme_idm_vendor_cmd {
	uint8_t             opcode_nvme;  //CDW0
	uint8_t             flags;        //CDW0    psdt_bits7_6, rsvd0_bits5_2, fuse_bits1_0
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
	uint8_t             opcode_idm_bits7_4;   //CDW12   // bits[7:4].  Lower nibble reserved.
	uint8_t             group_idm;    //CDW12
	uint16_t            rsvd2;        //CDW12
	uint32_t            cdw13;        //CDW13
	uint32_t            cdw14;        //CDW14
	uint32_t            cdw15;        //CDW15
	uint32_t            timeout_ms;   //Same as nvme_admin_cmd when using ioctl()??
	uint32_t            result;       //Same as nvme_admin_cmd when using ioctl()??
};



// //This struct represents the pieces of the status word that is returned from ioctl()
// // This bit field definitions of the status are defined in DWord3[31:17] of the NVMe spec's Common
// // Completion Queue Entry (CQE).
// //TODO: What does ioctl() return?  Just status code OR the entire DW3 status field.

// //Note that bits [14:0] of ioctl()'s return status word contain bits[31/24:17] above.
// struct cqe_status_fields {
//     uint8_t     dnr;        //Do Not Retry          (CQE DWord3[31])
//     uint8_t     more;       //More                  (CQE DWord3[30])
//     uint8_t     crd;        //Command Retry Delay   (CQE DWord3[29:28])
//     uint8_t     sct;        //Status Code Type      (CQE DWord3[27:25])
//     uint8_t     sc;         //Status Code           (CQE DWord3[24:17])
// };


//TODO: Add struct description HERE
struct idm_nvme_request {
	//Cached "IDM API" input params
	char                lock_id[IDM_LOCK_ID_LEN_BYTES];
	int                 mode_idm;
	char                host_id[IDM_HOST_ID_LEN_BYTES];
	char                drive[PATH_MAX];
	uint64_t            timeout;
	char                lvb[IDM_LVB_LEN_BYTES];
	int                 lvb_size;   //TODO: should be unsigned, but public API setup with int.  size_t anyway?

	//IDM core structs
	struct nvme_idm_vendor_cmd  cmd_nvme;
	struct idm_data             *data_idm;

	//Generally, these values are cached here in the API-layer, and then
	//stored in their final location in the IO-layer.
	uint8_t             opcode_idm;
	uint8_t             group_idm;
	char                res_ver_type;
	int                 data_len;      //TODO: should be unsigned.  size_t?
	unsigned int        data_num;      //Note: Currently, this also corresponds to # of mutexes (ie: mutex_num).  //TODO: uint64_t?
	uint64_t            class;
	int                 fd_nvme;

	//Variables related to temporary NVMe asynchronous code
	struct async_nvme_request     requent_async;
};


//////////////////////////////////////////
// Functions
//////////////////////////////////////////
int nvme_idm_async_data_rcv(struct idm_nvme_request *request_idm, int *result);
int nvme_idm_async_read(struct idm_nvme_request *request_idm);
int nvme_idm_async_write(struct idm_nvme_request *request_idm);
int nvme_idm_sync_read(struct idm_nvme_request *request_idm);
int nvme_idm_sync_write(struct idm_nvme_request *request_idm);
void nvme_idm_read_init(char *drive, struct idm_nvme_request *request_idm);
int nvme_idm_write_init(char *lock_id, int mode, char *host_id, char *drive,
                        uint64_t timeout, struct idm_nvme_request *request_idm);

int _async_idm_cmd_send(struct idm_nvme_request *request_idm);  // Uses write()   (_scsi_write())
int _async_idm_data_rcv(struct idm_nvme_request *request_idm, int *result); // Uses read()    (_scsi_read()) (ALWAYS returns status code.  MAY fill data)
void _fill_nvme_cmd(struct idm_nvme_request *request_idm,
                    struct nvme_admin_cmd *cmd_nvme_passthru);
int _idm_cmd_check_status(int status, uint8_t opcode_idm);
int _idm_cmd_init(struct idm_nvme_request *request_idm, uint8_t opcode_nvme);
int _idm_data_init_wrt(struct idm_nvme_request *request_idm);
int _sync_idm_cmd_send(struct idm_nvme_request *request_idm);   // Uses ioctl()

#endif /*__IDM_NVME_IO_H__ */
