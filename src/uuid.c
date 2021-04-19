/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2006 Red Hat, Inc. All rights reserved.
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 *
 * Derived from the lvm2 file lib/uuid/uuid.c.
 */

#include <string.h>

#include "log.h"
#include "uuid.h"

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
