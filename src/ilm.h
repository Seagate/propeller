
#ifndef __ILM_H__
#define __ILM_H__

#include <stdint.h>
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
int ilm_lock(int sock, struct idm_lock_id *id, struct idm_lock_op *op);
int ilm_unlock(int sock, struct idm_lock_id *id);
int ilm_convert(int sock, struct idm_lock_id *id, uint32_t mode);
int ilm_write_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);
int ilm_read_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);

#endif /* __ILM_H__ */
