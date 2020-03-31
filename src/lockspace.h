#ifndef __LOCKSPACE_H__
#define __LOCKSPACE_H__

struct ilm_lockspace;
struct ilm_lock;

int ilm_lockspace_create(struct ilm_cmd *cmd, struct ilm_lockspace **ls_out);
int ilm_lockspace_delete(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls);
int ilm_lockspace_add_lock(struct ilm_lockspace *ls,
			   struct ilm_lock *lock);
int ilm_lockspace_del_lock(struct ilm_lockspace *ls, struct ilm_lock *lock);
int ilm_lockspace_find_lock(struct ilm_lockspace *ls, char *lock_uuid,
			    struct ilm_lock **lock);

#endif /* __LOCKSPACE_H__ */
