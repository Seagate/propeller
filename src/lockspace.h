#ifndef __LOCKSPACE_H__
#define __LOCKSPACE_H__

#include <pthread.h>

#include "cmd.h"
#include "list.h"

#define IDM_HOST_ID_LEN			32

struct ilm_lockspace {
	struct list_head list;
	char host_id[IDM_HOST_ID_LEN];

	struct list_head lock_list;

	int exit;
	pthread_t thd;
	pthread_mutex_t mutex;

	/* TODO: support event and timeout */
};

struct ilm_lock;

int ilm_lockspace_create(struct ilm_cmd *cmd, struct ilm_lockspace **ls_out);
int ilm_lockspace_delete(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls);
int ilm_lockspace_add_lock(struct ilm_lockspace *ls,
			   struct ilm_lock *lock);
int ilm_lockspace_del_lock(struct ilm_lockspace *ls, struct ilm_lock *lock);
int ilm_lockspace_find_lock(struct ilm_lockspace *ls, char *lock_uuid,
			    struct ilm_lock **lock);

#endif /* __LOCKSPACE_H__ */
