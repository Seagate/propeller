/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_utils.h - Contains nvme-related helper utility functions.
  */

#ifndef __IDM_NVME_UTILS_H__
#define __IDM_NVME_UTILS_H__

#include <linux/nvme_ioctl.h>

#include "idm_cmd_common.h"
#include "idm_nvme_io.h"


void dumpIdmDataStruct(struct idm_data *data_idm);
void dumpIdmInfoStruct(struct idm_info *info);
void dumpNvmeCmdStruct(struct nvme_idm_vendor_cmd *cmd_nvme, int view_fields,
                       int view_cdws);
void dumpNvmePassthruCmd(struct nvme_passthru_cmd *cmd);
void fill_nvme_cmd(struct idm_nvme_request *request_idm,
                   struct nvme_passthru_cmd *cmd_nvme_passthru);

#endif /*__IDM_NVME_UTILS_H__ */
