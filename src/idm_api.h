/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __IDM_API_H__
#define __IDM_API_H__

#include <stdint.h>

#include "idm_cmd_common.h"

#define IDM_LOCK_ID_LEN			64
#define IDM_HOST_ID_LEN			32
#define IDM_VALUE_LEN			8

#define NVME_DEVICE_TAG "nvme"

/*
 * NOTE: assumes the functions idm_drive_init() and idm_drive_destroy_lock()
 * will be used internally in IDM wrapper layer; so this can be transparent
 * to upper layer and reduce the complexity in raid lock layer.
 */
#if 0
int idm_drive_init(char *lock_id, char *host_id, char *drive);
int idm_drive_destroy_lock(char *lock_id, char *host_id, char *drive);
#endif

int idm_drive_version(char *drive, uint8_t *version_major, uint8_t *version_minor);
int idm_drive_lock(char *lock_id, int mode, char *host_id, char *drive,
                   uint64_t timeout);
int idm_drive_lock_async(char *lock_id, int mode, char *host_id, char *drive,
                         uint64_t timeout, uint64_t *handle);
int idm_drive_unlock(char *lock_id, int mode, char *host_id, char *lvb,
                     int lvb_size, char *drive);
int idm_drive_unlock_async(char *lock_id, int mode, char *host_id, char *lvb,
                           int lvb_size, char *drive, uint64_t *handle);
int idm_drive_convert_lock(char *lock_id, int mode, char *host_id, char *drive,
                           uint64_t timeout);
int idm_drive_convert_lock_async(char *lock_id, int mode, char *host_id,
                                 char *drive, uint64_t timeout,
                                 uint64_t *handle);
int idm_drive_renew_lock(char *lock_id, int mode, char *host_id, char *drive,
                         uint64_t timeout);
int idm_drive_renew_lock_async(char *lock_id, int mode, char *host_id,
                               char *drive, uint64_t timeout,
                               uint64_t *handle);
int idm_drive_break_lock(char *lock_id, int mode, char *host_id, char *drive,
                         uint64_t timeout);
int idm_drive_break_lock_async(char *lock_id, int mode, char *host_id,
                               char *drive, uint64_t timeout,
                               uint64_t *handle);
int idm_drive_write_lvb(char *lock_id, char *host_id, char *lvb, int lvb_size,
                        char *drive);
int idm_drive_write_lvb_async(char *lock_id, char *host_id, char *lvb,
                              int lvb_size, char *drive, uint64_t *handle);

int idm_drive_read_lvb(char *lock_id, char *host_id, char *lvb, int lvb_size,
                       char *drive);
int idm_drive_read_lvb_async(char *lock_id, char *host_id, char *drive,
                             uint64_t *handle);
int idm_drive_read_lvb_async_result(char *drive, uint64_t handle, char *lvb,
                                    int lvb_size, int *result);
int idm_drive_lock_count(char *lock_id, char *host_id, int *count, int *self,
                         char *drive);
int idm_drive_lock_count_async(char *lock_id, char *host_id, char *drive,
                               uint64_t *handle);
int idm_drive_lock_count_async_result(char *drive, uint64_t handle, int *count,
                                      int *self, int *result);
int idm_drive_lock_mode(char *lock_id, int *mode, char *drive);
int idm_drive_lock_mode_async(char *lock_id, char *drive, uint64_t *handle);
int idm_drive_lock_mode_async_result(char *drive, uint64_t handle, int *mode,
                                     int *result);
int idm_drive_read_group(char *drive, struct idm_info **info_ptr,
                         int *info_num);
int idm_drive_destroy_lock(char *lock_id, int mode, char *host_id, char *drive);
int idm_drive_destroy_lock_async(char *lock_id, int mode, char *host_id, char *drive, uint64_t *handle);
int idm_drive_async_result(char *drive, uint64_t handle, int *result);
int idm_drive_host_state(char *lock_id, char *host_id, int *host_state,
                         char *drive);

int idm_drive_whitelist(char *drive, char **whitelist, int *whitelist_num);

int idm_drive_get_fd(char *drive, uint64_t handle);

void idm_drive_free_async_result(char *drive, uint64_t handle);

int  idm_environ_init(void);
void idm_environ_destroy(void);

#endif //__IDM_API_H__
