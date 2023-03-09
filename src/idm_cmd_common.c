/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_cmd_common.c - In-drive Mutex (IDM) related functions that are common to SCSI and NVMe.
 */

#include "idm_cmd_common.h"


//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * bswap_char_arr - Convenience function for swaping the byte order in a char array.
 *
 * @dest: Destination of byte-swapped data.
 * @src:  Source of data to be byte-spwapped.
 * @len:  Length of source char array to be byte-spwapped.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
void bswap_char_arr(char *dest, char *src, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		dest[i] = src[len - i - 1];
	}
}
