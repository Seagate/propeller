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

#include "idm_nvme_io.h"


int nvme_idm_break_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_convert_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_refresh_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_renew_lock(char *lock_id, int mode, char *host_id, char *drive, uint64_t timeout);
int nvme_idm_unlock(char *lock_id, int mode, char *host_id, char *lvb, int lvb_size, char *drive);

int nvme_idm_read_mutex_num(char *drive, unsigned int *num);
// int nvme_idm_read_lvb(char *lock_id, char *host_id, char *lvb, int lvb_size, char *drive);
// int nvme_idm_read_lock_count(char *lock_id, char *host_id, int *count, int *self, char *drive);
// int nvme_idm_read_lock_mode(char *lock_id, int *mode, char *drive);
// int nvme_idm_read_host_state(char *lock_id, char *host_id, int *host_state, char *drive);
// int nvme_idm_read_mutex_group(char *drive, struct idm_info **info_ptr, int *info_num);
// ??int idm_destroy                (char *lock_id, int mode, char *host_id, char *drive);

void _memory_free_idm_request(nvmeIdmRequest *request_idm);
int _memory_init_idm_request(nvmeIdmRequest **request_idm, unsigned int data_num);
int _validate_input_common(char *lock_id, char *host_id, char *drive);
int _validate_input_write(char *lock_id, int mode, char *host_id, char *drive);

#endif /*__IDM_NVME_API_H__ */
