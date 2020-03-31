
#ifndef __ILM_H__
#define __ILM_H__

#include <stdio.h>
#include <uuid/uuid.h>

#define ILM_DRIVE_MAX_NUM	32

struct idm_lock_id {
	uuid_t vg_uuid;
	uuid_t lv_uuid;
};

struct idm_lock_op {
	uint32_t mode;

	uint32_t drive_num;
	char *drives[ILM_DRIVE_MAX_NUM];

	int timeout; /* -1 means unlimited timeout */
	int quiescent;
};

int ilm_connect(int *sock);
int ilm_disconnect(int sock);

#endif /* __ILM_H__ */
