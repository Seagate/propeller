#ifndef __LOCK_H__
#define __LOCK_H__

#include <uuid/uuid.h>

#include "ilm.h"

#define ILM_ID_LENGTH		32

struct ilm_lock {
	struct list_head list;
	char lock_id[ILM_ID_LENGTH];
	int flag;
	int mode;

	int drive_num;
	char *drive_list[ILM_DRIVE_MAX_NUM];
};

#define ILM_LOCK_MAGIC		0x4C4F434B
#define ILM_LVB_SIZE		8

struct ilm_lock_payload {
	uint32_t magic;
	uint32_t mode;
	uint32_t drive_num;
	char lock_id[ILM_ID_LENGTH];
	int timeout;
	int quiescent;
};

int ilm_lock_acquire(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_release(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_convert_mode(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_vb_write(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lock_vb_read(struct ilm_cmd *cmd, struct ilm_lockspace *ls);

#endif /* __LOCK_H__ */
