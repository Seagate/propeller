/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

#include "ilm.h"

#include "idm_wrapper.h"
#include "inject_fault.h"
#include "lock.h"
#include "log.h"
#include "raid_lock.h"
#include "string.h"
#include "util.h"


#define EALL				0xDEADBEAF

/* Timeout 5s (5000ms) */
#define ILM_MAJORITY_TIMEOUT		5000

/* Poll timeout 1s (1000ms) */
#define RAID_LOCK_POLL_INTERVAL		1000

enum {
	ILM_OP_LOCK = 0,
	ILM_OP_UNLOCK,
	ILM_OP_CONVERT,
	ILM_OP_BREAK,
	ILM_OP_RENEW,
	ILM_OP_WRITE_LVB,
	ILM_OP_READ_LVB,
	ILM_OP_COUNT,
	ILM_OP_MODE,
};

enum {
	IDM_INIT = 0,	/* Also is for unlock state */
	IDM_BUSY,
	IDM_DUPLICATE,
	IDM_LOCK,
	IDM_TIMEOUT,
	IDM_FAULT,
};

struct _raid_state_transition {
	int curr;
	int result;
	int next;
};

struct _raid_request {
	struct list_head list;

	int renew;
	int op;

	struct ilm_lock *lock;
	char *host_id;
	struct ilm_drive *drive;

	char *lvb;
	int lvb_size;
	int count;
	int self;
	int mode;

	uint64_t handle;
	int result;
};

struct _raid_thread {
	pthread_t th;

	int init;

	int exit;
	pthread_cond_t exit_wait;

	struct list_head request_list;
	pthread_mutex_t request_mutex;
	pthread_cond_t request_cond;

	struct list_head process_list;

	int count;
	struct list_head response_list;
	pthread_mutex_t response_mutex;
	pthread_cond_t response_cond;

	int renew_count;
	struct list_head renew_list;
	pthread_mutex_t renew_mutex;
	pthread_cond_t renew_cond;
};

/*
 * Every IDM drive's state machine is maintained as below:
 *
 *                          ---------+----------------+
 *                          |        | Other failres  |
 *    Fail to break         V        | when lock      |
 *           +------->  IDM_INIT  ---+                |
 *           |             |   ^                      |
 *           |             |   | Release previous     |
 *           |   -EBUSY    |   | acquiration          |
 *           |  +----------+---+-------+              |
 *           |  |  -EAGAIN |   |       |              |
 *           |  V          V   |       |              |
 *          IDM_BUSY  IDM_DUPLICATE    |              |
 *              |                      |              |
 *              |                      |              |
 *              +----------+-----------+              |
 *                         |                          |
 *   Break successfully    |   Acquire successfully   |
 *                         |                          |
 *                 +---+   |                          |
 *                 |   V   V                          |  Release lock
 *          Renew  |   IDM_LOCK  ---------------------+
 *    successfully |   |   |                          |
 *                 |   |   |                          |
 *                 +---+   |  Fail to renew           |
 *                         V                          |
 *                    IDM_TIMEOUT  -------------------+
 */
struct _raid_state_transition state_transition[] = {
	/*
	 * The idm has been acquired successfully.
	 */
	{
		.curr	= IDM_INIT,
		.result = 0,
		.next	= IDM_LOCK,
	},

	/*
	 * The idm has been acquired by other hosts, but not sure if these
	 * hosts have been timeout or not or if set an infinite timeout
	 * value.  If all hosts have been timeout, we can break the idm's
	 * current granting and has chance to take this idm.
	 */
	{
		.curr	= IDM_INIT,
		.result = -EBUSY,
		.next	= IDM_BUSY,
	},

	/*
	 * The idm has been acquired successfully before, but for some reaons
	 * (e.g. the host lost connection with drive and reconnect again) the
	 * host tries to acquire it again and the host's membership isn't
	 * expired.  So it reports the error -EAGAIN.
	 */
	{
		.curr	= IDM_INIT,
		.result	= -EAGAIN,
		.next	= IDM_DUPLICATE,
	},

	/*
	 * Otherwise, if fail to acquire idm, stay IDM_INIT state.
	 */
	{
		.curr	= IDM_INIT,
		.result	= -EALL,
		.next	= IDM_INIT,
	},

	/*
	 * Break the busy lock and gain it.
	 */
	{
		.curr	= IDM_BUSY,
		.result	= 0,
		.next	= IDM_LOCK,
	},

	/*
	 * Fail to break the busy lock, for any reason, transit to IDM_INIT
	 * state, so allow to acquire lock in next round.
	 */
	{
		.curr	= IDM_BUSY,
		.result	= -EALL,
		.next	= IDM_INIT,
	},

	/*
	 * For duplicated locking state, we must call unlock operation, thus
	 * ignore any return value and always move to IDM_INIT state.
	 */
	{
		.curr	= IDM_DUPLICATE,
		.result	= -EALL,
		.next	= IDM_INIT,
	},

	/*
	 * After have been granted idm, make success for an operation (e.g.
	 * convert, renew, read lvb, etc) so keep the state.
	 */
	{
		.curr	= IDM_LOCK,
		.result	= 0,
		.next	= IDM_LOCK,
	},

	/*
	 * Even the drive is failure and return -EIO, still keep IDM_LOCK state,
	 * this allows to retry in next round in outer loop.
	 */
	{
		.curr	= IDM_LOCK,
		.result	= -EIO,
		.next	= IDM_LOCK,
	},

	/*
	 * The membership is timeout, transit to IDM_TIMEOUT state.
	 */
	{
		.curr	= IDM_LOCK,
		.result	= -ETIME,
		.next	= IDM_TIMEOUT,
	},

	/*
	 * The membership is fault (e.g. the lock mode is not consistent),
	 * transit to IDM_FAULT state.
	 */
	{
		.curr	= IDM_LOCK,
		.result	= -EFAULT,
		.next	= IDM_FAULT,
	},

	/*
	 * This is a special case which is used for the normal unlock flow.
	 */
	{
		.curr	= IDM_LOCK,
		.result	= 1,
		.next	= IDM_INIT,
	},

	/*
	 * Otherwise, stay in IDM_LOCK state.
	 */
	{
		.curr	= IDM_LOCK,
		.result	= -EALL,
		.next	= IDM_LOCK,
	},

	/*
	 * This is used for unlocking an IDM after timeout.
	 */
	{
		.curr	= IDM_TIMEOUT,
		.result	= -EALL,
		.next	= IDM_INIT,
	},

	/*
	 * This is used for unlocking an IDM after fault.
	 */
	{
		.curr	= IDM_FAULT,
		.result	= -EALL,
		.next	= IDM_INIT,
	},
};

#if 0
static int _raid_dispatch_request(struct _raid_request *req)
{
	struct ilm_lock *lock = req->lock;
	struct ilm_drive *drive = req->drive;
	int ret;

	/* Update inject fault */
	ilm_inject_fault_update(lock->drive_num, drive->index);

	switch (req->op) {
	case ILM_OP_LOCK:
		drive->is_brk = 0;
		ret = idm_drive_lock(lock->id, req->mode, req->host_id,
				     drive->path, lock->timeout);
		break;
	case ILM_OP_UNLOCK:
		ret = idm_drive_unlock(lock->id, req->host_id, req->lvb,
				       req->lvb_size, drive->path);
		break;
	case ILM_OP_CONVERT:
		ret = idm_drive_convert_lock(lock->id, req->mode, req->host_id,
					     drive->path);
		break;
	case ILM_OP_BREAK:
		ret = idm_drive_break_lock(lock->id, req->mode, req->host_id,
					   drive->path, lock->timeout);
		if (!ret)
			drive->is_brk = 1;
		break;
	case ILM_OP_RENEW:
		ret = idm_drive_renew_lock(lock->id, req->mode, req->host_id,
					   drive->path);
		break;
	case ILM_OP_READ_LVB:
		ret = idm_drive_read_lvb(lock->id, req->host_id, req->lvb,
					 req->lvb_size, drive->path);
		break;
	case ILM_OP_COUNT:
		ret = idm_drive_lock_count(lock->id, &req->count,
					   &req->self, drive->path);
		break;
	case ILM_OP_MODE:
		ret = idm_drive_lock_mode(lock->id, &req->mode, drive->path);
		break;
	default:
		assert(1);
		break;
	}

	req->result = ret;

	ilm_log_dbg("%s: op=%d result=%d", __func__, req->op, req->result);
	return ret;
}
#endif

static int _raid_dispatch_request_async(struct _raid_request *req)
{
	struct ilm_lock *lock = req->lock;
	struct ilm_drive *drive = req->drive;
	uint64_t handle;
	int ret;

	/* Update inject fault */
	ilm_inject_fault_update(lock->drive_num, drive->index);

	switch (req->op) {
	case ILM_OP_LOCK:
		drive->is_brk = 0;
		ret = idm_drive_lock_async(lock->id, req->mode, req->host_id,
					   drive->path, lock->timeout, &handle);
		break;
	case ILM_OP_UNLOCK:
		ret = idm_drive_unlock_async(lock->id, req->mode, req->host_id,
					     req->lvb, req->lvb_size,
					     drive->path, &handle);
		break;
	case ILM_OP_CONVERT:
		ret = idm_drive_convert_lock_async(lock->id, req->mode,
						   req->host_id, drive->path,
						   &handle);
		break;
	case ILM_OP_BREAK:
		ret = idm_drive_break_lock_async(lock->id, req->mode,
						 req->host_id, drive->path,
						 lock->timeout, &handle);
		break;
	case ILM_OP_RENEW:
		ret = idm_drive_renew_lock_async(lock->id, req->mode,
						 req->host_id, drive->path,
						 &handle);
		break;
	case ILM_OP_READ_LVB:
		ret = idm_drive_read_lvb_async(lock->id, req->host_id,
					       drive->path, &handle);
		break;
	case ILM_OP_COUNT:
		ret = idm_drive_lock_count_async(lock->id, req->host_id,
						 drive->path, &handle);
		break;
	case ILM_OP_MODE:
		ret = idm_drive_lock_mode_async(lock->id, drive->path, &handle);
		break;
	default:
		ret = -EINVAL;
		assert(1);
		break;
	}

	req->handle = handle;
	req->result = ret;

	ilm_log_dbg("%s: req=%p op=%d result=%d handle=%lx", __func__,
		    req, req->op, req->result, req->handle);
	return ret;
}

static int _raid_read_result_async(struct _raid_request *req)
{
	struct ilm_drive *drive = req->drive;
	int ret;

	switch (req->op) {
	case ILM_OP_LOCK:
	case ILM_OP_UNLOCK:
	case ILM_OP_CONVERT:
	case ILM_OP_RENEW:
		ret = idm_drive_async_result(req->handle, &req->result);
		break;
	case ILM_OP_BREAK:
		ret = idm_drive_async_result(req->handle, &req->result);
		if (!ret)
			drive->is_brk = 1;
		break;
	case ILM_OP_READ_LVB:
		ret = idm_drive_read_lvb_async_result(req->handle, req->lvb,
						      req->lvb_size,
						      &req->result);
		break;
	case ILM_OP_COUNT:
		ret = idm_drive_lock_count_async_result(req->handle,
							&req->count,
							&req->self,
						        &req->result);
		break;
	case ILM_OP_MODE:
		ret = idm_drive_lock_mode_async_result(req->handle,
						       &req->mode,
						       &req->result);
		break;
	default:
		assert(1);
		break;
	}

	ilm_log_dbg("%s: ret=%d", __func__, ret);
	assert(ret == 0);
	return ret;
}

static int _raid_state_machine_end(int state)
{
	ilm_log_dbg("%s: state=%d", __func__, state);
	if (state == IDM_LOCK || state == IDM_INIT)
		return 1;

	/* Something must be wrong! */
	if (state == -1)
		return -1;

	return 0;
}

static int _raid_state_find_op(int state, int func)
{
	int op;

	switch (state) {
	case IDM_INIT:
		assert(func == ILM_OP_LOCK || func == ILM_OP_COUNT ||
		       func == ILM_OP_MODE || func == ILM_OP_RENEW ||
		       func == ILM_OP_CONVERT);

		/* Enlarge majority when renew or convert */
		if (func == ILM_OP_RENEW || func == ILM_OP_CONVERT)
			op = ILM_OP_LOCK;
		else
			op = func;
		break;
	case IDM_BUSY:
		op = ILM_OP_BREAK;
		break;
	case IDM_DUPLICATE:
		op = ILM_OP_UNLOCK;
		break;
	case IDM_LOCK:
		assert(func == ILM_OP_UNLOCK || func == ILM_OP_CONVERT ||
		       func == ILM_OP_RENEW  || func == ILM_OP_READ_LVB ||
		       func == ILM_OP_COUNT  || func == ILM_OP_MODE);
		op = func;
		break;
	case IDM_TIMEOUT:
		op = ILM_OP_UNLOCK;
		break;
	case IDM_FAULT:
		op = ILM_OP_UNLOCK;
		break;
	default:
		ilm_log_err("%s: unsupported state %d", __func__, state);
		op = -1;
		break;
	}

	ilm_log_dbg("%s: state=%d op=%d->%d", __func__, state, func, op);
	return op;
}

static int _raid_state_lockup(int state, int result)
{
	int transition_num = sizeof(state_transition) /
		sizeof(struct _raid_state_transition);
	struct _raid_state_transition *trans;
	int i;

	for (i = 0; i < transition_num; i++) {
		trans = &state_transition[i];
		if ((trans->curr == state) && (trans->result == result))
			return trans->next;

		if ((trans->curr == state) && (trans->result == -EALL))
			return trans->next;
	}

	ilm_log_err("%s: state machine malfunction state=%d result=%d\n",
		    __func__, state, result);
	return -1;
}

static int idm_raid_state_transition(struct _raid_request *req)
{
	struct ilm_drive *drive = req->drive;
	int state = drive->state, next_state;
	int result = req->result;

	/*
	 * Three cases will force to set result to 1, so that can ensure
	 * the state machine transition to work as expected.
	 *
	 * Case 1: when a mutex is in IDM_INIT state and inquire IDM's
	 * count, always keep in the IDM_INIT state;
	 *
	 * Case 2: when a mutex is in IDM_INIT state and inquire IDM's
	 * mode, always keep in the IDM_INIT state;
	 *
	 * Case 3: when unlock a mutex, don't handle any failure in
	 * this case and always transit to IDM_UNLOCK state.
	 */
	if (state == IDM_INIT && req->op == ILM_OP_COUNT)
		result = 1;
	else if (state == IDM_INIT && req->op == ILM_OP_MODE)
		result = 1;
	else if (state == IDM_LOCK && req->op == ILM_OP_UNLOCK)
		result = 1;

	next_state = _raid_state_lockup(state, result);

	ilm_log_dbg("%s: drive=%s", __func__, drive->path);
	ilm_log_dbg("%s: op=%d return=%d state=%d->%d",
		    __func__, req->op, result, state, next_state);

	drive->state = next_state;
	return 0;
}

static int idm_raid_add_request(struct _raid_thread *raid_th,
				struct _raid_request *req)
{
	struct ilm_drive *drive = req->drive;

	req->op = _raid_state_find_op(drive->state, req->op);

	pthread_mutex_lock(&raid_th->request_mutex);

	if (raid_th->exit) {
		pthread_mutex_unlock(&raid_th->request_mutex);
		return -1;
	}

	list_add_tail(&req->list, &raid_th->request_list);

	ilm_log_dbg("%s: request [drive=%s]", __func__, drive->path);

	pthread_mutex_unlock(&raid_th->request_mutex);

	if (req->renew) {
		raid_th->renew_count++;
		ilm_log_dbg("%s: renew_count=%d",
			    __func__, raid_th->renew_count);
	} else {
		raid_th->count++;
		ilm_log_dbg("%s: count=%d", __func__, raid_th->count);
	}

	return 0;
}

static void idm_raid_signal_request(struct _raid_thread *raid_th)
{
	pthread_mutex_lock(&raid_th->request_mutex);
	pthread_cond_signal(&raid_th->request_cond);
	pthread_mutex_unlock(&raid_th->request_mutex);
}

static struct _raid_request *
idm_raid_wait_response(struct _raid_thread *raid_th)
{
	struct _raid_request *req;

	if (!raid_th->count)
		return NULL;

	pthread_mutex_lock(&raid_th->response_mutex);

	if (list_empty(&raid_th->response_list))
		pthread_cond_wait(&raid_th->response_cond,
				  &raid_th->response_mutex);

	req = list_first_entry(&raid_th->response_list,
				struct _raid_request, list);
	list_del(&req->list);

	ilm_log_dbg("%s: response [drive=%s]", __func__, req->drive->path);
	pthread_mutex_unlock(&raid_th->response_mutex);

	raid_th->count--;
	return req;
}

static struct _raid_request *
idm_raid_wait_renew(struct _raid_thread *raid_th)
{
	struct _raid_request *req;

	if (!raid_th->renew_count)
		return NULL;

	pthread_mutex_lock(&raid_th->renew_mutex);

	if (list_empty(&raid_th->renew_list))
		pthread_cond_wait(&raid_th->renew_cond,
				  &raid_th->renew_mutex);

	req = list_first_entry(&raid_th->renew_list,
				struct _raid_request, list);
	list_del(&req->list);

	ilm_log_dbg("%s: renew response [drive=%s]", __func__, req->drive->path);
	pthread_mutex_unlock(&raid_th->renew_mutex);

	raid_th->renew_count--;
	return req;
}

static struct _raid_request *
idm_raid_wait(struct _raid_thread *raid_th, int renew)
{
	if (!renew)
		return idm_raid_wait_response(raid_th);
	else
		return idm_raid_wait_renew(raid_th);
}

static void idm_raid_notify(struct _raid_thread *raid_th,
			    struct _raid_request *req)
{
	if (req->renew) {
		pthread_mutex_lock(&raid_th->renew_mutex);
		list_add_tail(&req->list, &raid_th->renew_list);

		ilm_log_dbg("%s: add to renew list [drive=%s]",
			    __func__, req->drive->path);

		pthread_cond_signal(&raid_th->renew_cond);
		pthread_mutex_unlock(&raid_th->renew_mutex);
	} else {
		pthread_mutex_lock(&raid_th->response_mutex);
		list_add_tail(&req->list, &raid_th->response_list);

		ilm_log_dbg("%s: add to response list [drive=%s]",
			    __func__, req->drive->path);

		pthread_cond_signal(&raid_th->response_cond);
		pthread_mutex_unlock(&raid_th->response_mutex);
	}

	return;
}

static void *idm_raid_thread(void *data)
{
	struct _raid_thread *raid_th = data;
	struct _raid_request *req, *tmp;
	int process_num, num;
	struct pollfd *poll_fd;
#ifndef IDM_PTHREAD_EMULATION
	int ret;
#endif
	int i;

	raid_th->init = 1;

	pthread_mutex_lock(&raid_th->request_mutex);

	while (1) {
		while (!raid_th->exit && list_empty(&raid_th->request_list))
			pthread_cond_wait(&raid_th->request_cond,
					  &raid_th->request_mutex);

		process_num = 0;
		while (!list_empty(&raid_th->request_list)) {
			req = list_first_entry(&raid_th->request_list,
					       struct _raid_request, list);
			list_del(&req->list);
			list_add_tail(&req->list, &raid_th->process_list);
			process_num++;
			ilm_log_dbg("%s: move to process list [drive=%s]",
				    __func__, req->drive->path);
		}

		pthread_mutex_unlock(&raid_th->request_mutex);

		list_for_each_entry(req, &raid_th->process_list, list) {
			_raid_dispatch_request_async(req);
			//_raid_dispatch_request(req);
		}

		poll_fd = malloc(sizeof(struct pollfd) * process_num);
		if (!poll_fd) {
			ilm_log_err("%s: cannot allcoate pollfd", __func__);
			return NULL;
		}

		while (!list_empty(&raid_th->process_list)) {

			memset(poll_fd, 0x0, sizeof(struct pollfd) * process_num);

			/* Prepare for polling file descriptor array */
			num = 0;
			list_for_each_entry(tmp, &raid_th->process_list, list) {
				poll_fd[num].fd = idm_drive_get_fd(tmp->handle);
				poll_fd[num].events = POLLIN;
				num++;
			}

#ifndef IDM_PTHREAD_EMULATION
			/* Wait for drive's response */
			ret = poll(poll_fd, num, RAID_LOCK_POLL_INTERVAL);
			if (ret == -1 && errno == EINTR)
				continue;
#else
			/*
			 * Emulate asnyc operation, simply set response for
			 * all FDs
			*/
			for (i = 0; i < num; i++)
				poll_fd[i].revents = POLLIN;
#endif

			/* Handle for all response */
			for (i = 0; i < num; i++) {
				if (!poll_fd[i].revents & POLLIN)
					continue;

				req = NULL;
				list_for_each_entry(tmp, &raid_th->process_list, list) {
					if (idm_drive_get_fd(tmp->handle) ==
							poll_fd[i].fd) {
						req = tmp;
						break;
					}
				}

				/* Should never happen? */
				if (!req) {
					ilm_log_err("%s: fail to find request for polling fd %d",
						    __func__, poll_fd[i].fd);
					continue;
				}

				_raid_read_result_async(req);

				list_del(&req->list);
				idm_raid_notify(raid_th, req);
			}
		}

		free(poll_fd);

		pthread_mutex_lock(&raid_th->request_mutex);

		if (raid_th->exit)
			break;
	}

	pthread_cond_signal(&raid_th->exit_wait);
	pthread_mutex_unlock(&raid_th->request_mutex);
	return NULL;
}

void idm_raid_thread_free(struct _raid_thread *raid_th)
{
	assert(raid_th);

	pthread_mutex_lock(&raid_th->request_mutex);
	raid_th->exit = 1;
	pthread_cond_broadcast(&raid_th->request_cond);
	pthread_cond_wait(&raid_th->exit_wait, &raid_th->request_mutex);
	pthread_mutex_unlock(&raid_th->request_mutex);
}

int idm_raid_thread_create(struct _raid_thread **rth)
{
	struct _raid_thread *raid_th;
	int ret;

	raid_th = malloc(sizeof(struct _raid_thread));
	if (!raid_th)
		return -ENOMEM;

	memset(raid_th, 0, sizeof(struct _raid_thread));

	pthread_mutex_init(&raid_th->request_mutex, NULL);
	pthread_cond_init(&raid_th->request_cond, NULL);
	INIT_LIST_HEAD(&raid_th->request_list);

	INIT_LIST_HEAD(&raid_th->response_list);
	pthread_mutex_init(&raid_th->response_mutex, NULL);
	pthread_cond_init(&raid_th->response_cond, NULL);

	INIT_LIST_HEAD(&raid_th->renew_list);
	pthread_mutex_init(&raid_th->renew_mutex, NULL);
	pthread_cond_init(&raid_th->renew_cond, NULL);

	INIT_LIST_HEAD(&raid_th->process_list);

	pthread_cond_init(&raid_th->exit_wait, NULL);

	ret = pthread_create(&raid_th->th, NULL, idm_raid_thread, raid_th);
	if (ret < 0) {
		ilm_log_err("Fail to create raid thread\n");
		free(raid_th);
		return ret;
	}

	/*
	 * Wait for raid thread's launching, otherwise it has small
	 * chance to send lock operations before the raid thread has
	 * been ready.  Thus the raid thread cannot receive signal
	 * and cause the stall issue.
	 */
	while (!raid_th->init)
		usleep(10);

	*rth = raid_th;
	return 0;
}

static void idm_raid_destroy(struct ilm_drive *drive)
{
	struct idm_info *info_list, *info_i, *info_j, *least_renew = NULL;
	int info_num;
	int ret, i, j;
	uint64_t least_renew_time = -1ULL;

	ret = idm_drive_read_group(drive->path, &info_list, &info_num);
	if (ret)
		return;

	/*
	 * This is to implement the Least Recently Used (LRU) algorithm by
	 * traversing all items
	 */
	for (i = 0; i < info_num; i++) {
		info_i = info_list + i;

		for (j = 0; j < info_num; j++) {
			if (i == j)
				continue;

			info_j = info_list + j;


			if (memcmp(info_i->id, info_j->id, IDM_LOCK_ID_LEN))
				continue;

			/*
			 * If two items have the same lock ID, consolidate their
			 * 'timeout' flags; if any item is not timeout, the
			 * related IDM cannot be destroied.  Simply set all
			 * items as not timeout as we don't consider it as a
			 * destroying candidate.
			 */
			if (!info_i->timeout || !info_j->timeout) {
				info_i->timeout = 0;
				info_j->timeout = 0;
				continue;
			}

			/*
			 * Make consistent for the last renew time for all
			 * items associated with the same IDM.
			 */
			if (info_i->last_renew_time > info_j->last_renew_time)
				info_j->last_renew_time = info_i->last_renew_time;
			else
				info_i->last_renew_time = info_j->last_renew_time;
		}
	}

	/* Find out the least renewal item */
	for (i = 0; i < info_num; i++) {
		info_i = info_list + i;

		if (!info_i->timeout)
			continue;

		if (least_renew_time > info_i->last_renew_time) {
			least_renew = info_i;
			least_renew_time = info_i->last_renew_time;
		}
	}

	if (least_renew) {
		ilm_log_dbg("%s: least_renew mode=%d last_renew_time=%lu timeout=%d",
			    __func__, least_renew->mode,
			    least_renew->last_renew_time, least_renew->timeout);
		ilm_log_array_dbg("Lock ID:", least_renew->id, IDM_LOCK_ID_LEN);
		ilm_log_array_dbg("Host ID:", least_renew->host_id, IDM_HOST_ID_LEN);
		idm_drive_destroy(least_renew->id, drive->path);
	}
}

static void idm_raid_multi_issue(struct ilm_lock *lock, char *host_id,
				 int op, int mode, int renew)
{
	struct ilm_drive *drive;
	struct _raid_request *req;
	int i;

	ilm_log_dbg("%s: op=%d mode=%d", __func__, op, mode);

	for (i = 0; i < lock->drive_num; i++) {

		drive = &lock->drive[i];

		if (drive->state == IDM_INIT && op == ILM_OP_UNLOCK) {
			drive->result = 0;
			continue;
		}

		req = malloc(sizeof(struct _raid_request));
		memset(req, 0, sizeof(struct _raid_request));

		req->op = op;
		req->lock = lock;
		req->host_id = host_id;
		req->drive = drive;
		req->mode = (mode != -1) ? mode : lock->mode;
		req->renew = renew;

		/*
		 * When unlock an IDM, it's the time to write LVB into drive,
		 * so copy the cached LVB in lock data structure into
		 * drive->vb, thus this will be passed to drive.
		 */
		if (op == ILM_OP_UNLOCK)
			memcpy(drive->vb, lock->vb, IDM_VALUE_LEN);

		req->lvb = drive->vb;
		req->lvb_size = IDM_VALUE_LEN;

		idm_raid_add_request(lock->raid_th, req);
	}

	idm_raid_signal_request(lock->raid_th);

	while ((req = idm_raid_wait(lock->raid_th, renew))) {
		idm_raid_state_transition(req);

		drive = req->drive;

		if (drive->state == IDM_INIT && req->result == -ENOMEM)
			idm_raid_destroy(drive);

		if (_raid_state_machine_end(drive->state)) {
			drive->result = req->result;
			drive->mode = req->mode;
			drive->count = req->count;
			drive->self = req->self;
			ilm_log_dbg("%s: drive result=%d mode=%d count=%d", __func__,
				    drive->result, drive->mode, drive->count);
			free(req);
			continue;
		}

		idm_raid_add_request(lock->raid_th, req);
		idm_raid_signal_request(lock->raid_th);
	}

	return;
}

static void ilm_raid_lock_dump(const char *str, struct ilm_lock *lock)
{
	int i;

	ilm_log_dbg("RAID lock dump: %s", str);
	for (i = 0; i < lock->drive_num; i++)
		ilm_log_dbg("drive[%d]: path=%s state=%d",
			    i, lock->drive[i].path, lock->drive[i].state);
}

int idm_raid_lock(struct ilm_lock *lock, char *host_id)
{
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	struct ilm_drive *drive;
	int rand_sleep;
	int io_err;
	int score, i;

	/* Initialize all drives state to NO_ACCESS */
	for (i = 0; i < lock->drive_num; i++)
		lock->drive[i].state = IDM_INIT;

	ilm_raid_lock_dump("Enter raid_lock", lock);

	do {
		idm_raid_multi_issue(lock, host_id, ILM_OP_LOCK, lock->mode, 0);

		score = 0;
		io_err = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];

			if (!drive->result && drive->state == IDM_LOCK)
				score++;

			if (drive->result == -EIO)
				io_err++;
		}

		/* Acquired majoirty */
		if (score >= ((lock->drive_num >> 1) + 1)) {
			ilm_raid_lock_dump("Exit raid_lock", lock);
			return 0;
		}

		/*
                 * Fail to achieve majority, release IDMs has been
		 * acquired; race for next round.
		 */
		idm_raid_multi_issue(lock, host_id, ILM_OP_UNLOCK, lock->mode, 0);

		/*
		 * Sleep for random interval for sleep, the interval range
		 * is [1 .. 10] us; this can avoid the multiple hosts keeping
		 * the same pace thus every one cannot acquire majority (e.g.
		 * two hosts, every host only can acquire successfully half
		 * drives, and finally no host can achieve the majority).
		 */
		rand_sleep = ilm_rand(1, 10);
		usleep(rand_sleep);

	} while (ilm_curr_time() < timeout);

	ilm_log_dbg("%s: Timeout", __func__);

	/*
	 * If I/O error prevents to achieve the majority, this is different
	 * for the cases with even and odd drive number.  E.g. for odd drive
	 * number, the I/O error must occur at least for (drive_num >> 1 + 1)
	 * times; for even drive number, the I/O error must occur at least
	 * for (drive_num >> 1) time.
	 *
	 * We can use the formula (drive_num - (drive_num >> 1)) to calculate
	 * it.
	 */
	if (io_err >= (lock->drive_num - (lock->drive_num >> 1)))
		return -EIO;

	/* Timeout, fail to acquire lock with majoirty */
	return -1;
}

int idm_raid_unlock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	int io_err = 0, timeout = 0;
	int i;

	ilm_raid_lock_dump("Enter raid_unlock", lock);

	idm_raid_multi_issue(lock, host_id, ILM_OP_UNLOCK, lock->mode, 0);

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		/* Always make success to unlock */
		assert(drive->state == IDM_INIT);

		if (drive->result == -EIO)
			io_err++;

		if (drive->result == -ETIME)
			timeout++;
	}

	/* All drives have been timeout */
	if (timeout >= (lock->drive_num - (lock->drive_num >> 1)))
		return -ETIME;

	if (io_err >= (lock->drive_num - (lock->drive_num >> 1)))
		return -EIO;

	ilm_raid_lock_dump("Exit raid_unlock", lock);
	return 0;
}

int idm_raid_convert_lock(struct ilm_lock *lock, char *host_id, int mode)
{
	struct ilm_drive *drive;
	int io_err = 0;
	int i, score, timeout = 0;

	ilm_raid_lock_dump("Enter raid_convert_lock", lock);

	/*
	 * If fail to convert mode previously, afterwards cannot convert
	 * mode anymore.
	 */
	if (lock->convert_failed == 1)
		return -1;

	idm_raid_multi_issue(lock, host_id, ILM_OP_CONVERT, mode, 0);

	score = 0;
	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		if (!drive->result && drive->state == IDM_LOCK)
			score++;

		if (drive->result == -EIO)
			io_err++;

		if (drive->result == -ETIME)
			timeout++;
	}

	ilm_raid_lock_dump("Finish raid_convert_lock", lock);

	/* Has achieved majoirty */
	if (score >= ((lock->drive_num >> 1) + 1))
		return 0;

	/* Majority drives have been timeout */
	if (timeout >= (lock->drive_num - (lock->drive_num >> 1)))
		return -ETIME;

	/* Majority drives have I/O error */
	if (io_err >= (lock->drive_num - (lock->drive_num >> 1)))
		return -EIO;

	/*
	 * Always return success when the mode is demotion, if it fails to
	 * acheive majority with demotion, set the flag 'convert_failed'
	 * so disallow to convert mode anymore, this can avoid further mess.
	 */
	if (lock->mode == IDM_MODE_EXCLUSIVE && mode == IDM_MODE_SHAREABLE) {
		lock->convert_failed = 1;
		return 0;
	}

	idm_raid_multi_issue(lock, host_id, ILM_OP_CONVERT, lock->mode, 0);

	score = 0;
	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		if (!drive->result && drive->state == IDM_LOCK)
			score++;
	}

	/* Unfortunately, fail to revert to old mode. */
	if (score < ((lock->drive_num >> 1) + 1)) {
		ilm_log_warn("Fail to revert lock mode, old %d vs new %d\n",
			     lock->mode, mode);
		ilm_log_warn("Defer to resolve this when renew the lock\n");
		lock->convert_failed = 1;
	}

	ilm_raid_lock_dump("Exit raid_convert_lock", lock);
	return -1;
}

int idm_raid_renew_lock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i;

	ilm_raid_lock_dump("Enter raid_renew_lock", lock);

	do {
		idm_raid_multi_issue(lock, host_id, ILM_OP_RENEW, lock->mode, 1);

		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];

			if (!drive->result && drive->state == IDM_LOCK)
				score++;
		}

		/* Drives have even number */
		if (!(lock->drive_num & 1) &&
		    (score >= (lock->drive_num >> 1))) {
			ilm_raid_lock_dump("Exit raid_renew_lock", lock);
			return 0;
		}

		/* Drives have odd number */
		if ((lock->drive_num & 1) &&
		    (score >= ((lock->drive_num >> 1) + 1))) {
			ilm_raid_lock_dump("Exit raid_renew_lock", lock);
			return 0;
		}

	} while (ilm_curr_time() < timeout);

	ilm_log_dbg("%s: Timeout", __func__);

	/* Timeout, fail to acquire lock with majoirty */
	return -1;
}

int idm_raid_read_lvb(struct ilm_lock *lock, char *host_id,
		      char *lvb, int lvb_size)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i;
	uint64_t max_vb = 0, vb;

	ilm_raid_lock_dump("Enter raid_read_lvb", lock);

	assert(lvb_size == sizeof(uint64_t));

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		if (drive->is_brk == 1) {
			/*
			 * If find any IDM is broken when acquire it, though the
			 * mutex has been granted but its VB cannot be trusted,
			 * the reason is if a prior host has acquired the same
			 * mutex but this prior host has no chance to update VB
			 * into drive due to drive failures.
			 *
			 * To safely reslove this issue, set a reserved value
			 * -1ULL and pass it to lvmlockd; lvmlockd will handle
			 * this special value to force invalidating metadata.
			 */
			max_vb = -1ULL;
			memcpy(lvb, (char *)&max_vb, sizeof(uint64_t));
			return 0;
		}
	}

	do {
		idm_raid_multi_issue(lock, host_id, ILM_OP_READ_LVB, lock->mode, 0);

		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];

			if (!drive->result && drive->state == IDM_LOCK) {
				score++;
				/*
				 * FIXME: so far VB only has 8 bytes, so simply
				 * use uint64_t to compare the maximum value.
				 * Need to fix this if VB is longer than 8 bytes.
				 */
				memcpy(&vb, drive->vb, IDM_VALUE_LEN);

				if (vb > max_vb)
					max_vb = vb;

				ilm_log_dbg("%s: i %d vb=%lx max_vb=%lx\n",
					    __func__, i, vb, max_vb);
			}
		}

		if (score >= ((lock->drive_num >> 1) + 1)) {
			memcpy(lvb, (char *)&max_vb, sizeof(uint64_t));
			ilm_raid_lock_dump("Exit raid_read_lvb", lock);
			return 0;
		}
	} while (ilm_curr_time() < timeout);

	ilm_log_dbg("%s: Timeout", __func__);

	/* Timeout, return failure */
	return -1;
}

/*
 * Read back the user count for IDM.  It's possible that different drive has
 * the different user count, so let's firstly do the statistic for user count
 * with below kinds:
 *
 * - stat[0]: the number of drives which its idm has zero user count, or the
 *   IDM has not been allocated at all in the drive;
 * - stat[1]: the number of drives which its idm has one user count;
 * - stat[2]: the number of drives which its idm has user count >= 2.
 */
int idm_raid_count(struct ilm_lock *lock, char *host_id, int *count, int *self)
{
	int i;
	int cnt = 0, slf = 0, no_ent = 0;
	struct ilm_drive *drive;

	idm_raid_multi_issue(lock, host_id, ILM_OP_COUNT, lock->mode, 0);

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		if (!drive->result) {
			if (drive->count > cnt)
				cnt = drive->count;

			if (drive->self > slf)
				slf = drive->self;
		}

		if (drive->result)
			no_ent++;
	}

	/* The IDM doesn't exist */
	if (no_ent == lock->drive_num)
		return -ENOENT;

	*count = cnt;
	*self = slf;
	return 0;
}

/*
 * Statistic for lock mode in RAID drives, it's possible that drives have
 * different lock mode after drive's failure.  So find out the highest number
 * of drives which have the same lock mode, and if the sum number can achieve
 * the majority, this means the lock mode can be trusted and return to upper
 * user.  Otherwise, return failure.
 */
int idm_raid_mode(struct ilm_lock *lock, int *mode)
{
	int i, m;
	int stat_mode[3] = { 0 }, mode_max = 0, no_ent = 0;
	struct ilm_drive *drive;

	idm_raid_multi_issue(lock, NULL, ILM_OP_MODE, lock->mode, 0);

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		if (!drive->result) {
			m = drive->mode;
			if (m < 3 && m >= 0)
				stat_mode[m]++;
			else if (m >= 3)
				stat_mode[2]++;
			else
				ilm_log_warn("wrong idm mode %d\n", m);
		}

		if (drive->result)
			no_ent++;
	}

	/* The IDM doesn't exist */
	if (no_ent == lock->drive_num)
		return -ENOENT;

	/* Figure out which index is maximum */
	for (i = 0; i < 3; i++) {
		if (stat_mode[i] > stat_mode[mode_max])
			mode_max = i;
	}

	if (stat_mode[mode_max] >= ((lock->drive_num >> 1) + 1)) {
		*mode = mode_max;
		return 0;
	}

	return -1;
}
