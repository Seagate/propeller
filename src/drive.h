#ifndef __DRIVE_H__
#define __DRIVE_H__

#include <stdint.h>
#include <uuid/uuid.h>

int ilm_read_blk_uuid(char *dev, uuid_t *uuid);
char *ilm_convert_sg(char *blk_dev);
int ilm_read_parttable_id(char *dev, uuid_t *uuid);
int ilm_scsi_list_init(void);
void ilm_scsi_list_exit(void);

#endif /* __DRIVE_H__ */
