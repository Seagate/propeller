/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_utils.c - Contains nvme-related helper utility functions.
 */

#include <byteswap.h>
#include <inttypes.h>
#include <stdio.h>

#include "idm_nvme_utils.h"

//TODO: switch all "printf" to appropriate log functions from this project's log.h

/**
 * dumpIdmDataStruct - Convenience function for pretty printing the contends of the
 *                     passed in data struct.
 *
 * @d: Data structure for sending and receiving IDM-specifc data.
 */
void dumpIdmDataStruct(idmData *d){

    printf("idmData struct: fields\n");
    printf("=======================\n");
    printf("state\\ignored0     = 0x%.16"PRIX64" (%lu)\n", d->state,    d->state);
    printf("modified\\time_now  = 0x%.16"PRIX64" (%lu)\n", d->modified, d->modified);
    printf("countdown          = 0x%.16"PRIX64" (%lu)\n", d->countdown, d->countdown);
    printf("class              = 0x%.16"PRIX64" (%lu)\n", d->class,     d->class);
    printf("resource_ver = '");
    _print_char_arr(d->resource_ver, IDM_LVB_LEN_BYTES);
    printf("res_ver_type = 0x%X\n", d->resource_ver[0]);
    printf("resource_id = '");
    _print_char_arr(d->resource_id, IDM_LOCK_ID_LEN_BYTES);
    printf("metadata    = '");
    _print_char_arr(d->metadata, IDM_DATA_METADATA_LEN_BYTES);
    printf("host_id     = '");
    _print_char_arr(d->host_id, IDM_HOST_ID_LEN_BYTES);
    printf("\n");
}

/**
 * dumpIdmInfoStruct - Convenience function for pretty printing the contends of the
 *                     passed in data struct.
 *
 * @info: Data structure containing IDM-specific info.
 */
void dumpIdmInfoStruct(idmInfo *info){

    printf("idmInfo struct: fields\n");
    printf("=======================\n");
    printf("id      = '");
    _print_char_arr(info->id, IDM_LOCK_ID_LEN_BYTES);
    printf("state   = 0x%X (%d)\n", info->state, info->state);
    printf("mode    = 0x%X (%d)\n", info->mode, info->mode);
    printf("host_id = '");
    _print_char_arr(info->host_id, IDM_HOST_ID_LEN_BYTES);
    printf("last_renew_time  = 0x%.16"PRIX64" (%lu)\n", info->last_renew_time,
                                                        info->last_renew_time);
   printf("\n");
}

/**
 * dumpNvmeCmdStruct - Convenience function for pretty printing the contends of the
 *                     passed in data struct.
 *
 * @cmd_nvme:    Data structure for NVMe Vendor Specific Commands.
 * @view_fields: Boolean flag that outputs the struct's named fields.
 * @view_cdws:   Boolean flag that outputs all the struct's data as 32-bit words.
 */
void dumpNvmeCmdStruct(nvmeIdmVendorCmd *cmd_nvme, int view_fields, int view_cdws) {

    if(view_fields){
        nvmeIdmVendorCmd *c = cmd_nvme;

        printf("nvmeIdmVendorCmd struct: fields\n");
        printf("===========================\n");
        printf("opcode_nvme  (CDW0[ 7:0])  = 0x%.2X (%u)\n", c->opcode_nvme,  c->opcode_nvme);
        printf("flags        (CDW0[15:8])  = 0x%.2X (%u)\n", c->flags,        c->flags);
        printf("command_id   (CDW0[32:16]) = 0x%.4X (%u)\n", c->command_id,   c->command_id);
        printf("nsid         (CDW1[32:0])  = 0x%.8X (%u)\n", c->nsid,         c->nsid);
        printf("cdw2         (CDW2[32:0])  = 0x%.8X (%u)\n", c->cdw2,         c->cdw2);
        printf("cdw3         (CDW3[32:0])  = 0x%.8X (%u)\n", c->cdw3,         c->cdw3);
        printf("metadata     (CDW5&4[64:0])= 0x%.16"PRIX64" (%lu)\n",c->metadata, c->metadata);
        printf("addr         (CDW7&6[64:0])= 0x%.16"PRIX64" (%lu)\n",c->addr, c->addr);
        printf("metadata_len (CDW8[32:0])  = 0x%.8X (%u)\n", c->metadata_len, c->metadata_len);
        printf("data_len     (CDW9[32:0])  = 0x%.8X (%u)\n", c->data_len,     c->data_len);
        printf("ndt          (CDW10[32:0]) = 0x%.8X (%u)\n", c->ndt,          c->ndt);
        printf("ndm          (CDW11[32:0]) = 0x%.8X (%u)\n", c->ndm,          c->ndm);
        printf("opcode_idm_bits7_4(CDW12[ 7:0]) = 0x%.2X (%u)\n", c->opcode_idm_bits7_4, c->opcode_idm_bits7_4);
        printf("group_idm    (CDW12[15:8]) = 0x%.2X (%u)\n", c->group_idm,    c->group_idm);
        printf("rsvd2        (CDW12[32:16])= 0x%.4X (%u)\n", c->rsvd2,        c->rsvd2);
        printf("cdw13        (CDW13[32:0]) = 0x%.8X (%u)\n", c->cdw13,        c->cdw13);
        printf("cdw14        (CDW14[32:0]) = 0x%.8X (%u)\n", c->cdw14,        c->cdw14);
        printf("cdw15        (CDW15[32:0]) = 0x%.8X (%u)\n", c->cdw15,        c->cdw15);
        printf("timeout_ms   (CDW16[32:0]) = 0x%.8X (%u)\n", c->timeout_ms,   c->timeout_ms);
        printf("result       (CDW17[32:0]) = 0x%.8X (%u)\n", c->result,       c->result);
        printf("\n");
    }

    if(view_cdws){
        uint32_t *cdw = (uint32_t*)cmd_nvme;
        int i;

        printf("nvmeIdmVendorCmd struct: CDWs (hex)\n");
        printf("===============================\n");
        for(i = 0; i <= 17; i++) {
            printf("cdw%.2d = 0x%.8X\n", i, cdw[i]);
        }
        printf("\n");
    }
}

void dumpNvmePassthruCmd(struct nvme_passthru_cmd *cmd) {

    printf("\n");
    printf("nvme_passthru_cmd struct: fields\n");
    printf("================================\n");
    printf("opcode_nvme  (CDW0[ 7:0])  = 0x%.2X (%u)\n", cmd->opcode,       cmd->opcode);
    printf("flags        (CDW0[15:8])  = 0x%.2X (%u)\n", cmd->flags,        cmd->flags);
    printf("rsvd1        (CDW0[32:16]) = 0x%.4X (%u)\n", cmd->rsvd1,        cmd->rsvd1);
    printf("nsid         (CDW1[32:0])  = 0x%.8X (%u)\n", cmd->nsid,         cmd->nsid);
    printf("cdw2         (CDW2[32:0])  = 0x%.8X (%u)\n", cmd->cdw2,         cmd->cdw2);
    printf("cdw3         (CDW3[32:0])  = 0x%.8X (%u)\n", cmd->cdw3,         cmd->cdw3);
    printf("metadata     (CDW5&4[64:0])= 0x%.16llX (%llu)\n",cmd->metadata, cmd->metadata);
    printf("addr         (CDW7&6[64:0])= 0x%.16llX (%llu)\n",cmd->addr,     cmd->addr);
    printf("metadata_len (CDW8[32:0])  = 0x%.8X (%u)\n", cmd->metadata_len, cmd->metadata_len);
    printf("data_len     (CDW9[32:0])  = 0x%.8X (%u)\n", cmd->data_len,     cmd->data_len);
    printf("cdw10        (CDW10[32:0]) = 0x%.8X (%u)\n", cmd->cdw10,        cmd->cdw10);
    printf("cdw11        (CDW11[32:0]) = 0x%.8X (%u)\n", cmd->cdw11,        cmd->cdw11);
    printf("cdw12        (CDW12[32:0]) = 0x%.8X (%u)\n", cmd->cdw12,        cmd->cdw12);
    printf("cdw13        (CDW13[32:0]) = 0x%.8X (%u)\n", cmd->cdw13,        cmd->cdw13);
    printf("cdw14        (CDW14[32:0]) = 0x%.8X (%u)\n", cmd->cdw14,        cmd->cdw14);
    printf("cdw15        (CDW15[32:0]) = 0x%.8X (%u)\n", cmd->cdw15,        cmd->cdw15);
    printf("timeout_ms   (CDW16[32:0]) = 0x%.8X (%u)\n", cmd->timeout_ms,   cmd->timeout_ms);
    printf("result       (CDW17[32:0]) = 0x%.8X (%u)\n", cmd->result,       cmd->result);
    printf("\n");
}

void _print_char_arr(char *data, unsigned int len) {
    int i;
    for(i=0; i<len; i++) {
        if(data[i])
            printf("%c", data[i]);
        else
            printf(" ");
    }
    printf("'\n");
}
