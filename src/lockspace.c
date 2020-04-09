/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <uuid/uuid.h>
#include <unistd.h>

#include "client.h"
#include "cmd.h"
#include "list.h"
#include "lock.h"
#include "log.h"
#include "raid_lock.h"
#include "util.h"

#define IDM_QUIESCENT_PERIOD	50000	/* 50 seconds */

static struct list_head ls_list = LIST_HEAD_INIT(ls_list);
static pthread_mutex_t ls_mutex = PTHREAD_MUTEX_INITIALIZER;

static int _ls_is_valid(struct ilm_lockspace *ilm_ls)
{
	struct ilm_lockspace *pos;
	int ret = 0;

	pthread_mutex_lock(&ls_mutex);

	list_for_each_entry(pos, &ls_list, list) {

		if (pos == ilm_ls) {
			ret = 1;
			break;
		}
	}

	pthread_mutex_unlock(&ls_mutex);
	return ret;
}

static void *ilm_lockspace_thread(void *data)
{
	struct ilm_lockspace *ls = data;
	struct ilm_lock *lock;
	int exit, ret, now;

	while (1) {
		pthread_mutex_lock(&ls->mutex);
		exit = ls->exit;
		pthread_mutex_unlock(&ls->mutex);

		if (exit)
			break;

		pthread_mutex_lock(&ls->mutex);

		/* Test timeout related features */
		if (ls->stop_renew)
			goto sleep_loop;

		list_for_each_entry(lock, &ls->lock_list, list) {

			now = ilm_curr_time();

			/*
			 * If an IDM has been added into lock list but has not
			 * been acquired the raid lock yet, its renewal_success
			 * is zero, so skip to renew it.
			 *
			 * If an IDM has been failed to renew for more than
			 * IDM_QUIESCENT_PERIOD, the lock manager will stop
			 * to try to renew it anymore.
			 */
			pthread_mutex_lock(&lock->mutex);
			if (!lock->last_renewal_success ||
			    now > lock->last_renewal_success +
					IDM_QUIESCENT_PERIOD) {
				pthread_mutex_unlock(&lock->mutex);
				continue;
			}
			pthread_mutex_unlock(&lock->mutex);

			ret = idm_raid_renew_lock(lock, ls->host_id);
			if (!ret) {
				pthread_mutex_lock(&lock->mutex);
				lock->last_renewal_success = ilm_curr_time();
				pthread_mutex_unlock(&lock->mutex);
			}
		}

sleep_loop:
		pthread_mutex_unlock(&ls->mutex);

		sleep(1);
	}

	return NULL;
}

int ilm_lockspace_create(struct ilm_cmd *cmd, struct ilm_lockspace **ls_out)
{
	struct ilm_lockspace *ilm_ls;
	int ret;

	ilm_ls = malloc(sizeof(struct ilm_lockspace));
	if (!ilm_ls)
		return -ENOMEM;
	memset(ilm_ls, 0, sizeof(struct ilm_lockspace));

	/*
	 * ILM UUID is copied to high 16 bytes and PID is copied into low 16
	 * bytes.
	 */
	memcpy(ilm_ls->host_id + 16, &ilm_uuid, sizeof(uuid_t));
	memcpy(ilm_ls->host_id, &cmd->cl->pid, sizeof(int));

	INIT_LIST_HEAD(&ilm_ls->lock_list);
	pthread_mutex_init(&ilm_ls->mutex, NULL);

	pthread_mutex_lock(&ls_mutex);
	list_add(&ilm_ls->list, &ls_list);
	pthread_mutex_unlock(&ls_mutex);

	ret = pthread_create(&ilm_ls->thd, NULL, ilm_lockspace_thread, ilm_ls);
	if (ret < 0) {
		ilm_log_err("%s: create thread failed", __func__);
		goto fail;
	}

	*ls_out = ilm_ls;
	ilm_send_result(cmd->cl->fd, 0, NULL, 0);
	return 0;

fail:
	pthread_mutex_lock(&ls_mutex);
	list_del(&ilm_ls->list);
	pthread_mutex_unlock(&ls_mutex);

	free(ilm_ls);
	return -1;
}

int ilm_lockspace_delete(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls)
{
	int ret;

	if (!_ls_is_valid(ilm_ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ilm_ls->mutex);
	ilm_ls->exit = 1;
	pthread_mutex_unlock(&ilm_ls->mutex);

	ret = pthread_join(ilm_ls->thd, NULL);

	pthread_mutex_lock(&ls_mutex);
	list_del(&ilm_ls->list);
	pthread_mutex_unlock(&ls_mutex);

	ilm_send_result(cmd->cl->fd, 0, NULL, 0);
	return ret;
}

int ilm_lockspace_add_lock(struct ilm_lockspace *ls,
			   struct ilm_lock *lock)
{
	int ret;

	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ls->mutex);
	list_add(&lock->list, &ls->lock_list);
	pthread_mutex_unlock(&ls->mutex);

	return 0;
}

int ilm_lockspace_del_lock(struct ilm_lockspace *ls, struct ilm_lock *lock)
{
	int ret;

	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ls->mutex);
	list_del(&lock->list);
	pthread_mutex_unlock(&ls->mutex);

	return 0;
}

int ilm_lockspace_find_lock(struct ilm_lockspace *ls, char *lock_id,
			    struct ilm_lock **lock)
{
	struct ilm_lock *pos;
	int ret = -1;

	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ls->mutex);

	list_for_each_entry(pos, &ls->lock_list, list) {

		if (!memcmp(pos->id, lock_id, IDM_LOCK_ID_LEN)) {
			if (lock)
				*lock = pos;
			ret = 0;
			break;
		}
	}

	pthread_mutex_unlock(&ls->mutex);
	return ret;
}

int ilm_lockspace_set_host_id(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls)
{
	struct ilm_lockspace *pos;
	char host_id[IDM_HOST_ID_LEN];
	int ret;

	if (!_ls_is_valid(ilm_ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = recv(cmd->cl->fd, host_id, IDM_HOST_ID_LEN, MSG_WAITALL);
	if (ret <= 0) {
		ilm_log_err("Failed to read out host ID\n");
		goto out;
	}

	pthread_mutex_lock(&ls_mutex);
	list_for_each_entry(pos, &ls_list, list) {
		if (pos == ilm_ls)
			continue;
		if (!memcmp(pos->host_id, host_id, IDM_HOST_ID_LEN)) {
			pthread_mutex_unlock(&ls_mutex);
			ret = -EBUSY;
			goto out;
		}
	}
	pthread_mutex_unlock(&ls_mutex);

	memcpy(ilm_ls->host_id, host_id, IDM_HOST_ID_LEN);

	ilm_log_array_dbg("Host ID:", host_id, IDM_HOST_ID_LEN);
	ilm_send_result(cmd->cl->fd, 0, NULL, 0);
	return 0;

out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lockspace_stop_renew(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls)
{
	int ret = 0;

	if (!_ls_is_valid(ilm_ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	pthread_mutex_lock(&ilm_ls->mutex);
	ilm_ls->stop_renew = 1;
	pthread_mutex_unlock(&ilm_ls->mutex);

out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lockspace_start_renew(struct ilm_cmd *cmd, struct ilm_lockspace *ilm_ls)
{
	int ret = 0;

	if (!_ls_is_valid(ilm_ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	pthread_mutex_lock(&ilm_ls->mutex);
	ilm_ls->stop_renew = 0;
	pthread_mutex_unlock(&ilm_ls->mutex);

out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}
