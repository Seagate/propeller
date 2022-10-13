/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_api.h - Primary NVMe interface for the In-drive Mutex (IDM)
  */

#include <stdint.h>


int idm_nvme_drive_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);

