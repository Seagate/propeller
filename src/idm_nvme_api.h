/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_api.h - Primary NVMe interface for the In-drive Mutex (IDM)
  */

#ifndef __IDM_NVME_API_H__
#define __IDM_NVME_API_H__

#include <stdint.h>


int nvme_idm_break_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_convert_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_refresh_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_renew_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_unlock(char *lock_id, int mode, char *host_id, char *lvb, int lvb_size, char *drive);

#endif /*__IDM_NVME_API_H__ */
