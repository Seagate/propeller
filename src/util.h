#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <uuid/uuid.h>

uint64_t ilm_curr_time(void);
int ilm_rand(int min, int max);
int ilm_read_blk_uuid(char *dev, uuid_t *uuid);

#endif
