#ifndef __LOCK_H__
#define __LOCK_H__

#include <uuid/uuid.h>

#include "cmd.h"
#include "ilm.h"
#include "list.h"
#include "lockspace.h"

#define IDM_LOCK_ID_LEN			64
#define IDM_VALUE_LEN			8

struct ilm_drive {
	int state;
	char *path;
};

#define ILM_DRIVE_NO_ACCESS		0
#define ILM_DRIVE_ACCESSED		1
#define ILM_DRIVE_FAILED		2

struct ilm_lock {
	struct list_head list;
	pthread_mutex_t mutex;
	char id[IDM_LOCK_ID_LEN];
	int mode;
	int timeout;
	uint64_t last_renewal_success;

	int drive_num;
	struct ilm_drive drive[ILM_DRIVE_MAX_NUM];

	char vb[IDM_VALUE_LEN];

	int convert_failed;
};

#define ILM_LOCK_MAGIC		0x4C4F434B
#define ILM_LVB_SIZE		8

struct ilm_lock_payload {
	uint32_t magic;
	uint32_t mode;
	uint32_t drive_num;
	char lock_id[IDM_LOCK_ID_LEN];
	int timeout;
};

int ilm_lock_acquire(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_release(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_convert_mode(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_vb_write(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_vb_read(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_host_count(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_mode(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_terminate(struct ilm_lockspace *ls, struct ilm_lock *lock);

#endif /* __LOCK_H__ */
