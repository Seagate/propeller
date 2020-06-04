#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <uuid/uuid.h>

#include "ilm_internal.h"

#ifndef TEST
uint64_t ilm_read_utc_time(void) __maybe_unused;
#else
static inline uint64_t ilm_read_utc_time(void) { return 0; }
#endif

uint64_t ilm_curr_time(void);
int ilm_rand(int min, int max);
int ilm_read_blk_uuid(char *dev, uuid_t *uuid);
char *ilm_convert_sg(char *blk_dev);

#endif
