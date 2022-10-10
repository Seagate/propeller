/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme.h - NVMe interface for In-drive Mutex (IDM)
 */

#include <stdint.h>


#define ADMIN_CMD_TIMEOUT_DEFAULT 15
#define NVME_IDENTIFY_DATA_LEN    4096

#define C_CAST(type, val) (type)(val)

// From Seagate/opensea-transport/include/nvme_helper.h
typedef enum _eNVMeAdminOpCodes {
    // NVME_ADMIN_CMD_DELETE_SQ                    = 0x00,
    // NVME_ADMIN_CMD_CREATE_SQ                    = 0x01,
    // NVME_ADMIN_CMD_GET_LOG_PAGE                 = 0x02,
    // NVME_ADMIN_CMD_DELETE_CQ                    = 0x04,
    // NVME_ADMIN_CMD_CREATE_CQ                    = 0x05,
    NVME_ADMIN_CMD_IDENTIFY                     = 0x06,
    // NVME_ADMIN_CMD_ABORT_CMD                    = 0x08,
    // NVME_ADMIN_CMD_SET_FEATURES                 = 0x09,
    // NVME_ADMIN_CMD_GET_FEATURES                 = 0x0A,
    // NVME_ADMIN_CMD_ASYNC_EVENT                  = 0x0C,
    // NVME_ADMIN_CMD_NAMESPACE_MANAGEMENT         = 0x0D,
    // NVME_ADMIN_CMD_ACTIVATE_FW                  = 0x10,
    // NVME_ADMIN_CMD_DOWNLOAD_FW                  = 0x11,
    // NVME_ADMIN_CMD_DEVICE_SELF_TEST             = 0x14,
    // NVME_ADMIN_CMD_NAMESPACE_ATTACHMENT         = 0x15,
    // NVME_ADMIN_CMD_KEEP_ALIVE                   = 0x18,
    // NVME_ADMIN_CMD_DIRECTIVE_SEND               = 0x19,
    // NVME_ADMIN_CMD_DIRECTIVE_RECEIVE            = 0x1A,
    // NVME_ADMIN_CMD_VIRTUALIZATION_MANAGEMENT    = 0x1C,
    // NVME_ADMIN_CMD_NVME_MI_SEND                 = 0x1D,
    // NVME_ADMIN_CMD_NVME_MI_RECEIVE              = 0x1E,
    // NVME_ADMIN_CMD_DOORBELL_BUFFER_CONFIG       = 0x7C,
    // NVME_ADMIN_CMD_NVME_OVER_FABRICS            = 0x7F,
    // NVME_ADMIN_CMD_FORMAT_NVM                   = 0x80,
    // NVME_ADMIN_CMD_SECURITY_SEND                = 0x81,
    // NVME_ADMIN_CMD_SECURITY_RECV                = 0x82,
    // NVME_ADMIN_CMD_SANITIZE                     = 0x84,
} eNVMeAdminOpCodes;

// From Seagate/opensea-transport/include/nvme_helper.h
typedef enum _eNvmeIdentifyCNS {
    NVME_IDENTIFY_NS = 0,
    NVME_IDENTIFY_CTRL = 1,
    // NVME_IDENTIFY_ALL_ACTIVE_NS = 2,
    // NVME_IDENTIFY_NS_ID_DESCRIPTOR_LIST = 3,
} eNvmeIdentifyCNS;

//////////////////////////////////////////
// Admin Command\Data Structs for Identify
//////////////////////////////////////////
// From Seagate/opensea-transport/include/nvme_helper.h
typedef struct _nvmeIDPowerState {
    uint16_t            maxPower;   /* centiwatts */
    uint8_t             rsvd2;
    uint8_t             flags;
    uint32_t            entryLat;   /* microseconds */
    uint32_t            exitLat;    /* microseconds */
    uint8_t             readTPut;
    uint8_t             readLat;
    uint8_t             writeLput;
    uint8_t             writeLat;
    uint16_t            idlePower;
    uint8_t             idleScale;
    uint8_t             rsvd19;
    uint16_t            activePower;
    uint8_t             activeWorkScale;
    uint8_t             rsvd23[9];
}nvmeIDPowerState;

// From Seagate/opensea-transport/include/nvme_helper.h
typedef struct _nvmeIDCtrl {
    //controller capabilities and features
    uint16_t            vid;
    uint16_t            ssvid;
    char                sn[20];
    char                mn[40];
    char                fr[8];
    uint8_t             rab;
    uint8_t             ieee[3];
    uint8_t             cmic;
    uint8_t             mdts;
    uint16_t            cntlid;
    uint32_t            ver;
    uint32_t            rtd3r;
    uint32_t            rtd3e;
    uint32_t            oaes;
    uint32_t            ctratt;
    uint16_t            rrls;
    uint8_t             reservedBytes110_102[9];
    uint8_t             cntrltype;
    uint8_t             fguid[16];//128bit identifier
    uint16_t            crdt1;
    uint16_t            crdt2;
    uint16_t            crdt3;
    uint8_t             reservedBytes239_134[106];
    uint8_t             nvmManagement[16];
    //Admin command set attribues & optional controller capabilities
    uint16_t            oacs;
    uint8_t             acl;
    uint8_t             aerl;
    uint8_t             frmw;
    uint8_t             lpa;
    uint8_t             elpe;
    uint8_t             npss;
    uint8_t             avscc;
    uint8_t             apsta;
    uint16_t            wctemp;
    uint16_t            cctemp;
    uint16_t            mtfa;
    uint32_t            hmpre;
    uint32_t            hmmin;
    uint8_t             tnvmcap[16];
    uint8_t             unvmcap[16];
    uint32_t            rpmbs;
    uint16_t            edstt;
    uint8_t             dsto;
    uint8_t             fwug;
    uint16_t            kas;
    uint16_t            hctma;
    uint16_t            mntmt;
    uint16_t            mxtmt;
    uint32_t            sanicap;
    uint32_t            hmminds;
    uint16_t            hmmaxd;
    uint16_t            nsetidmax;
    uint16_t            endgidmax;
    uint8_t             anatt;
    uint8_t             anacap;
    uint32_t            anagrpmax;
    uint32_t            nanagrpid;
    uint32_t            pels;
    uint16_t            domainIdentifier;
    uint8_t             reservedBytes367_358[10];
    uint8_t             megcap[16];
    uint8_t             reservedBytes511_384[128];
    //NVM command set attributes;
    uint8_t             sqes;
    uint8_t             cqes;
    uint16_t            maxcmd;
    uint32_t            nn;
    uint16_t            oncs;
    uint16_t            fuses;
    uint8_t             fna;
    uint8_t             vwc;
    uint16_t            awun;
    uint16_t            awupf;
    union {
        uint8_t             nvscc;
        uint8_t             icsvscc;
    };
    uint8_t             nwpc;
    uint16_t            acwu;
    uint16_t            optionalCopyFormatsSupported;
    uint32_t            sgls;
    uint32_t            mnan;
    uint8_t             maxdna[16];
    uint32_t            maxcna;
    uint8_t             reservedBytes767_564[204];
    char                subnqn[256];
    uint8_t             reservedBytes1791_1024[768];
    uint8_t             nvmeOverFabrics[256];
    nvmeIDPowerState    psd[32];
    uint8_t             vs[1024];
}nvmeIDCtrl;

//////////////////////////////////////////
// Vendor Specific
//////////////////////////////////////////
typedef enum _eNvmeVendorCmdOpcodes {
    NVME_IDM_VENDOR_CMD_OP_WRITE = 0xC1,
    NVME_IDM_VENDOR_CMD_OP_READ  = 0xC2,
} eNvmeVendorCmdOpcodes;   //CDW0 opcode

typedef struct _nvmeIdmVendorCmd {
        uint8_t             opcode;       //CDW0
        uint8_t             flags;        //CDW0
        uint16_t            commandId;    //CDW0
        uint32_t            nsid;         //CDW1
        uint32_t            cdw2;         //CDW2
        uint32_t            cdw3;         //CDW3
        uint64_t            metadata;     //CDW4 & 5
//CDW 6 - 9: Used when talking to the kernel layer (via ioctl()).
        uint64_t            addr;         //CDW6 & 7
        uint32_t            metadata_len; //CDW8
        uint32_t            data_len;     //CDW9
//CDW 6 - 9: Used when talking to the drive firmware layer, I think.
        // uint64_t            prp1;        //CDW6 & 7
        // uint64_t            prp2;        //CDW8 & 9
        uint32_t            ndt;          //CDW10
        uint32_t            ndm;          //CDW11
//TODO: Move bit fields in CDW12. (saves a bit shift, at least for me)
//  idm_group[7:0]
//  idm_opcode_bits11_8[11:8]
        uint8_t             idm_opcode_bits7_4;   //CDW12   // bits[7:4].  Lower nibble reserved.
        uint8_t             idm_group;    //CDW12
        uint16_t            rsvd2;        //CDW12
//        uint32_t            cdw12;        //CDW12
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





////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////// COMMON ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

//TODO: Move EVERYTHING in this section to a idm_common.h file
//  Use for all new NVMe code
//  Double-check SCSI code to see if some\all of these exist already
//      Replace as necessary


#define IDM_VENDOR_CMD_DATA_LEN_BYTES   512            //TODO: Find where this is implemented on SCSI
#define IDM_VENDOR_CMD_DATA_LEN_DWORDS  512 / 4

typedef enum _eIdmOpcodes {
    IDM_OPCODE_NORMAL   = 0x0,
    IDM_OPCODE_INIT     = 0x1,
    IDM_OPCODE_TRYLOCK  = 0x2,
    IDM_OPCODE_LOCK     = 0x3,
    IDM_OPCODE_UNLOCK   = 0x4,
    IDM_OPCODE_REFRESH  = 0x5,
    IDM_OPCODE_BREAK    = 0x6,
    IDM_OPCODE_DESTROY  = 0x7,
}eIdmOpcodes;  //NVMe CDW12 mutex opcode

typedef enum _eIdmStates {
    IDM_STATE_UNINIT            = 0,
    IDM_STATE_LOCKED            = 0x101,
    IDM_STATE_UNLOCKED          = 0x102,
    IDM_STATE_MULTIPLE_LOCKED   = 0x103,
    IDM_STATE_TIMEOUT           = 0x104,
    IDM_STATE_DEAD              = 0xdead,
}eIdmStates;

typedef enum _eIdmClasses {
    IDM_CLASS_EXCLUSIVE             = 0,
    IDM_CLASS_PROTECTED_WRITE       = 0x1,
    IDM_CLASS_SHARED_PROTECTED_READ = 0x2,
}eIdmClasses;

typedef struct _idmReadData {
    uint64_t    state;
    uint64_t    modified;
    uint64_t    countdown;
    uint64_t    class;
    char        resource_ver[8];
    char        rsvd0[24];
    char        resource_id[64];
    char        metadata[64];
    char        host_id[32];
    char        rsvd1[32];
    char        rsvd2[256];
}idmReadData;

typedef struct _idmWriteData {
    uint64_t    ignored0;
    uint64_t    time_now;
    uint64_t    countdown;
    uint64_t    class;
    char        resource_ver[8];
    char        rsvd0[24];
    char        resource_id[64];
    char        metadata[64];
    char        host_id[32];
    char        rsvd1[32];
    char        ignored1[256];
}idmWriteData;


//TODO: Can I get this "generic" idm data struct to work??
//          Make sure "union's" don't cause a problem.

// typedef struct _idmData {
//     union {
//         uint64_t    state;
//         uint64_t    ignored0;
//     };
//     union {
//         uint64_t    modified;
//         uint64_t    time_now;
//     };
//     uint64_t    countdown;
//     uint64_t    class;
//     char        resource_ver[8];
//     char        rsvd0[24];
//     char        resource_id[64];
//     char        metadata[64];
//     char        host_id[32];
//     char        rsvd1[32];
//     union {
//         uint64_t    rsvd2[256];
//         uint64_t    ignored1[256];
//     };
// }idmData;



////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////// COMMON - END ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////







//////////////////////////////////////////
// Functions
//////////////////////////////////////////
void gen_nvme_cmd_identify(struct nvme_admin_cmd *cmd_admin, nvmeIDCtrl *data_identify_ctrl);
void gen_nvme_cmd_idm_read(nvmeIdmVendorCmd *cmd_idm_read,
                           idmReadData *data_idm_read,
                           uint8_t idm_opcode,
                           uint8_t idm_group);
void gen_nvme_cmd_idm_write(nvmeIdmVendorCmd *cmd_idm_write,
                            idmWriteData *data_idm_write,
                            uint8_t idm_opcode,
                            uint8_t idm_group);

int nvme_admin_identify(char *drive);
int nvme_idm_read(char *drive, uint8_t idm_opcode, uint8_t idm_group);
int nvme_idm_write(char *drive, uint8_t idm_opcode, uint8_t idm_group);

int send_nvme_cmd_admin(char *drive, struct nvme_admin_cmd *cmd_admin);
int send_nvme_cmd_idm(char *drive, nvmeIdmVendorCmd *cmd_idm_vendor);



