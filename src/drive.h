#ifndef __DRIVE_H__
#define __DRIVE_H__

#include <stdint.h>
#include <uuid/uuid.h>

char *ilm_find_cached_device_mapping(char *dev_map, uuid_t *id);
int ilm_add_cached_device_mapping(char *dev_map, char *sg_path, uuid_t *id);

int ilm_read_blk_uuid(char *dev, uuid_t *uuid);
char *ilm_convert_sg(char *blk_dev);
int ilm_read_parttable_id(char *dev, uuid_t *uuid);
char *ilm_scsi_get_first_sg(char *dev);
char *ilm_scsi_convert_blk_name(char *blk_dev);
int ilm_scsi_get_part_table_uuid(char *dev, uuid_t *id);
int ilm_scsi_get_all_sgs(uuid_t id, char **sg_node, int sg_num);
int ilm_scsi_list_init(void);
void ilm_scsi_list_exit(void);

#endif /* __DRIVE_H__ */
