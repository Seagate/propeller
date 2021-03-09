#ifndef __DRIVE_H__
#define __DRIVE_H__

#include <stdint.h>
#include <uuid/uuid.h>

char *ilm_find_cached_device_mapping(char *dev_map,
				     unsigned long *wwn);
int ilm_add_cached_device_mapping(char *dev_map, char *sg_path,
				  unsigned long wwn);

int ilm_read_blk_uuid(char *dev, uuid_t *uuid);
char *ilm_convert_sg(char *blk_dev);
int ilm_read_parttable_id(char *dev, uuid_t *uuid);
int ilm_read_device_wwn(char *dev, unsigned long *wwn);
char *ilm_scsi_get_first_sg(char *dev);
char *ilm_scsi_convert_blk_name(char *blk_dev);
int ilm_scsi_get_part_table_uuid(char *dev, uuid_t *id);
int ilm_scsi_get_all_sgs(unsigned long wwn, char **sg_node, int sg_num);
int ilm_scsi_list_init(void);
void ilm_scsi_list_exit(void);
int ilm_scsi_drive_version(void);

#endif /* __DRIVE_H__ */
