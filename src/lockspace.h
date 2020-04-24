#ifndef __LOCKSPACE_H__
#define __LOCKSPACE_H__

#include <pthread.h>

#include "ilm.h"
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

	struct _raid_thread *raid_thd;

	char *kill_path;
	char *kill_args;
	int kill_pid;
	char kill_sig;
	int failed;

	/* Testing purpose */
	int stop_renew;
};

struct ilm_lock;

int ilm_lockspace_create(struct ilm_cmd *cmd, struct ilm_lockspace **ls_out);
int ilm_lockspace_delete(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls);
int ilm_lockspace_set_host_id(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls);
int ilm_lockspace_add_lock(struct ilm_lockspace *ls,
			   struct ilm_lock *lock);
int ilm_lockspace_del_lock(struct ilm_lockspace *ls, struct ilm_lock *lock);
int ilm_lockspace_start_lock(struct ilm_lockspace *ls, struct ilm_lock *lock);
int ilm_lockspace_stop_lock(struct ilm_lockspace *ls, struct ilm_lock *lock);
int ilm_lockspace_set_signal(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lockspace_set_killpath(struct ilm_cmd *cmd, struct ilm_lockspace *ls);
int ilm_lockspace_find_lock(struct ilm_lockspace *ls, char *lock_uuid,
			    struct ilm_lock **lock);
int ilm_lockspace_stop_renew(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls);
int ilm_lockspace_start_renew(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls);
int ilm_lockspace_terminate(struct ilm_lockspace *ls);

#endif /* __LOCKSPACE_H__ */
