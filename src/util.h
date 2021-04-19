/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

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

#endif
