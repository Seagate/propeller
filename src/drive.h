#ifndef __DRIVE_H__
#define __DRIVE_H__

#include <stdint.h>
#include <uuid/uuid.h>

int ilm_read_blk_uuid(char *dev, uuid_t *uuid);
char *ilm_convert_sg(char *blk_dev);

#endif /* __DRIVE_H__ */
