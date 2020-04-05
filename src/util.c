/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <stdlib.h>
#include <time.h>

#include "util.h"

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
