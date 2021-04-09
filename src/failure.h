/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __FAILURE_H__
#define __FAILURE_H__

#include "lockspace.h"

int ilm_failure_handler(struct ilm_lockspace *ls);

#endif
