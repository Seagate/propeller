/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * Derived from the sanlock file of the same name.
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
#include "inject_fault.h"
#include "list.h"
#include "lockspace.h"
#include "lock.h"
#include "log.h"
#include "util.h"

#define ILM_MIN_WORKER_THREADS		2
#define ILM_MAX_WORKER_THREADS		8

const char *CMD_NAMES[16] = {
	"ILM_CMD_VERSION",
	"ILM_CMD_ADD_LOCKSPACE",
	"ILM_CMD_DEL_LOCKSPACE",
	"ILM_CMD_ACQUIRE",
	"ILM_CMD_RELEASE",
	"ILM_CMD_CONVERT",
	"ILM_CMD_WRITE_LVB",
	"ILM_CMD_READ_LVB",
	"ILM_CMD_LOCK_HOST_COUNT",
	"ILM_CMD_LOCK_MODE",
	"ILM_CMD_SET_SIGNAL",
	"ILM_CMD_SET_KILLPATH",
	"ILM_CMD_SET_HOST_ID",
	"ILM_CMD_STOP_RENEW",
	"ILM_CMD_START_RENEW",
	"ILM_CMD_INJECT_FAULT"
};

struct ilm_cmd_queue {
	int exit;
	struct list_head list;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_cond_t exit_wait;

	int num_workers;
	int max_workers;
	int free_workers;
};

static struct ilm_cmd_queue cmd_queue;

static void ilm_cmd_version(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_version(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to read version\n");
}

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

static void ilm_cmd_set_signal(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lockspace_set_signal(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to set signal\n");
}

static void ilm_cmd_set_killpath(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lockspace_set_killpath(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to set killpath\n");
}

static void ilm_cmd_set_host_id(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lockspace_set_host_id(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to set host ID\n");
}

static void ilm_cmd_stop_renew(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lockspace_stop_renew(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to set host ID\n");
}

static void ilm_cmd_start_renew(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lockspace_start_renew(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to set host ID\n");
}

static void ilm_cmd_acquire(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_acquire(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to acquire IDM\n");
}

static void ilm_cmd_release(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_release(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to release IDM\n");
}

static void ilm_cmd_convert(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_convert_mode(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to convert IDM mode\n");
}

static void ilm_cmd_lvb_write(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_vb_write(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to write LVB\n");
}

static void ilm_cmd_lvb_read(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_vb_read(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to read LVB\n");
}

static void ilm_cmd_lock_host_count(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_host_count(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to read host count\n");
}

static void ilm_cmd_lock_mode(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_lock_mode(cmd, cmd->cl->ls);
	if (ret < 0)
		ilm_log_err("Fail to read host count\n");
}

static void ilm_cmd_inject_fault(struct ilm_cmd *cmd)
{
	int ret;

	ret = ilm_inject_fault_set_percentage(cmd);
	if (ret < 0)
		ilm_log_err("Fail to inject failure\n");
}

static void ilm_cmd_handle(struct ilm_cmd *cmd)
{
	ilm_log_dbg("cmd=%d (%s)", cmd->cmd, CMD_NAMES[cmd->cmd]);

	switch (cmd->cmd) {
	case ILM_CMD_VERSION:
		ilm_cmd_version(cmd);
		break;
	case ILM_CMD_ADD_LOCKSPACE:
		ilm_cmd_add_lockspace(cmd);
		break;
	case ILM_CMD_DEL_LOCKSPACE:
		ilm_cmd_del_lockspace(cmd);
		break;
	case ILM_CMD_ACQUIRE:
		ilm_cmd_acquire(cmd);
		break;
	case ILM_CMD_RELEASE:
		ilm_cmd_release(cmd);
		break;
	case ILM_CMD_CONVERT:
		ilm_cmd_convert(cmd);
		break;
	case ILM_CMD_WRITE_LVB:
		ilm_cmd_lvb_write(cmd);
		break;
	case ILM_CMD_READ_LVB:
		ilm_cmd_lvb_read(cmd);
		break;
	case ILM_CMD_LOCK_HOST_COUNT:
		ilm_cmd_lock_host_count(cmd);
		break;
	case ILM_CMD_LOCK_MODE:
		ilm_cmd_lock_mode(cmd);
		break;
	case ILM_CMD_SET_SIGNAL:
		ilm_cmd_set_signal(cmd);
		break;
	case ILM_CMD_SET_KILLPATH:
		ilm_cmd_set_killpath(cmd);
		break;
	case ILM_CMD_SET_HOST_ID:
		ilm_cmd_set_host_id(cmd);
		break;
	case ILM_CMD_STOP_RENEW:
		ilm_cmd_stop_renew(cmd);
		break;
	case ILM_CMD_START_RENEW:
		ilm_cmd_start_renew(cmd);
		break;
	case ILM_CMD_INJECT_FAULT:
		ilm_cmd_inject_fault(cmd);
		break;
	default:
		break;
	}

	ilm_client_resume(cmd->cl);
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
		while (!cmd_queue.exit && list_empty(&cmd_queue.list)) {
			cmd_queue.free_workers++;
			pthread_cond_wait(&cmd_queue.cond, &cmd_queue.mutex);
			cmd_queue.free_workers--;
		}

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

	cmd_queue.num_workers--;
	if (!cmd_queue.num_workers)
		pthread_cond_signal(&cmd_queue.exit_wait);
	pthread_mutex_unlock(&cmd_queue.mutex);

	return NULL;
}

int ilm_cmd_queue_add_work(struct ilm_cmd *cmd)
{
	pthread_t th;
	int ret;

	pthread_mutex_lock(&cmd_queue.mutex);

	if (cmd_queue.exit) {
		pthread_mutex_unlock(&cmd_queue.mutex);
		return -1;
	}

	list_add_tail(&cmd->list, &cmd_queue.list);

	if (!cmd_queue.free_workers &&
	    cmd_queue.num_workers < ILM_MAX_WORKER_THREADS) {

		ret = pthread_create(&th, NULL, ilm_cmd_thread,
				     (void *)(long)cmd_queue.num_workers);
		if (ret < 0) {
			ilm_log_err("Fail to create thread for cmd queue\n");
			list_del(&cmd->list);
			pthread_mutex_unlock(&cmd_queue.mutex);
			return ret;
		}
		cmd_queue.num_workers++;
	}

	pthread_cond_signal(&cmd_queue.cond);
	pthread_mutex_unlock(&cmd_queue.mutex);
	return 0;
}

void ilm_cmd_queue_free(void)
{
	pthread_mutex_lock(&cmd_queue.mutex);
	cmd_queue.exit = 1;
	if (cmd_queue.num_workers > 0) {
		pthread_cond_broadcast(&cmd_queue.cond);
		pthread_cond_wait(&cmd_queue.exit_wait, &cmd_queue.mutex);
	}
	pthread_mutex_unlock(&cmd_queue.mutex);
}

int ilm_cmd_queue_create(void)
{
	pthread_t th;
	int i, ret;

	memset(&cmd_queue, 0, sizeof(cmd_queue));
	INIT_LIST_HEAD(&cmd_queue.list);
	pthread_mutex_init(&cmd_queue.mutex, NULL);
	pthread_cond_init(&cmd_queue.cond, NULL);
	pthread_cond_init(&cmd_queue.exit_wait, NULL);

	for (i = 0; i < ILM_MIN_WORKER_THREADS; i++) {
		ret = pthread_create(&th, NULL, ilm_cmd_thread,
				     (void *)(long)cmd_queue.num_workers);
		if (ret < 0)
			ilm_log_err("Fail to create thread for cmd queue\n");
		cmd_queue.num_workers++;
	}

	if (ret < 0)
		ilm_cmd_queue_free();

	return ret;
}
