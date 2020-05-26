%module idm_scsi

%include "typemaps.i"
%include "stdint.i"
%include "carrays.i"

%array_class(char, charArray);

%apply int *OUTPUT { int *version };

%{

#define IDM_LOCK_ID_LEN         64
#define IDM_HOST_ID_LEN         32
#define IDM_VALUE_LEN           8

#define IDM_MODE_UNLOCK		0
#define IDM_MODE_EXCLUSIVE	1
#define IDM_MODE_SHAREABLE	2

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

int idm_drive_version(int *version, char *drive);
int idm_drive_lock(char *lock_id, int mode, char *host_id,
                   char *drive, uint64_t timeout);
int idm_drive_lock_async(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout, uint64_t *handle);
int idm_drive_unlock(char *lock_id, char *host_id,
		     void *lvb, int lvb_size, char *drive);
int idm_drive_unlock_async(char *lock_id, char *host_id,
			   void *lvb, int lvb_size, char *drive,
			   uint64_t *handle);
int idm_drive_convert_lock(char *lock_id, int mode,
                           char *host_id, char *drive);
int idm_drive_convert_lock_async(char *lock_id, int mode, char *host_id,
				 char *drive, uint64_t *handle);
int idm_drive_renew_lock(char *lock_id, int mode,
                         char *host_id, char *drive);
int idm_drive_renew_lock_async(char *lock_id, int mode,
			       char *host_id, char *drive, uint64_t *handle);
int idm_drive_break_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout);
int idm_drive_break_lock_async(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle);
int idm_drive_read_lvb_async(char *lock_id, char *host_id,
			     char *drive, uint64_t *handle);
int idm_drive_read_lvb_async_result(uint64_t handle, void *lvb, int lvb_size,
				    int *result);
int idm_drive_lock_count(char *lock_id, char *host_id,
			 int *count, int *self, char *drive);
int idm_drive_lock_count_async(char *lock_id, char *host_id,
			       char *drive, uint64_t *handle);
int idm_drive_lock_count_async_result(uint64_t handle, int *count,
				      int *self, int *result);
int idm_drive_lock_mode(char *lock_id, int *mode, char *drive);
int idm_drive_lock_mode_async(char *lock_id, char *drive, uint64_t *handle);
int idm_drive_lock_mode_async_result(uint64_t handle, int *mode, int *result);
int idm_drive_read_group(char *drive, struct idm_info **info_ptr, int *info_num);
int idm_drive_destroy(char *lock_id, char *drive);

int idm_drive_async_result(uint64_t handle, int *result);

int idm_drive_host_state(char *lock_id,
			 char *host_id,
			 int *host_state,
			 char *drive);
int idm_drive_whitelist(char *drive,
                        char **whitelist,
                        int *whitelist_num);

int idm_drive_get_fd(uint64_t handle);

%}

#define IDM_LOCK_ID_LEN         64
#define IDM_HOST_ID_LEN         32
#define IDM_VALUE_LEN           8

#define IDM_MODE_UNLOCK		0
#define IDM_MODE_EXCLUSIVE	1
#define IDM_MODE_SHAREABLE	2

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

int idm_drive_version(int *version, char *drive);
int idm_drive_lock(char *lock_id, int mode, char *host_id,
                   char *drive, uint64_t timeout);
int idm_drive_lock_async(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout, uint64_t *handle);
int idm_drive_unlock(char *lock_id, char *host_id,
		     void *lvb, int lvb_size, char *drive);
int idm_drive_unlock_async(char *lock_id, char *host_id,
			   void *lvb, int lvb_size, char *drive,
			   uint64_t *handle);
int idm_drive_convert_lock(char *lock_id, int mode,
                           char *host_id, char *drive);
int idm_drive_convert_lock_async(char *lock_id, int mode, char *host_id,
				 char *drive, uint64_t *handle);
int idm_drive_renew_lock(char *lock_id, int mode,
                         char *host_id, char *drive);
int idm_drive_renew_lock_async(char *lock_id, int mode,
			       char *host_id, char *drive, uint64_t *handle);
int idm_drive_break_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout);
int idm_drive_break_lock_async(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle);
int idm_drive_read_lvb_async(char *lock_id, char *host_id,
			     char *drive, uint64_t *handle);
int idm_drive_read_lvb_async_result(uint64_t handle, void *lvb, int lvb_size,
				    int *result);
int idm_drive_lock_count(char *lock_id, char *host_id,
			 int *count, int *self, char *drive);
int idm_drive_lock_count_async(char *lock_id, char *host_id,
			       char *drive, uint64_t *handle);
int idm_drive_lock_count_async_result(uint64_t handle, int *count,
				      int *self, int *result);
int idm_drive_lock_mode(char *lock_id, int *mode, char *drive);
int idm_drive_lock_mode_async(char *lock_id, char *drive, uint64_t *handle);
int idm_drive_lock_mode_async_result(uint64_t handle, int *mode, int *result);
int idm_drive_read_group(char *drive, struct idm_info **info_ptr, int *info_num);
int idm_drive_destroy(char *lock_id, char *drive);

int idm_drive_async_result(uint64_t handle, int *result);

int idm_drive_host_state(char *lock_id,
			 char *host_id,
			 int *host_state,
			 char *drive);
int idm_drive_whitelist(char *drive,
                        char **whitelist,
                        int *whitelist_num);

int idm_drive_get_fd(uint64_t handle);
