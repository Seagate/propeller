/*
 * Copyright (C) 2020-2021 Seagate
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <blkid/blkid.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

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

int ilm_id_write_format(const char *id, char *buffer, size_t size)
{
	int i, tot;

	static const unsigned group_size[] = { 6, 4, 4, 4, 4, 4, 6 };

	/* split into groups separated by dashes */
	if (size < (32 + 6 + 1)) {
		if (size > 0)
			buffer[0] = '\0';
		ilm_log_err("Couldn't write uuid, buffer too small.");
		return -1;
	}

	for (i = 0, tot = 0; i < 7; i++) {
		memcpy(buffer, id + tot, group_size[i]);
		buffer += group_size[i];
		tot += group_size[i];
		*buffer++ = '-';
	}

	*--buffer = '\0';
	return 0;
}
