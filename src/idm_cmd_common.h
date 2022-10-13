/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_cmd_common.h - In-drive Mutex (IDM) related structs, enums, etc. that are common to SCSI and NVMe.
 */


//TODO: Double-check SCSI code to see if some\all of these exist already
//      Replace as necessary

#include <stdint.h>


// #define C_CAST(type, val) (type)(val)

#define SUCCESS 0;
#define FAILURE -1;



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

typedef enum _eIdmModes {
    IDM_CLASS_EXCLUSIVE             = 0,
    IDM_CLASS_PROTECTED_WRITE       = 0x1,
    IDM_CLASS_SHARED_PROTECTED_READ = 0x2,
}eIdmModes;

typedef enum _eIdmClasses {
    IDM_MODE_UNLOCK    = 0,
    IDM_MODE_EXCLUSIVE = 0x1,
    IDM_MODE_SHAREABLE = 0x2,
}eIdmClasses;

typedef enum _eIdmResVer {
    IDM_RES_VER_NO_UPDATE_NO_VALID = 0,
    IDM_RES_VER_UPDATE_NO_VALID    = 0x1,
    IDM_RES_VER_UPDATE_VALID       = 0x2,
    IDM_RES_VER_INVALID            = 0x3,
}eIdmResVer;

typedef struct _idmData {
    union {
        uint64_t    state;           // For idm_read
        uint64_t    ignored0;        // For idm_write
    };
    union {
        uint64_t    modified;        // For idm_read
        uint64_t    time_now;        // For idm_write
    };
    uint64_t    countdown;
    uint64_t    class_idm;
    char        resource_ver[8];
    char        rsvd0[24];
    char        resource_id[64];
    char        metadata[64];
    char        host_id[32];
    char        rsvd1[32];
    union {
        uint64_t    rsvd2[256];      // For idm_read
        uint64_t    ignored1[256];   // For idm_write
    };
}idmData;
