#ifndef __IDM_WRAPPER_H__
#define __IDM_WRAPPER_H__

#include <stdint.h>

/*
 * NOTE: assumes the functions idm_drive_init() and idm_drive_destroy()
 * will be used internally in IDM wrapper layer; so this can be transparent
 * to upper layer and reduce the complexity in raid lock layer.
 */
#if 0
int idm_drive_init(char *lock_id, char *host_id, char *drive);
int idm_drive_destroy(char *lock_id, char *host_id, char *drive);
#endif

int idm_drive_lock(char *lock_id, int mode, char *host_id,
                   char *drive, uint64_t timeout);
int idm_drive_unlock(char *lock_id, char *host_id,
		     void *lvb, int lvb_size, char *drive);
int idm_drive_convert_lock(char *lock_id, int mode,
                           char *host_id, char *drive);
int idm_drive_renew_lock(char *lock_id, int mode,
                         char *host_id, char *drive);
int idm_drive_break_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout);
#if 0
int idm_drive_write_lvb(char *lock_id, char *host_id,
			void *lvb, int lvb_size, char *drive);
#endif
int idm_drive_read_lvb(char *lock_id, char *host_id,
		       void *lvb, int lvb_size, char *drive);
int idm_drive_lock_count(char *lock_id, int *count, char *drive);
int idm_drive_lock_mode(char *lock_id,
			int *mode,
			char *drive);
int idm_drive_host_state(char *lock_id,
			 char *host_id,
			 int *host_state,
			 char *drive);
int idm_drive_whitelist(char *drive,
                        char **whitelist,
                        int *whitelist_num);

#endif
