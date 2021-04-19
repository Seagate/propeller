/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
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
