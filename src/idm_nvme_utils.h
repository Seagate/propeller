/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_utils.h - Contains nvme-related helper utility functions.
  */

#ifndef __IDM_NVME_UTILS_H__
#define __IDM_NVME_UTILS_H__

#include "idm_cmd_common.h"
#include "idm_nvme_io.h"


void dumpIdmDataStruct(idmData *data_idm);
void dumpNvmeCmdStruct(nvmeIdmVendorCmd *cmd_nvme, int view_fields, int view_cdws);

#endif /*__IDM_NVME_UTILS_H__ */