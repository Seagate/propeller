/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_api.h - Primary NVMe interface for the In-drive Mutex (IDM)
  */

#ifndef __IDM_NVME_API_H__
#define __IDM_NVME_API_H__

#include <stdint.h>

#include "idm_nvme_io.h"


//TODO: Refactor function params when scsi idm apis are renamed.
//For reads, move "output" params to the end of the param list.

void nvme_idm_async_free_result(uint64_t handle);
int nvme_idm_async_get_result(uint64_t handle, int *result);
int nvme_idm_async_get_result_lock_count(uint64_t handle, int *count,
                                         int *self, int *result);
int nvme_idm_async_get_result_lock_mode(uint64_t handle, int *mode,
                                        int *result);
int nvme_idm_async_get_result_lvb(uint64_t handle, char *lvb, int lvb_size,
                                  int *result);

int nvme_idm_async_lock(char *lock_id, int mode, char *host_id,
                        char *drive, uint64_t timeout, uint64_t *handle);
int nvme_idm_async_lock_break(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout, uint64_t *handle);
int nvme_idm_async_lock_convert(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t timeout,
                                uint64_t *handle);
int nvme_idm_async_lock_destroy(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t *handle);
int nvme_idm_async_lock_renew(char *lock_id, int mode, char *host_id,
                              char *drive, uint64_t timeout, uint64_t *handle);
int nvme_idm_async_read_lock_count(char *lock_id, char *host_id, char *drive,
                                   uint64_t *handle);
int nvme_idm_async_read_lock_mode(char *lock_id, char *drive, uint64_t *handle);
int nvme_idm_async_read_lvb(char *lock_id, char *host_id, char *drive,
                            uint64_t *handle);
int nvme_idm_async_unlock(char *lock_id, int mode, char *host_id,
                          char *lvb, int lvb_size, char *drive,
                          uint64_t *handle);

int nvme_idm_environ_init(void);
void nvme_idm_environ_destroy(void);
int nvme_idm_get_fd(uint64_t handle);
int nvme_idm_read_version(char *drive, uint8_t *version_major, uint8_t *version_minor);

int nvme_idm_sync_lock(char *lock_id, int mode, char *host_id,
                       char *drive, uint64_t timeout);
int nvme_idm_sync_lock_break(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout);
int nvme_idm_sync_lock_convert(char *lock_id, int mode, char *host_id,
                               char *drive, uint64_t timeout);
int nvme_idm_sync_lock_destroy(char *lock_id, int mode, char *host_id,
                               char *drive);
int nvme_idm_sync_lock_renew(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout);
int nvme_idm_sync_read_host_state(char *lock_id, char *host_id,
                                  int *host_state, char *drive);
int nvme_idm_sync_read_lock_count(char *lock_id, char *host_id, int *count,
                                  int *self, char *drive);
int nvme_idm_sync_read_lock_mode(char *lock_id, int *mode, char *drive);
int nvme_idm_sync_read_lvb(char *lock_id, char *host_id, char *lvb,
                           int lvb_size, char *drive);
int nvme_idm_sync_read_mutex_group(char *drive, struct idm_info **info_ptr,
                                   int *info_num);
int nvme_idm_sync_unlock(char *lock_id, int mode, char *host_id,
                         char *lvb, int lvb_size, char *drive);

#endif /*__IDM_NVME_API_H__ */
