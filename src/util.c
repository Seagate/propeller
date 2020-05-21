/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <blkid/blkid.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "log.h"
#include "util.h"

uint64_t ilm_read_utc_time(void)
{
	struct timeval cur_time;
	uint64_t utc_ms;

	gettimeofday(&cur_time, NULL);

	utc_ms = (uint64_t)(cur_time.tv_sec) * 1000 +
		 (uint64_t)(cur_time.tv_usec) / 1000;
	return utc_ms;
}

/* Get current time in millisecond */
uint64_t ilm_curr_time(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int ilm_rand(int min, int max)
{
        long int ret;

        ret = random();
        if (ret < 0)
                return min;

        return min + (int)(((float)(max - min + 1)) * ret / (RAND_MAX + 1.0));
}

int ilm_read_blk_uuid(char *dev, uuid_t *uuid)
{
#ifdef IDM_PTHREAD_EMULATION
	uuid_t id;

	uuid_generate(id);
	memcpy(uuid, &id, sizeof(uuid_t));
	return 0;
#else
	blkid_probe probe;
	const char *uuid_str;
	size_t uuid_str_size;
	uuid_t id;
	int ret;

	probe = blkid_new_probe_from_filename(dev);
	if (!probe) {
		ilm_log_err("fail to create blkid probe for %s", dev);
		return -1;
	}

	blkid_do_probe(probe);

	ret = blkid_probe_lookup_value(probe, "UUID",
				       &uuid_str, &uuid_str_size);
	if (ret) {
		ilm_log_err("fail to lookup blkid value %s", dev);
		goto out;
	}

	uuid_parse(uuid_str, id);

	memcpy(uuid, &id, sizeof(uuid_t));
out:
	blkid_free_probe(probe);
	return ret;
#endif
}
