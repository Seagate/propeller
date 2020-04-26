#ifndef __IDM_WRAPPER_H__
#define __IDM_WRAPPER_H__

#include <stdint.h>

#define IDM_LOCK_ID_LEN			64
#define IDM_HOST_ID_LEN			32
#define IDM_VALUE_LEN			8

struct idm_info {
	/* Lock ID */
	char id[IDM_LOCK_ID_LEN];
	int mode;

	/* Host ID */
	char host_id[IDM_HOST_ID_LEN];

	/* Membership */
	uint64_t last_renew_time;
	int timeout;
};

/*
 * NOTE: assumes the functions idm_drive_init() and idm_drive_destroy()
 * will be used internally in IDM wrapper layer; so this can be transparent
 * to upper layer and reduce the complexity in raid lock layer.
 */
#if 0
int idm_drive_init(char *lock_id, char *host_id, char *drive);
int idm_drive_destroy(char *lock_id, char *host_id, char *drive);
#endif

int idm_drive_version(int *version, char *drive);
int idm_drive_lock(char *lock_id, int mode, char *host_id,
                   char *drive, uint64_t timeout);
int idm_drive_lock_async(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout, int *fd);
int idm_drive_unlock(char *lock_id, char *host_id,
		     void *lvb, int lvb_size, char *drive);
int idm_drive_unlock_async(char *lock_id, char *host_id,
			   void *lvb, int lvb_size, char *drive, int *fd);
int idm_drive_convert_lock(char *lock_id, int mode,
                           char *host_id, char *drive);
int idm_drive_convert_lock_async(char *lock_id, int mode, char *host_id,
				 char *drive, int *fd);
int idm_drive_renew_lock(char *lock_id, int mode,
                         char *host_id, char *drive);
int idm_drive_renew_lock_async(char *lock_id, int mode,
			       char *host_id, char *drive, int *fd);
int idm_drive_break_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout);
int idm_drive_break_lock_async(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, int *fd);
#if 0
int idm_drive_write_lvb(char *lock_id, char *host_id,
			void *lvb, int lvb_size, char *drive);
int idm_drive_write_lvb_async(char *lock_id, char *host_id,
			      void *lvb, int lvb_size,
			      char *drive, int *fd);
#endif
int idm_drive_read_lvb(char *lock_id, char *host_id,
		       void *lvb, int lvb_size, char *drive);
int idm_drive_read_lvb_async(char *lock_id, char *host_id,
			     char *drive, int *fd);
int idm_drive_read_lvb_async_result(int fd, void *lvb, int lvb_size,
				    int *result);
int idm_drive_lock_count(char *lock_id, int *count, char *drive);
int idm_drive_lock_count_async(char *lock_id, char *drive, int *fd);
int idm_drive_lock_count_async_result(int fd, int *count, int *result);
int idm_drive_lock_mode(char *lock_id, int *mode, char *drive);
int idm_drive_lock_mode_async(char *lock_id, char *drive, int *fd);
int idm_drive_lock_mode_async_result(int fd, int *mode, int *result);
int idm_drive_read_group(char *drive, struct idm_info **info_ptr, int *info_num);

int idm_drive_async_result(int fd, int *result);

int idm_drive_host_state(char *lock_id,
			 char *host_id,
			 int *host_state,
			 char *drive);
int idm_drive_whitelist(char *drive,
                        char **whitelist,
                        int *whitelist_num);

#endif
