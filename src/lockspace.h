#ifndef __LOCKSPACE_H__
#define __LOCKSPACE_H__

struct ilm_lockspace;

int ilm_lockspace_create(struct ilm_cmd *cmd, struct ilm_lockspace **ls_out);
int ilm_lockspace_delete(struct ilm_lockspace *ilm_ls);

#endif /* __LOCKSPACE_H__ */
