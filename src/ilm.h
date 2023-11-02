/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __ILM_H__
#define __ILM_H__

#include <stdint.h>
#include <stdio.h>
#include <uuid/uuid.h>

#define ILM_DRIVE_MAX_NUM		512

#define IDM_MODE_UNLOCK			0
#define IDM_MODE_EXCLUSIVE		1
#define IDM_MODE_SHAREABLE		2

#define IDM_FAILURE_PATH_LEN		128
#define IDM_FAILURE_ARGS_LEN		128

#define IDM_VALUE_LEN			8

struct idm_lock_id {
	char vg_uuid[32];
	char lv_uuid[32];
};

struct idm_lock_op {
	uint32_t mode;

	uint32_t drive_num;
	char *drives[ILM_DRIVE_MAX_NUM];

	int timeout; /* -1 means unlimited timeout */
};

extern uuid_t ilm_uuid;
int ilm_connect(int *sock);
int ilm_disconnect(int sock);
int ilm_version(int sock, char *drive, uint8_t *version_major, uint8_t *version_minor);
int ilm_lock(int sock, struct idm_lock_id *id, struct idm_lock_op *op);
int ilm_unlock(int sock, struct idm_lock_id *id);
int ilm_convert(int sock, struct idm_lock_id *id, uint32_t mode);
int ilm_write_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);
int ilm_read_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);
int ilm_set_signal(int sock, int signo);
int ilm_set_killpath(int sock, char *killpath, char *killargs);
int ilm_get_host_count(int sock, struct idm_lock_id *id,
		       struct idm_lock_op *op, int *count, int *self);
int ilm_get_mode(int sock, struct idm_lock_id *id,
		 struct idm_lock_op *op, int *mode);
int ilm_set_host_id(int sock, char *id, int id_len);
int ilm_stop_renew(int sock);
int ilm_start_renew(int sock);
int ilm_inject_fault(int sock, int percentage);

#endif /* __ILM_H__ */
