/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_utils.c - Contains nvme-related helper utility functions.
 */

#include <stdint.h>
#include <stdio.h>

#include "idm_nvme_utils.h"


/**
 * dumpIdmDataStruct - Convenience function for pretty printing the contends of the
 *                     passed in data struct.
 *                     Currently, outputs only to the CLI.
 *
 * @data_idm: Data structure for sending and receiving IDM-speicifc data.
 *
 * No return value
 */
void dumpIdmDataStruct(idmData *data_idm){

    idmData *d = data_idm;

    printf("IDM Data Struct: Fields\n");
    printf("=======================\n");
    printf("state\\ignored0        = 0x%0.16X (%u)\n", d->state,    d->state);
    printf("modified\\time_now     = 0x%0.16X (%u)\n", d->modified, d->modified);
    printf("countdown             = 0x%0.16X (%u)\n", d->countdown, d->countdown);
    printf("class_idm             = 0x%0.16X (%u)\n", d->class_idm, d->class_idm);
    printf("resource_ver          = '%s'\n", d->resource_ver);  //TODO: first char getting stomped on by "res_ver_type"
    printf("resource_id (lock_id) = '%s'\n", d->resource_id);
    printf("metadata              = '%s'\n", d->metadata);
    printf("host_id               = '%s'\n", d->host_id);
    printf("\n");
}

/**
 * dumpNvmeCmdStruct - Convenience function for pretty printing the contends of the
 *                     passed in data struct.
 *                     Currently, outputs only to the CLI.
 *
 * @cmd_nvme:    NVMe Vendor Specific Command command word data structure.
 * @view_fields: Boolean flag that outputs the struct's named fields.
 * @view_cdws:   Boolean flag that outputs all the struct's data as 32-bit words.
 *
 * No return value
 */
void dumpNvmeCmdStruct(nvmeIdmVendorCmd *cmd_nvme, int view_fields, int view_cdws) {

    if(view_fields){
        nvmeIdmVendorCmd *c = cmd_nvme;

        printf("NVMe Command Struct: Fields\n");
        printf("===========================\n");
        printf("opcode_nvme       (CDW0[ 7:0])  = 0x%0.2X (%u)\n", c->opcode_nvme,  c->opcode_nvme);
        printf("flags             (CDW0[15:8])  = 0x%0.2X (%u)\n", c->flags,        c->flags);
        printf("command_id        (CDW0[32:16]) = 0x%0.4X (%u)\n", c->command_id,   c->command_id);
        printf("nsid              (CDW1[32:0])  = 0x%0.8X (%u)\n", c->nsid,         c->nsid);
        printf("cdw2              (CDW2[32:0])  = 0x%0.8X (%u)\n", c->cdw2,         c->cdw2);
        printf("cdw3              (CDW3[32:0])  = 0x%0.8X (%u)\n", c->cdw3,         c->cdw3);
        printf("metadata          (CDW4&5[64:0])= 0x%0.16X (%u)\n",c->metadata,     c->metadata);
//TODO: There is a descrepency between views: specifically  "addr" and "cdw 6 & 7"
        printf("addr              (CDW6&7[64:0])= 0x%0.16X (%u)\n",c->addr,         c->addr);
        printf("metadata_len      (CDW8[32:0])  = 0x%0.8X (%u)\n", c->metadata_len, c->metadata_len);
        printf("data_len          (CDW9[32:0])  = 0x%0.8X (%u)\n", c->data_len,     c->data_len);
        printf("ndt               (CDW10[32:0]) = 0x%0.8X (%u)\n", c->ndt,          c->ndt);
        printf("ndm               (CDW11[32:0]) = 0x%0.8X (%u)\n", c->ndm,          c->ndm);
        printf("opcode_idm_bits7_4(CDW12[ 7:0]) = 0x%0.2X (%u)\n", c->opcode_idm_bits7_4, c->opcode_idm_bits7_4);
        printf("group_idm         (CDW12[15:8]) = 0x%0.2X (%u)\n", c->group_idm,    c->group_idm);
        printf("rsvd2             (CDW12[32:16])= 0x%0.4X (%u)\n", c->rsvd2,        c->rsvd2);
        printf("cdw13             (CDW13[32:0]) = 0x%0.8X (%u)\n", c->cdw13,        c->cdw13);
        printf("cdw14             (CDW14[32:0]) = 0x%0.8X (%u)\n", c->cdw14,        c->cdw14);
        printf("cdw15             (CDW15[32:0]) = 0x%0.8X (%u)\n", c->cdw15,        c->cdw15);
        printf("timeout_ms        (CDW16[32:0]) = 0x%0.8X (%u)\n", c->timeout_ms,   c->timeout_ms);
        printf("result            (CDW17[32:0]) = 0x%0.8X (%u)\n", c->result,       c->result);
        printf("\n");
    }

    if(view_cdws){
        uint32_t *cdw = (uint32_t*)cmd_nvme;

        printf("NVMe Command Struct: CDWs (hex)\n");
        printf("===============================\n");
        for(int i = 0; i <= 17; i++) {
            printf("cdw%0.2d = 0x%0.8X\n", i, cdw[i]);
        }
        printf("\n");
    }
}
