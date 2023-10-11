/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __IDM_SCSI_H__
#define __IDM_SCSI_H__

#include <stdint.h>

#include "idm_cmd_common.h"

#define IDM_LOCK_ID_LEN			64
#define IDM_HOST_ID_LEN			32
#define IDM_VALUE_LEN			8

int scsi_idm_read_version(char *drive, uint8_t *version_major, uint8_t *version_minor);
int scsi_idm_sync_lock(char *lock_id, int mode, char *host_id,
                   char *drive, uint64_t timeout);
int scsi_idm_async_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout, uint64_t *handle);
int scsi_idm_sync_unlock(char *lock_id, int mode, char *host_id,
		     char *lvb, int lvb_size, char *drive);
int scsi_idm_async_unlock(char *lock_id, int mode, char *host_id,
			   char *lvb, int lvb_size, char *drive,
			   uint64_t *handle);
int scsi_idm_sync_lock_convert(char *lock_id, int mode, char *host_id,
			   char *drive, uint64_t timeout);
int scsi_idm_async_lock_convert(char *lock_id, int mode, char *host_id,
				 char *drive, uint64_t timeout,
				 uint64_t *handle);
int scsi_idm_sync_lock_renew(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout);
int scsi_idm_async_lock_renew(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle);
int scsi_idm_sync_lock_break(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout);
int scsi_idm_async_lock_break(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle);

int scsi_idm_sync_read_lvb(char *lock_id, char *host_id,
		       char *lvb, int lvb_size, char *drive);
int scsi_idm_async_read_lvb(char *lock_id, char *host_id,
			     char *drive, uint64_t *handle);
int scsi_idm_async_get_result_lvb(uint64_t handle, char *lvb, int lvb_size,
				    int *result);
int scsi_idm_sync_read_lock_count(char *lock_id, char *host_id,
			 int *count, int *self, char *drive);
int scsi_idm_async_read_lock_count(char *lock_id, char *host_id,
			       char *drive, uint64_t *handle);
int scsi_idm_async_get_result_lock_count(uint64_t handle, int *count,
				      int *self, int *result);
int scsi_idm_sync_read_lock_mode(char *lock_id, int *mode, char *drive);
int scsi_idm_async_read_lock_mode(char *lock_id, char *drive, uint64_t *handle);
int scsi_idm_async_get_result_lock_mode(uint64_t handle, int *mode, int *result);
int scsi_idm_sync_read_mutex_group(char *drive, struct idm_info **info_ptr, int *info_num);
int scsi_idm_sync_lock_destroy(char *lock_id, int mode, char *host_id, char *drive);
int scsi_idm_async_lock_destroy(char *lock_id, int mode, char *host_id, char *drive, uint64_t *handle);

int scsi_idm_async_get_result(uint64_t handle, int *result);

int scsi_idm_sync_read_host_state(char *lock_id,
			 char *host_id,
			 int *host_state,
			 char *drive);
int scsi_idm_read_whitelist(char *drive,
                        char **whitelist,
                        int *whitelist_num);

int scsi_idm_get_fd(uint64_t handle);

void scsi_idm_async_free_result(uint64_t handle);

#endif //__IDM_SCSI_H__
