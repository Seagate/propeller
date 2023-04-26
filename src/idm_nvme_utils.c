/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_utils.c - Contains nvme-related helper utility functions.
 *
 * There are several violations of the 80 char line limit.
 * This is on purpose to make the code easier to read.
 */

#include <byteswap.h>
#include <inttypes.h>
#include <stdio.h>

#include "idm_nvme_utils.h"

//TODO: switch all "printf" to appropriate log functions from this project's log.h

/**
 * dumpIdmDataStruct - Convenience function for pretty printing the contends of
 * the passed in data struct.
 *
 * @d: Data structure for sending and receiving IDM-specifc data.
 */
void dumpIdmDataStruct(struct idm_data *d)
{
	printf("struct idm_data: fields\n");
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
 * dumpIdmInfoStruct - Convenience function for pretty printing the contends of
 * the passed in data struct.
 *
 * @info: Data structure containing IDM-specific info.
 */
void dumpIdmInfoStruct(struct idm_info *info)
{
	printf("struct idm_info: fields\n");
	printf("======================\n");
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
 * dumpNvmeCmdStruct - Convenience function for pretty printing the contends of
 * the passed in data struct.
 *
 * @cmd_nvme:    Data structure for NVMe Vendor Specific Commands.
 * @view_fields: Boolean flag that outputs the struct's named fields.
 * @view_cdws:   Boolean flag that outputs all the struct's data as 32-bit words.
 */
void dumpNvmeCmdStruct(struct nvme_idm_vendor_cmd *cmd_nvme, int view_fields,
                       int view_cdws)
{
	if(view_fields){
		struct nvme_idm_vendor_cmd *c = cmd_nvme;

		printf("struct nvme_idm_vendor_cmd: fields\n");
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

		printf("struct nvme_idm_vendor_cmd: CDWs (hex)\n");
		printf("===============================\n");
		for(i = 0; i <= 17; i++) {
			printf("cdw%.2d = 0x%.8X\n", i, cdw[i]);
		}
		printf("\n");
	}
}

void dumpNvmePassthruCmd(struct nvme_passthru_cmd *cmd)
{
	printf("\n");
	printf("struct nvme_passthru_cmd: fields\n");
	printf("================================\n");
	printf("opcode       (CDW0[ 7:0])  = 0x%.2X (%u)\n", cmd->opcode,       cmd->opcode);
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

void _print_char_arr(char *data, unsigned int len)
{
	int i;
	for(i=0; i<len; i++) {
		if(data[i])
			printf("%c", data[i]);
		else
			printf(" ");
	}
	printf("'\n");
}

/**
 * fill_nvme_cmd -  Transfer all the data in the "request" structure to the
 * prefined system NVMe command structure used by the system commands
 * (like ioctl()).
 *
 * @request_idm:        Struct containing all NVMe-specific command info for the
 *                      requested IDM action.
 * @cmd_nvme_passthru:  Predefined NVMe command struct to be filled.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
void fill_nvme_cmd(struct idm_nvme_request *request_idm,
                    struct nvme_passthru_cmd *cmd_nvme_passthru)
{
	//TODO: Leave this commented out section for nsid in-place for now.  May be needed in near future.
	// request_idm->cmd_nvme.nsid = ioctl(fd_nvme, NVME_IOCTL_ID);
	// if (request_idm->cmd_nvme.nsid <= 0)
	// {
	//     printf("%s: nsid ioctl fail: %d\n", __func__, request_idm->cmd_nvme.nsid);
	//     ret = request_idm->cmd_nvme.nsid;
	//     goto EXIT;
	// }

	cmd_nvme_passthru->opcode       = request_idm->cmd_nvme.opcode_nvme;
	cmd_nvme_passthru->flags        = request_idm->cmd_nvme.flags;
	cmd_nvme_passthru->rsvd1        = request_idm->cmd_nvme.command_id;
	cmd_nvme_passthru->nsid         = request_idm->cmd_nvme.nsid;
	cmd_nvme_passthru->cdw2         = request_idm->cmd_nvme.cdw2;
	cmd_nvme_passthru->cdw3         = request_idm->cmd_nvme.cdw3;
	cmd_nvme_passthru->metadata     = request_idm->cmd_nvme.metadata;
	cmd_nvme_passthru->addr         = request_idm->cmd_nvme.addr;
	cmd_nvme_passthru->metadata_len = request_idm->cmd_nvme.metadata_len;
	cmd_nvme_passthru->data_len     = request_idm->cmd_nvme.data_len;
	cmd_nvme_passthru->cdw10        = request_idm->cmd_nvme.ndt;
	cmd_nvme_passthru->cdw11        = request_idm->cmd_nvme.ndm;
	cmd_nvme_passthru->cdw12        = ((uint32_t)request_idm->cmd_nvme.rsvd2 << 16) |
					((uint32_t)request_idm->cmd_nvme.group_idm << 8) |
					(uint32_t)request_idm->cmd_nvme.opcode_idm_bits7_4;
	cmd_nvme_passthru->cdw13        = request_idm->cmd_nvme.cdw13;
	cmd_nvme_passthru->cdw14        = request_idm->cmd_nvme.cdw14;
	cmd_nvme_passthru->cdw15        = request_idm->cmd_nvme.cdw15;
	cmd_nvme_passthru->timeout_ms   = request_idm->cmd_nvme.timeout_ms;
}

