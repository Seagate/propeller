/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 */

#include <stdio.h>
#include <string.h>

#include "utils_nvme.h"

/**
 * ilm_nvme_dir_select - Helper function for finding active nvme devices
 *
 * Search for "nvmeXnX", where "X" is any integer.
 * Ignore "nvmeX" and "nmveXnXpX".
 */
int ilm_nvme_dir_select(const struct dirent *s)
{
//TODO: Do I only need 1 ptr here?  Test it
	char *token1;
	char *token2;

	token1 = strstr(s->d_name, NVME_NAME);
	if (token1 != NULL) {
		token1 += strlen(NVME_NAME);

		//want namespace char
		token2 = strstr(token1, NVME_NAMESPACE_TAG);
		if (token2 != NULL) {
			token2 += strlen(NVME_NAMESPACE_TAG);

			//want no partition char
			token1 = strstr(token2, NVME_PARTITION_TAG);
			if (token1 == NULL) {
				return 1;
			}
		}

	}

	return 0;
}
