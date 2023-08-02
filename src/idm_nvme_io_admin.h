/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme.h - NVMe interface for In-drive Mutex (IDM)
 */

#ifndef __IDM_NVME_IO_ADMIN_H__
#define __IDM_NVME_IO_ADMIN_H__

#include <stdint.h>


#define ADMIN_CMD_TIMEOUT_MS_DEFAULT 15000     //TODO: Duplicated from SCSI. Uncertain behavior
#define NVME_IDENTIFY_DATA_LEN_BYTES 4096

//////////////////////////////////////////
// Admin Command Enums
//////////////////////////////////////////

// From Seagate/opensea-transport/include/nvme_helper.h
enum nvme_admin_opcodes {
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
} ;

// From Seagate/opensea-transport/include/nvme_helper.h
enum nvme_id_cns {
    NVME_IDENTIFY_CNS_NS = 0,
    NVME_IDENTIFY_CNS_CTRL = 1,
    // NVME_IDENTIFY_CNS_ALL_ACTIVE_NS = 2,
    // NVME_IDENTIFY_CNS_NS_ID_DESCRIPTOR_LIST = 3,
};

//////////////////////////////////////////
// Structs for Admin Identify
//////////////////////////////////////////

// From Seagate/opensea-transport/include/nvme_helper.h
struct nvme_id_power_state {
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
};

// From Seagate/opensea-transport/include/nvme_helper.h
struct nvme_id_ctrl {
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
    struct nvme_id_power_state psd[32];
    uint8_t             vs[1024];
};

//////////////////////////////////////////
// Functions
//////////////////////////////////////////

int nvme_admin_identify(char *drive, struct nvme_id_ctrl *data_identify_ctrl);

#endif /*__IDM_NVME_IO_ADMIN_H__ */
