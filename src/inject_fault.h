/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __INJECT_FAULT_H__
#define __INJECT_FAULT_H__

struct ilm_cmd;

#ifndef TEST

int ilm_inject_fault_set_percentage(struct ilm_cmd *cmd);
void ilm_inject_fault_update(int total, int index);
int ilm_inject_fault_is_hit(void);

#else

static inline int ilm_inject_fault_set_percentage(struct ilm_cmd *cmd)
{ return 0; }
static inline void ilm_inject_fault_update(int total, int index) { }
static inline int ilm_inject_fault_is_hit(void) { return 0; }

#endif

#endif
