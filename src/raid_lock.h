/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __RAID_LOCK_H__
#define __RAID_LOCK_H__

#include "lock.h"

int idm_raid_lock(struct ilm_lock *lock, char *host_id);
int idm_raid_unlock(struct ilm_lock *lock, char *host_id);
int idm_raid_convert_lock(struct ilm_lock *lock, char *host_id, int mode);
int idm_raid_renew_lock(struct ilm_lock *lock, char *host_id);
int idm_raid_destroy_lock(struct ilm_lock *lock, char *host_id);
int idm_raid_write_lvb(struct ilm_lock *lock, char *host_id,
		       char *lvb, int lvb_size);
int idm_raid_read_lvb(struct ilm_lock *lock, char *host_id,
		      char *lvb, int lvb_size);
int idm_raid_count(struct ilm_lock *lock, char *host_id, int *count, int *self);
int idm_raid_mode(struct ilm_lock *lock, int *mode);

int idm_raid_thread_create(struct _raid_thread **rth);
void idm_raid_thread_free(struct _raid_thread *raid_th);

#endif
