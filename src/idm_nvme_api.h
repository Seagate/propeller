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
int nvme_idm_async_lock_refresh(char *lock_id, int mode, char *host_id,
                                char *drive, uint64_t timeout,
                                uint64_t *handle);
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

int nvme_idm_get_fd(uint64_t handle);
int nvme_idm_read_version(int *version, char *drive);

int nvme_idm_sync_lock(char *lock_id, int mode, char *host_id,
                       char *drive, uint64_t timeout);
int nvme_idm_sync_lock_break(char *lock_id, int mode, char *host_id,
                             char *drive, uint64_t timeout);
int nvme_idm_sync_lock_convert(char *lock_id, int mode, char *host_id,
                               char *drive, uint64_t timeout);
int nvme_idm_sync_lock_destroy(char *lock_id, int mode, char *host_id,
                               char *drive);
int nvme_idm_sync_lock_refresh(char *lock_id, int mode, char *host_id,
                               char *drive, uint64_t timeout);
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
int nvme_idm_sync_read_mutex_num(char *drive, unsigned int *mutex_num);
int nvme_idm_sync_unlock(char *lock_id, int mode, char *host_id,
                         char *lvb, int lvb_size, char *drive);

int _init_lock(char *lock_id, int mode, char *host_id, char *drive,
               uint64_t timeout, struct idm_nvme_request **request_idm);
int _init_lock_break(char *lock_id, int mode, char *host_id, char *drive,
                     uint64_t timeout, struct idm_nvme_request **request_idm);
int _init_lock_destroy(char *lock_id, int mode, char *host_id, char *drive,
                       struct idm_nvme_request **request_idm);
int _init_lock_refresh(char *lock_id, int mode, char *host_id, char *drive,
                       uint64_t timeout, struct idm_nvme_request **request_idm);
int _init_read_host_state(char *lock_id, char *host_id, char *drive,
                          struct idm_nvme_request **request_idm);
int _init_read_lock_count(int async_on, char *lock_id, char *host_id,
                          char *drive, struct idm_nvme_request **request_idm);
int _init_read_lock_mode(int async_on, char *lock_id, char *drive,
                         struct idm_nvme_request **request_idm);
int _init_read_lvb(int async_on, char *lock_id, char *host_id, char *drive,
                   struct idm_nvme_request **request_idm);
int _init_read_mutex_group(char *drive, struct idm_nvme_request **request_idm);
int _init_read_mutex_num(char *drive, struct idm_nvme_request **request_idm);
int _init_unlock(char *lock_id, int mode, char *host_id, char *lvb,
                 int lvb_size, char *drive,
                 struct idm_nvme_request **request_idm);

void _memory_free_idm_request(struct idm_nvme_request *request_idm);
int _memory_init_idm_request(struct idm_nvme_request **request_idm,
                             unsigned int data_num);

int _parse_host_state(struct idm_nvme_request *request_idm, int *host_state);
int _parse_lock_count(struct idm_nvme_request *request_idm, int *count,
                      int *self);
int _parse_lock_mode(struct idm_nvme_request *request_idm, int *mode);
int _parse_lvb(struct idm_nvme_request *request_idm, char *lvb, int lvb_size);
int _parse_mutex_group(struct idm_nvme_request *request_idm,
                       struct idm_info **info_ptr, int *info_num);
void _parse_mutex_num(struct idm_nvme_request *request_idm,
                      unsigned int *mutex_num);

int _validate_input_common(char *lock_id, char *host_id, char *drive);
int _validate_input_write(char *lock_id, int mode, char *host_id, char *drive);

#endif /*__IDM_NVME_API_H__ */
