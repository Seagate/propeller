/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __CMD_H__
#define __CMD_H__

#include <stdint.h>

#include "list.h"

enum {
	ILM_CMD_ADD_LOCKSPACE,
	ILM_CMD_DEL_LOCKSPACE,
	ILM_CMD_ACQUIRE,
	ILM_CMD_RELEASE,
	ILM_CMD_CONVERT,
	ILM_CMD_WRITE_LVB,
	ILM_CMD_READ_LVB,
};

struct ilm_cmd {
	struct list_head list; /* thread_pool data */
	struct client *cl;
	uint32_t cmd;
	int sock_msg_len;
};

int ilm_cmd_queue_add_work(struct ilm_cmd *cmd);
void ilm_cmd_queue_free(void);
int ilm_cmd_queue_create(void);

#endif
