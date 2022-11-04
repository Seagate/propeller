/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_cmd_common.h - In-drive Mutex (IDM) related structs, enums, etc. that are common to SCSI and NVMe.
 */

#ifndef __IDM_CMD_COMMON_H__
#define __IDM_CMD_COMMON_H__

//TODO: After NVMe implementation complete, and while updating scsi
// code to interact with new IDM API code, replace the #defines
// at the top of the idm.scsi.c file with this files constants

#include <stdint.h>

//////////////////////////////////////////
// Defines
//////////////////////////////////////////
#define SUCCESS 0;
#define FAILURE -1;

#define DFLT_NUM_IDM_DATA_BLOCKS    1

#define IDM_LOCK_ID_LEN_BYTES       64
#define IDM_HOST_ID_LEN_BYTES       32
#define IDM_LVB_LEN_BYTES            8

// Other idmData char array lengths
#define IDM_DATA_RESOURCE_VER_LEN_BYTES 8
#define IDM_DATA_RESERVED_0_LEN_BYTES   24
#define IDM_DATA_METADATA_LEN_BYTES     64
#define IDM_DATA_RESERVED_1_LEN_BYTES   32

//////////////////////////////////////////
// Enums
//////////////////////////////////////////
typedef enum _eIdmClasses {
    IDM_MODE_UNLOCK    = 0,
    IDM_MODE_EXCLUSIVE = 0x1,
    IDM_MODE_SHAREABLE = 0x2,
}eIdmClasses;

typedef enum _eIdmGroups {
    IDM_GROUP_DEFAULT = 1,
    IDM_GROUP_INQUIRY = 0xFF,
}eIdmGroups;

typedef enum _eIdmModes {
    IDM_CLASS_EXCLUSIVE             = 0,
    IDM_CLASS_PROTECTED_WRITE       = 0x1,
    IDM_CLASS_SHARED_PROTECTED_READ = 0x2,
}eIdmModes;

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

typedef enum _eIdmResVer {
    IDM_RES_VER_NO_UPDATE_NO_VALID = 0,
    IDM_RES_VER_UPDATE_NO_VALID    = 0x1,
    IDM_RES_VER_UPDATE_VALID       = 0x2,
    IDM_RES_VER_INVALID            = 0x3,
}eIdmResVer;

typedef enum _eIdmStates {
    IDM_STATE_UNINIT            = 0,
    IDM_STATE_LOCKED            = 0x101,
    IDM_STATE_UNLOCKED          = 0x102,
    IDM_STATE_MULTIPLE_LOCKED   = 0x103,
    IDM_STATE_TIMEOUT           = 0x104,
    IDM_STATE_DEAD              = 0xdead,
}eIdmStates;

//////////////////////////////////////////
// Structs
//////////////////////////////////////////
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
    uint64_t    class;
    char        resource_ver[IDM_DATA_RESOURCE_VER_LEN_BYTES];
    char        rsvd0[IDM_DATA_RESERVED_0_LEN_BYTES];
    char        resource_id[IDM_LOCK_ID_LEN_BYTES];
    char        metadata[IDM_DATA_METADATA_LEN_BYTES];
    char        host_id[IDM_HOST_ID_LEN_BYTES];
    char        rsvd1[IDM_DATA_RESERVED_1_LEN_BYTES];
    union {
        char    rsvd2[256];      // For idm_read
        char    ignored1[256];   // For idm_write
    };
}idmData;

typedef struct _idmInfo {
    /* Lock ID */
    char id[IDM_LOCK_ID_LEN_BYTES];
    int state;
    int mode;

    /* Host ID */
    char host_id[IDM_HOST_ID_LEN_BYTES];

    /* Membership */
    uint64_t last_renew_time;
}idmInfo;


//////////////////////////////////////////
// Functions
//////////////////////////////////////////

void bswap_char_arr(char *dst, char *src, int len);


#endif /*__IDM_CMD_COMMON_H__ */
