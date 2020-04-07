%module ilm

%include "typemaps.i"

%apply int *OUTPUT { int *sock };

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
int ilm_set_host_id(int sock, char *id, int id_len);
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
int ilm_set_host_id(int sock, char *id, int id_len);

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
