/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "client.h"
#include "cmd.h"
#include "list.h"
#include "lockspace.h"
#include "log.h"

struct ilm_cmd_queue {
	int exit;
	struct list_head list;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_cond_t exit_wait;
};

static struct ilm_cmd_queue cmd_queue;

static void ilm_cmd_add_lockspace(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lockspace_create(cmd, &cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to create lockspace\n");
}

static void ilm_cmd_del_lockspace(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lockspace_delete(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to delete lockspace\n");
}

static void ilm_cmd_handle(struct ilm_cmd *cmd)
{
	switch (cmd->cmd) {
	case ILM_CMD_ADD_LOCKSPACE:
		ilm_cmd_add_lockspace(cmd);
		break;
	case ILM_CMD_DEL_LOCKSPACE:
		ilm_cmd_del_lockspace(cmd);
		break;
	default:
		break;
	}

	return;
}

static void *ilm_cmd_thread(void *data)
{
	struct ilm_cmd *cmd;

	/*
	 * TODO: AIO setting
	 */

	pthread_mutex_lock(&cmd_queue.mutex);

	while (1) {
		while (!cmd_queue.exit && list_empty(&cmd_queue.list))
			pthread_cond_wait(&cmd_queue.cond, &cmd_queue.mutex);

		while (!list_empty(&cmd_queue.list)) {
			cmd = list_first_entry(&cmd_queue.list,
					       struct ilm_cmd, list);
			list_del(&cmd->list);
			pthread_mutex_unlock(&cmd_queue.mutex);

			ilm_cmd_handle(cmd);
			free(cmd);

			pthread_mutex_lock(&cmd_queue.mutex);
		}

		if (cmd_queue.exit)
			break;
	}

	pthread_cond_signal(&cmd_queue.exit_wait);
	pthread_mutex_unlock(&cmd_queue.mutex);

	return NULL;
}

int ilm_cmd_queue_add_work(struct ilm_cmd *cmd)
{
	pthread_mutex_lock(&cmd_queue.mutex);

	if (cmd_queue.exit) {
		pthread_mutex_unlock(&cmd_queue.mutex);
		return -1;
	}

	list_add_tail(&cmd->list, &cmd_queue.list);

	pthread_cond_signal(&cmd_queue.cond);
	pthread_mutex_unlock(&cmd_queue.mutex);
	return 0;
}

void ilm_cmd_queue_free(void)
{
	pthread_mutex_lock(&cmd_queue.mutex);
	cmd_queue.exit = 1;
	pthread_cond_broadcast(&cmd_queue.cond);
	pthread_cond_wait(&cmd_queue.exit_wait, &cmd_queue.mutex);
	pthread_mutex_unlock(&cmd_queue.mutex);
}

int ilm_cmd_queue_create(void)
{
	pthread_t th;
	int ret;

	memset(&cmd_queue, 0, sizeof(cmd_queue));
	INIT_LIST_HEAD(&cmd_queue.list);
	pthread_mutex_init(&cmd_queue.mutex, NULL);
	pthread_cond_init(&cmd_queue.cond, NULL);
	pthread_cond_init(&cmd_queue.exit_wait, NULL);

	ret = pthread_create(&th, NULL, ilm_cmd_thread, NULL);
	if (ret < 0)
		ilm_log_err("Fail to create thread for cmd queue\n");

	return ret;
}
