/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __UUID_H__
#define __UUID_H__

#include <stddef.h>
#include <stdint.h>
#include <uuid/uuid.h>

int ilm_id_write_format(const char *id, char *buffer, size_t size);

#endif /* __UUID_H__ */
