%module ilm

%include "typemaps.i"
%include "stdint.i"

%apply int *OUTPUT { int *sock };
%apply int *OUTPUT { int *count};
%apply int *OUTPUT { int *mode };

%{
#define ILM_DRIVE_MAX_NUM       32

struct idm_lock_id {
        char vg_uuid[16];
        char lv_uuid[16];
};

struct idm_lock_op {
        uint32_t mode;

        uint32_t drive_num;
        char *drives[ILM_DRIVE_MAX_NUM];

        int timeout; /* -1 means unlimited timeout */
};

int ilm_connect(int *sock);
int ilm_disconnect(int sock);
int ilm_lock(int sock, struct idm_lock_id *id, struct idm_lock_op *op);
int ilm_unlock(int sock, struct idm_lock_id *id);
int ilm_convert(int sock, struct idm_lock_id *id, uint32_t mode);
int ilm_write_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);
int ilm_read_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);
int ilm_get_host_count(int sock, struct idm_lock_id *id,
                       struct idm_lock_op *op, int *count);
int ilm_get_mode(int sock, struct idm_lock_id *id,
                 struct idm_lock_op *op, int *mode);
int ilm_set_host_id(int sock, char *id, int id_len);
int ilm_stop_renew(int sock);
int ilm_start_renew(int sock);
int ilm_inject_fault(int sock, int percentage);
%}

#define ILM_DRIVE_MAX_NUM       32

#define IDM_MODE_UNLOCK         0
#define IDM_MODE_EXCLUSIVE      1
#define IDM_MODE_SHAREABLE      2

struct idm_lock_id {
        char vg_uuid[16];
        char lv_uuid[16];
};

struct idm_lock_op {
        int mode;

        int drive_num;
        char *drives[ILM_DRIVE_MAX_NUM];

        int timeout; /* -1 means unlimited timeout */
};

int ilm_connect(int *sock);
int ilm_disconnect(int sock);
int ilm_lock(int sock, struct idm_lock_id *id, struct idm_lock_op *op);
int ilm_unlock(int sock, struct idm_lock_id *id);
int ilm_convert(int sock, struct idm_lock_id *id, uint32_t mode);
int ilm_write_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);
int ilm_read_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len);
int ilm_get_host_count(int sock, struct idm_lock_id *id,
                       struct idm_lock_op *op, int *count);
int ilm_get_mode(int sock, struct idm_lock_id *id,
                 struct idm_lock_op *op, int *mode);
int ilm_set_host_id(int sock, char *id, int id_len);
int ilm_stop_renew(int sock);
int ilm_start_renew(int sock);
int ilm_inject_fault(int sock, int percentage);

%extend idm_lock_op {
        void set_drive_names(int i, char *path) {
                $self->drives[i] = strdup(path);
        }
}

%extend idm_lock_id {
        void set_vg_uuid(char *id) {
                memcpy($self->vg_uuid, id, 16);
        }

        void set_lv_uuid(char *id) {
                memcpy($self->lv_uuid, id, 16);
        }
}
