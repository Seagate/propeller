/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
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
#include "log.h"

//TODO: switch all "ilm_log_dbg" to appropriate log functions from this project's log.h

/**
 * dumpIdmDataStruct - Convenience function for pretty printing the contends of
 * the passed in data struct.
 *
 * @d: Data structure for sending and receiving IDM-specifc data.
 */
void dumpIdmDataStruct(struct idm_data *d)
{
	ilm_log_dbg("struct idm_data: fields");
	ilm_log_dbg("=======================");
	ilm_log_dbg("state\\ignored0     = 0x%.16"PRIX64" (%lu)", d->state,    d->state);
	ilm_log_dbg("modified\\time_now  = 0x%.16"PRIX64" (%lu)", d->modified, d->modified);
	ilm_log_dbg("countdown          = 0x%.16"PRIX64" (%lu)", d->countdown, d->countdown);
	ilm_log_dbg("class              = 0x%.16"PRIX64" (%lu)", d->class,     d->class);
	ilm_log_array_dbg("resource_ver", d->resource_ver, IDM_LVB_LEN_BYTES);
	ilm_log_dbg("res_ver_type = 0x%X", d->resource_ver[0]);
	ilm_log_array_dbg("resource_id", d->resource_id, IDM_LOCK_ID_LEN_BYTES);
	ilm_log_array_dbg("metadata", d->metadata, IDM_DATA_METADATA_LEN_BYTES);
	ilm_log_array_dbg("host_id", d->host_id, IDM_HOST_ID_LEN_BYTES);
	ilm_log_dbg("\n");
}

/**
 * dumpIdmInfoStruct - Convenience function for pretty printing the contends of
 * the passed in data struct.
 *
 * @info: Data structure containing IDM-specific info.
 */
void dumpIdmInfoStruct(struct idm_info *info)
{
	ilm_log_dbg("struct idm_info: fields");
	ilm_log_dbg("======================");
	ilm_log_array_dbg("id", info->id, IDM_LOCK_ID_LEN_BYTES);
	ilm_log_dbg("state   = 0x%X (%d)", info->state, info->state);
	ilm_log_dbg("mode    = 0x%X (%d)", info->mode, info->mode);
	ilm_log_array_dbg("host_id", info->host_id, IDM_HOST_ID_LEN_BYTES);
	ilm_log_dbg("last_renew_time  = 0x%.16"PRIX64" (%lu)", info->last_renew_time,
	                                                    info->last_renew_time);
	ilm_log_dbg("\n");
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

		ilm_log_dbg("struct nvme_idm_vendor_cmd: fields");
		ilm_log_dbg("===========================");
		ilm_log_dbg("opcode_nvme  (CDW0[ 7:0])  = 0x%.2X (%u)", c->opcode_nvme,  c->opcode_nvme);
		ilm_log_dbg("flags        (CDW0[15:8])  = 0x%.2X (%u)", c->flags,        c->flags);
		ilm_log_dbg("command_id   (CDW0[32:16]) = 0x%.4X (%u)", c->command_id,   c->command_id);
		ilm_log_dbg("nsid         (CDW1[32:0])  = 0x%.8X (%u)", c->nsid,         c->nsid);
		ilm_log_dbg("cdw2         (CDW2[32:0])  = 0x%.8X (%u)", c->cdw2,         c->cdw2);
		ilm_log_dbg("cdw3         (CDW3[32:0])  = 0x%.8X (%u)", c->cdw3,         c->cdw3);
		ilm_log_dbg("metadata     (CDW5&4[64:0])= 0x%.16"PRIX64" (%lu)",c->metadata, c->metadata);
		ilm_log_dbg("addr         (CDW7&6[64:0])= 0x%.16"PRIX64" (%lu)",c->addr, c->addr);
		ilm_log_dbg("metadata_len (CDW8[32:0])  = 0x%.8X (%u)", c->metadata_len, c->metadata_len);
		ilm_log_dbg("data_len     (CDW9[32:0])  = 0x%.8X (%u)", c->data_len,     c->data_len);
		ilm_log_dbg("ndt          (CDW10[32:0]) = 0x%.8X (%u)", c->ndt,          c->ndt);
		ilm_log_dbg("ndm          (CDW11[32:0]) = 0x%.8X (%u)", c->ndm,          c->ndm);
		ilm_log_dbg("opcode_idm_bits7_4(CDW12[ 7:0]) = 0x%.2X (%u)", c->opcode_idm_bits7_4, c->opcode_idm_bits7_4);
		ilm_log_dbg("group_idm    (CDW12[15:8]) = 0x%.2X (%u)", c->group_idm,    c->group_idm);
		ilm_log_dbg("rsvd2        (CDW12[32:16])= 0x%.4X (%u)", c->rsvd2,        c->rsvd2);
		ilm_log_dbg("cdw13        (CDW13[32:0]) = 0x%.8X (%u)", c->cdw13,        c->cdw13);
		ilm_log_dbg("cdw14        (CDW14[32:0]) = 0x%.8X (%u)", c->cdw14,        c->cdw14);
		ilm_log_dbg("cdw15        (CDW15[32:0]) = 0x%.8X (%u)", c->cdw15,        c->cdw15);
		ilm_log_dbg("timeout_ms   (CDW16[32:0]) = 0x%.8X (%u)", c->timeout_ms,   c->timeout_ms);
		ilm_log_dbg("result       (CDW17[32:0]) = 0x%.8X (%u)", c->result,       c->result);
		ilm_log_dbg("\n");
	}

	if(view_cdws){
		uint32_t *cdw = (uint32_t*)cmd_nvme;
		int i;

		ilm_log_dbg("struct nvme_idm_vendor_cmd: CDWs (hex)");
		ilm_log_dbg("===============================");
		for(i = 0; i <= 17; i++) {
			ilm_log_dbg("cdw%.2d = 0x%.8X\n", i, cdw[i]);
		}
		ilm_log_dbg("\n");
	}
}

void dumpNvmePassthruCmd(struct nvme_passthru_cmd *cmd)
{
	ilm_log_dbg("\n");
	ilm_log_dbg("struct nvme_passthru_cmd: fields");
	ilm_log_dbg("================================");
	ilm_log_dbg("opcode       (CDW0[ 7:0])  = 0x%.2X (%u)", cmd->opcode,       cmd->opcode);
	ilm_log_dbg("flags        (CDW0[15:8])  = 0x%.2X (%u)", cmd->flags,        cmd->flags);
	ilm_log_dbg("rsvd1        (CDW0[32:16]) = 0x%.4X (%u)", cmd->rsvd1,        cmd->rsvd1);
	ilm_log_dbg("nsid         (CDW1[32:0])  = 0x%.8X (%u)", cmd->nsid,         cmd->nsid);
	ilm_log_dbg("cdw2         (CDW2[32:0])  = 0x%.8X (%u)", cmd->cdw2,         cmd->cdw2);
	ilm_log_dbg("cdw3         (CDW3[32:0])  = 0x%.8X (%u)", cmd->cdw3,         cmd->cdw3);
	ilm_log_dbg("metadata     (CDW5&4[64:0])= 0x%.16llX (%llu)",cmd->metadata, cmd->metadata);
	ilm_log_dbg("addr         (CDW7&6[64:0])= 0x%.16llX (%llu)",cmd->addr,     cmd->addr);
	ilm_log_dbg("metadata_len (CDW8[32:0])  = 0x%.8X (%u)", cmd->metadata_len, cmd->metadata_len);
	ilm_log_dbg("data_len     (CDW9[32:0])  = 0x%.8X (%u)", cmd->data_len,     cmd->data_len);
	ilm_log_dbg("cdw10        (CDW10[32:0]) = 0x%.8X (%u)", cmd->cdw10,        cmd->cdw10);
	ilm_log_dbg("cdw11        (CDW11[32:0]) = 0x%.8X (%u)", cmd->cdw11,        cmd->cdw11);
	ilm_log_dbg("cdw12        (CDW12[32:0]) = 0x%.8X (%u)", cmd->cdw12,        cmd->cdw12);
	ilm_log_dbg("cdw13        (CDW13[32:0]) = 0x%.8X (%u)", cmd->cdw13,        cmd->cdw13);
	ilm_log_dbg("cdw14        (CDW14[32:0]) = 0x%.8X (%u)", cmd->cdw14,        cmd->cdw14);
	ilm_log_dbg("cdw15        (CDW15[32:0]) = 0x%.8X (%u)", cmd->cdw15,        cmd->cdw15);
	ilm_log_dbg("timeout_ms   (CDW16[32:0]) = 0x%.8X (%u)", cmd->timeout_ms,   cmd->timeout_ms);
	ilm_log_dbg("result       (CDW17[32:0]) = 0x%.8X (%u)", cmd->result,       cmd->result);
	ilm_log_dbg("\n");
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
	//     ilm_log_dbg("%s: nsid ioctl fail: %d\n", __func__, request_idm->cmd_nvme.nsid);
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

