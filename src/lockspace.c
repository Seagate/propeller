/*
 * Copyright (C) 2020-2021 Seagate
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
#include "failure.h"
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

		if (exit) {
			ilm_log_dbg("%s: lockspace is exiting ...", __func__);
			break;
		}

		if (ls->failed == 1) {
			if (!list_empty(&ls->lock_list))
				ilm_log_warn("%s: renewal failure has been detected ...",
					     __func__);
				ilm_log_warn("%s: but lock still is not released",
					     __func__);
			ls->failed++;
		}

		if (ls->failed)
			continue;

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
			 */
			if (!lock->last_renewal_success)
				continue;

			/*
			 * If an IDM has been failed to renew for more than
			 * IDM_QUIESCENT_PERIOD, the lock manager will stop
			 * to try to renew it anymore.
			 */
			if (now > lock->last_renewal_success +
					IDM_QUIESCENT_PERIOD) {
				ilm_failure_handler(ls);
				ls->failed = 1;
				ilm_log_dbg("%s: has sent kill path or signal",
					     __func__);
				continue;
			}

			pthread_mutex_lock(&lock->mutex);
			ret = idm_raid_renew_lock(lock, ls->host_id);
			pthread_mutex_unlock(&lock->mutex);
			if (!ret)
				lock->last_renewal_success = ilm_curr_time();
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
	ilm_ls->kill_pid = cmd->cl->pid;

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

	ret = idm_raid_thread_create(&ilm_ls->raid_thd);
	if (ret < 0) {
		ilm_log_err("%s: create raid thread failed", __func__);
		goto fail_raid_thd;
	}

	*ls_out = ilm_ls;
	ilm_send_result(cmd->cl->fd, 0, NULL, 0);
	return 0;

fail_raid_thd:
	pthread_mutex_lock(&ilm_ls->mutex);
	ilm_ls->exit = 1;
	pthread_mutex_unlock(&ilm_ls->mutex);
	pthread_join(ilm_ls->thd, NULL);
fail:
	pthread_mutex_lock(&ls_mutex);
	list_del(&ilm_ls->list);
	pthread_mutex_unlock(&ls_mutex);

	free(ilm_ls);
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
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

	idm_raid_thread_free(ilm_ls->raid_thd);

	if (ilm_ls->kill_path)
		free(ilm_ls->kill_path);
	if (ilm_ls->kill_args)
		free(ilm_ls->kill_args);
	free(ilm_ls);

	ilm_send_result(cmd->cl->fd, 0, NULL, 0);
	return ret;
}

int ilm_lockspace_add_lock(struct ilm_lockspace *ls,
			   struct ilm_lock *lock)
{
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
	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ls->mutex);
	list_del(&lock->list);
	pthread_mutex_unlock(&ls->mutex);

	return 0;
}

int ilm_lockspace_start_lock(struct ilm_lockspace *ls,
			     struct ilm_lock *lock,
			     uint64_t time)
{
	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ls->mutex);
	lock->last_renewal_success = time;
	pthread_mutex_unlock(&ls->mutex);

	return 0;
}

int ilm_lockspace_stop_lock(struct ilm_lockspace *ls,
			    struct ilm_lock *lock,
			    uint64_t *time)
{
	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ls->mutex);
	if (time)
		*time = lock->last_renewal_success;
	lock->last_renewal_success = 0;
	pthread_mutex_unlock(&ls->mutex);

	return 0;
}

int ilm_lockspace_set_signal(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	int signo, ret;

	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = recv(cmd->cl->fd, &signo, sizeof(int), MSG_WAITALL);
	if (ret <= 0) {
		ilm_log_err("Failed to read out singal number\n");
		goto out;
	}

	ls->kill_sig = signo;

out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lockspace_set_killpath(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	char path[IDM_FAILURE_PATH_LEN];
	char args[IDM_FAILURE_ARGS_LEN];
	int ret, pos = 0;

	if (!_ls_is_valid(ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = recv(cmd->cl->fd, path, IDM_FAILURE_PATH_LEN, MSG_WAITALL);
	if (!ret || (ret != IDM_FAILURE_PATH_LEN)) {
		ilm_log_err("Fail to receive kill path %d", errno);
		ret = -EIO;
		goto out;
	}
	pos += ret;

	ls->kill_path = strndup(path, IDM_FAILURE_PATH_LEN);
	if (!ls->kill_path) {
		ilm_log_err("Fail to allocate memory for kill path");
		ret = -ENOMEM;
		goto out;
	}

	ret = recv(cmd->cl->fd, args, IDM_FAILURE_ARGS_LEN, MSG_WAITALL);
	if (!ret || (ret != IDM_FAILURE_ARGS_LEN)) {
		ilm_log_err("Fail to receive kill args %d", errno);
		ret = -EIO;
		goto out;
	}
	pos += ret;

	ls->kill_args = strndup(args, IDM_FAILURE_ARGS_LEN);
	if (!ls->kill_args) {
		ilm_log_err("Fail to allocate memory for kill args");
		ret = -ENOMEM;
	}

out:
	ilm_client_recv_all(cmd->cl, cmd->sock_msg_len, pos);
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
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

int ilm_lockspace_terminate(struct ilm_lockspace *ls)
{
	struct ilm_lock *lock, *next;

	/*
	 * If client has released locks and deleted lockspace, bail out
	 * and do nothing.  Otherwise, if the client exits abnormally
	 * and losts connection with daemon, this function is used to
	 * release locks and its lockspace.
	 */
	if (!_ls_is_valid(ls))
		return 0;

	pthread_mutex_lock(&ls->mutex);

	list_for_each_entry_safe(lock, next, &ls->lock_list, list) {
		list_del(&lock->list);
		ilm_lock_terminate(ls, lock);
	}

	ls->exit = 1;

	pthread_mutex_unlock(&ls->mutex);

	pthread_join(ls->thd, NULL);

	pthread_mutex_lock(&ls_mutex);
	list_del(&ls->list);
	pthread_mutex_unlock(&ls_mutex);

	idm_raid_thread_free(ls->raid_thd);

	if (ls->kill_path)
		free(ls->kill_path);
	if (ls->kill_args)
		free(ls->kill_args);
	free(ls);
	return 0;
}
