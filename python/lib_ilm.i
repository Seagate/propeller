%module ilm

%include "typemaps.i"

%apply int *OUTPUT { int *sock };

%{
#define ILM_DRIVE_MAX_NUM       32

struct idm_lock_id {
        char *vg_uuid;
        char *lv_uuid;
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
%}

#define ILM_DRIVE_MAX_NUM       32

struct idm_lock_id {
        char *vg_uuid;
        char *lv_uuid;
};

struct idm_lock_op {
        int mode;

        int drive_num;
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

%extend idm_lock_op {
        char **get_array_element(int i) {
                return &$self->drives[i];
        }
}
