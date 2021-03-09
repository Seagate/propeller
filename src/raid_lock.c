/*
 * Copyright (C) 2020-2021 Seagate
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

	char *path;
	int path_idx;

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

static const char *_raid_state_str(int state)
{
	if (state == IDM_INIT)
		return "IDM_INIT";
	if (state == IDM_BUSY)
		return "IDM_BUSY";
	if (state == IDM_DUPLICATE)
		return "IDM_DUPLICATE";
	if (state == IDM_LOCK)
		return "IDM_LOCK";
	if (state == IDM_TIMEOUT)
		return "IDM_TIMEOUT";
	if (state == IDM_FAULT)
		return "IDM_FAULT";

	return "UNKNOWN STATE";
}

static const char *_raid_op_str(int op)
{
	if (op == ILM_OP_LOCK)
		return "ILM_OP_LOCK";
	if (op == ILM_OP_UNLOCK)
		return "ILM_OP_UNLOCK";
	if (op == ILM_OP_CONVERT)
		return "ILM_OP_CONVERT";
	if (op == ILM_OP_BREAK)
		return "ILM_OP_BREAK";
	if (op == ILM_OP_RENEW)
		return "ILM_OP_RENEW";
	if (op == ILM_OP_WRITE_LVB)
		return "ILM_OP_WRITE_LVB";
	if (op == ILM_OP_READ_LVB)
		return "ILM_OP_READ_LVB";
	if (op == ILM_OP_COUNT)
		return "ILM_OP_COUNT";
	if (op == ILM_OP_MODE)
		return "ILM_OP_MODE";

	return "UNKNOWN OP";
}

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
					     drive->path, lock->timeout);
		break;
	case ILM_OP_BREAK:
		ret = idm_drive_break_lock(lock->id, req->mode, req->host_id,
					   drive->path, lock->timeout);
		if (!ret)
			drive->is_brk = 1;
		break;
	case ILM_OP_RENEW:
		ret = idm_drive_renew_lock(lock->id, req->mode, req->host_id,
					   drive->path, lock->timeout);
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
	ilm_inject_fault_update(lock->good_drive_num, drive->index);

	switch (req->op) {
	case ILM_OP_LOCK:
		drive->is_brk = 0;
		ret = idm_drive_lock_async(lock->id, req->mode, req->host_id,
					   req->path, lock->timeout, &handle);
		break;
	case ILM_OP_UNLOCK:
		ret = idm_drive_unlock_async(lock->id, req->mode, req->host_id,
					     req->lvb, req->lvb_size,
					     req->path, &handle);
		break;
	case ILM_OP_CONVERT:
		ret = idm_drive_convert_lock_async(lock->id, req->mode,
						   req->host_id, req->path,
						   lock->timeout, &handle);
		break;
	case ILM_OP_BREAK:
		ret = idm_drive_break_lock_async(lock->id, req->mode,
						 req->host_id, req->path,
						 lock->timeout, &handle);
		break;
	case ILM_OP_RENEW:
		ret = idm_drive_renew_lock_async(lock->id, req->mode,
						 req->host_id, req->path,
						 lock->timeout, &handle);
		break;
	case ILM_OP_READ_LVB:
		ret = idm_drive_read_lvb_async(lock->id, req->host_id,
					       req->path, &handle);
		break;
	case ILM_OP_COUNT:
		ret = idm_drive_lock_count_async(lock->id, req->host_id,
						 req->path, &handle);
		break;
	case ILM_OP_MODE:
		ret = idm_drive_lock_mode_async(lock->id, req->path, &handle);
		break;
	default:
		ret = -EINVAL;
		assert(1);
		break;
	}

	req->handle = handle;
	req->result = ret;

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
		ilm_log_err("%s: unsupported op=%d", __func__, req->op);
		ret = -1;
		break;
	}

	if (ret)
		ilm_log_err("%s: ret=%d", __func__, ret);

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
		       func == ILM_OP_CONVERT || func == ILM_OP_READ_LVB);

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

	ilm_log_dbg("%s: state=%s(%d) orignal op=%s(%d) op=%s(%d)",
		    __func__, _raid_state_str(state), state,
		    _raid_op_str(func), func,
		    _raid_op_str(op), op);

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

	ilm_log_err("%s: state machine malfunction state=%d result=%d",
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
	else if (state == IDM_INIT && req->op == ILM_OP_READ_LVB)
		result = 1;
	else if (state == IDM_LOCK && req->op == ILM_OP_UNLOCK)
		result = 1;

	next_state = _raid_state_lockup(state, result);

	ilm_log_err("raid_lock state transition: drive=%s op=%s(%d) result=%d state=%s(%d) -> next_state=%s(%d)",
		    req->path, _raid_op_str(req->op),
		    req->op, result,
		    _raid_state_str(drive->state), drive->state,
		    _raid_state_str(next_state), next_state);

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

	pthread_mutex_unlock(&raid_th->request_mutex);

	if (req->renew)
		raid_th->renew_count++;
	else
		raid_th->count++;

	ilm_log_err("raid_lock send request: drive=%s state=%s(%d) op=%s(%d) mode=%d renew=%d",
		    req->path, _raid_state_str(drive->state), drive->state,
		    _raid_op_str(req->op), req->op, req->mode, req->renew);
	ilm_log_err("  -> raid_thread=%p renew_count=%d count=%d",
		    raid_th, raid_th->renew_count, raid_th->count);

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

	ilm_log_dbg("%s: response [drive=%s]", __func__, req->path);
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

	ilm_log_dbg("%s: renew response [drive=%s]", __func__, req->path);
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
		pthread_cond_signal(&raid_th->renew_cond);
		pthread_mutex_unlock(&raid_th->renew_mutex);
	} else {
		pthread_mutex_lock(&raid_th->response_mutex);
		list_add_tail(&req->list, &raid_th->response_list);
		pthread_cond_signal(&raid_th->response_cond);
		pthread_mutex_unlock(&raid_th->response_mutex);
	}

	ilm_log_dbg("[raid_thread=%p] <- add [drive=%s] result to %s list",
		    raid_th, req->path, req->renew ? "renew" : "response");
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
		}

		pthread_mutex_unlock(&raid_th->request_mutex);

		list_for_each_entry_safe(req, tmp,
				         &raid_th->process_list, list) {
			ret = _raid_dispatch_request_async(req);

			ilm_log_err("[raid_thread=%p] -> (async) drive=%s op=%s(%d) ret=%d",
				    raid_th, req->path, _raid_op_str(req->op),
				    req->op, ret);

			if (ret < 0) {
				req->result = ret;
				/* Remove from process list */
				list_del(&req->list);
				idm_raid_notify(raid_th, req);
			}

			//_raid_dispatch_request(req);
		}

		poll_fd = malloc(sizeof(struct pollfd) * process_num);
		if (!poll_fd) {
			ilm_log_err("[raid_thread=%p] cannot allcoate pollfd",
				    raid_th);
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
					ilm_log_err("[raid_thread=%p] fail to find request for polling fd %d",
						    raid_th, poll_fd[i].fd);
					continue;
				}

				_raid_read_result_async(req);

				ilm_log_err("[raid_thread=%p] <- (resp) drive=%s op=%s(%d) result=%d",
					    raid_th, req->path, _raid_op_str(req->op),
					    req->op, req->result);

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

	ilm_log_dbg("%s: raid_thread=%p is freed", __func__, raid_th);
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
		ilm_log_err("Fail to create raid thread");
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
	ilm_log_dbg("%s: raid_thread=%p is created", __func__, raid_th);
	return 0;
}

static void idm_raid_destroy(char *path)
{
	struct idm_info *info_list, *info, *least_renew = NULL;
	int info_num;
	int ret, i;
	uint64_t least_renew_time = -1ULL;
	char uuid_str[39];	/* uuid string is 39 chars + '\0' */

	ret = idm_drive_read_group(path, &info_list, &info_num);
	if (ret)
		return;

	/*
	 * This is to implement the Least Recently Used (LRU) algorithm by
	 * traversing all items
	 */
	for (i = 0; i < info_num; i++) {
		info = info_list + i;

		/* If the mutex is not unlock, skip it */
		if (info->state != IDM_MODE_UNLOCK)
			continue;

		if (least_renew_time > info->last_renew_time) {
			least_renew = info;
			least_renew_time = info->last_renew_time;
		}
	}

	if (!least_renew)
		return;

	ilm_log_array_err("raid_destroy: lock ID", least_renew->id, IDM_LOCK_ID_LEN);
	ilm_id_write_format(least_renew->id, uuid_str, sizeof(uuid_str));
	if (strlen(uuid_str))
		ilm_log_err("raid_destroy: lock ID (VG): %s", uuid_str);
	else
		ilm_log_err("raid_destroy: lock ID (VG): Empty string");

	ilm_id_write_format(least_renew->id + 32, uuid_str, sizeof(uuid_str));
	if (strlen(uuid_str))
		ilm_log_err("raid_destroy: lock ID (LV): %s", uuid_str);
	else
		ilm_log_err("raid_destroy: lock ID (LV): Empty string");

	ilm_log_err("raid_destroy: state=%d mode=%d last_renew_time=%lu",
		    least_renew->state, least_renew->mode,
		    least_renew->last_renew_time);

	idm_drive_destroy(least_renew->id, least_renew->mode,
			  least_renew->host_id, path);
}

static void idm_raid_multi_issue(struct ilm_lock *lock, char *host_id,
				 int op, int mode, int renew)
{
	struct ilm_drive *drive;
	struct _raid_request *req;
	int i, reverse_mode;

	ilm_log_dbg("%s: start mutex op=%s(%d) mode=%d renew=%d",
		    __func__, _raid_op_str(op), op, mode, renew);

	ilm_update_drive_multi_paths(lock);

	for (i = 0; i < lock->good_drive_num; i++) {

		drive = &lock->drive[i];

		if (drive->state == IDM_INIT && op == ILM_OP_UNLOCK) {
			drive->result = 0;
			continue;
		}

		/*
		 * If failed to fetch the drive's paths, skip the send
		 * command and directly return error -EIO.
		 */
		if (drive->path_num == 0 || drive->path[0] == NULL) {
			ilm_log_err("%s: cannot find any path for drive with wwn 0x%lx\n",
				    __func__, drive->wwn);
			drive->result = -EIO;
			continue;
		}

		req = malloc(sizeof(struct _raid_request));
		if (!req) {
			drive->result = -ENOMEM;
			continue;
		}

		memset(req, 0, sizeof(struct _raid_request));

		req->op = op;
		req->lock = lock;
		req->host_id = host_id;
		req->drive = drive;
		req->mode = (mode != -1) ? mode : lock->mode;
		req->renew = renew;
		req->path_idx = 0;

		/*
		 * Since the drive pathes might be altered by other requesters,
		 * duplicate the drive path at this point to avoid the
		 * use-after-free issue.
		 */
		req->path = strdup(drive->path[req->path_idx]);
		if (!req->path) {
			free(req);
			drive->result = -ENOMEM;
			continue;
		}

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

		drive = req->drive;

		/*
		 * Detect the I/O failure, we can try another path for the same
		 * drive, this can allow us to have more chance to make success
		 * for the request.
		 */
		if (req->result == -EIO &&
		    (req->path_idx < drive->path_num -1) &&
		    (drive->path[req->path_idx + 1] != NULL)) {

			ilm_log_dbg("%s: I/O failure path=%s", __func__, req->path);

			/* Free the previous drive path */
			free(req->path);

			req->path_idx++;
			req->path = strdup(drive->path[req->path_idx]);
			if (req->path) {
				ilm_log_dbg("%s: New path selection: idx=%d path=%s",
					    __func__, req->path_idx, req->path);
				goto send_next_request;
			}
		}

		idm_raid_state_transition(req);

		/* Drive compliants no free memory, destroy mutex */
		if (drive->state == IDM_INIT && req->result == -ENOMEM)
			idm_raid_destroy(req->path);

		/*
		 * When release mutex, if returns -EINVAL usually it means
		 * it passes wrong lock mode, this might be caused by the
		 * previous user forgot to release mutex.  For this case,
		 * try to cleanup the context by destroying the mutex,
		 * and needs to revert the locking mode so can allow drive
		 * firmware to destroy mutex successfully.
		 */
		if (drive->state == IDM_INIT && req->result == -EINVAL &&
		    req->op == ILM_OP_UNLOCK) {
			if (req->mode == IDM_MODE_EXCLUSIVE)
				reverse_mode = IDM_MODE_SHAREABLE;
			else
				reverse_mode = IDM_MODE_EXCLUSIVE;

			idm_drive_unlock(lock->id, reverse_mode, req->host_id,
					 req->lvb, req->lvb_size, req->path);
			idm_drive_destroy(lock->id, reverse_mode,
					  req->host_id, req->path);
		}

		/*
		 * When convert lock mode from shareable to exclusive, if
		 * there have other hosts have been timeout, it returns error
		 * -EPERM.  For this case, needs to use break operation to
		 * dismiss the hosts have been timeout, and it can promote
		 * lock mode to exclusive.
		 */
		if (drive->state == IDM_LOCK && req->result == -EPERM &&
		    req->op == ILM_OP_CONVERT && req->mode == IDM_MODE_EXCLUSIVE) {
			req->result = idm_drive_break_lock(lock->id, req->mode,
				req->host_id, req->path, lock->timeout);
		}

		if (_raid_state_machine_end(drive->state)) {
			drive->result = req->result;
			drive->mode = req->mode;
			drive->count = req->count;
			drive->self = req->self;
			ilm_log_dbg("%s: drive result=%d mode=%d count=%d", __func__,
				    drive->result, drive->mode, drive->count);
			free(req->path);
			free(req);
			continue;
		}

send_next_request:
		idm_raid_add_request(lock->raid_th, req);
		idm_raid_signal_request(lock->raid_th);
	}

	return;
}

static void ilm_raid_lock_dump(const char *str, struct ilm_lock *lock)
{
	int i, j;

	ilm_log_err("<<<<< RAID lock dump: %s <<<<<", str);

	for (i = 0; i < lock->good_drive_num; i++) {
		ilm_log_err("drive[%d] state=%d", i, lock->drive[i].state);
		for (j = 0; j < lock->drive[i].path_num; j++)
			ilm_log_err("  path=%s", lock->drive[i].path[j]);
	}

	ilm_log_err(">>>>> RAID lock dump: %s >>>>>", str);
}

int idm_raid_lock(struct ilm_lock *lock, char *host_id)
{
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	struct ilm_drive *drive;
	int rand_sleep;
	int io_err;
	int score, i;

	/* Initialize all drives state to NO_ACCESS */
	for (i = 0; i < lock->good_drive_num; i++)
		lock->drive[i].state = IDM_INIT;

	ilm_raid_lock_dump("raid_lock", lock);

	do {
		idm_raid_multi_issue(lock, host_id, ILM_OP_LOCK, lock->mode, 0);

		score = 0;
		io_err = 0;
		for (i = 0; i < lock->good_drive_num; i++) {
			drive = &lock->drive[i];

			if (!drive->result && drive->state == IDM_LOCK)
				score++;

			if (drive->result == -EIO)
				io_err++;
		}

		/* Acquired majoirty */
		if (score >= ((lock->total_drive_num >> 1) + 1)) {
			ilm_log_dbg("%s: success", __func__);
			return 0;
		}

		/*
                 * Fail to achieve majority, release IDMs has been
		 * acquired; race for next round.
		 */
		idm_raid_multi_issue(lock, host_id, ILM_OP_UNLOCK, lock->mode, 0);

		/*
		 * Sleep for random interval for sleep, the interval range
		 * is [500 .. 1000] us; this can avoid the multiple hosts keeping
		 * the same pace thus every one cannot acquire majority (e.g.
		 * two hosts, every host only can acquire successfully half
		 * drives, and finally no host can achieve the majority).
		 */
		rand_sleep = ilm_rand(500, 1000);
		usleep(rand_sleep);

	} while (ilm_curr_time() < timeout);

	ilm_raid_lock_dump("raid_lock failed", lock);

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
	if ((io_err + lock->fail_drive_num) >=
			(lock->total_drive_num - (lock->total_drive_num >> 1)))
		return -EIO;

	/* Timeout, fail to acquire lock with majoirty */
	return -1;
}

int idm_raid_unlock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	int io_err = 0, timeout = 0;
	int i, ret = 0;

	ilm_raid_lock_dump("raid_unlock", lock);

	idm_raid_multi_issue(lock, host_id, ILM_OP_UNLOCK, lock->mode, 0);

	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];

		if (drive->result == -EIO)
			io_err++;

		if (drive->result == -ETIME)
			timeout++;
	}

	/* All drives have been timeout */
	if (timeout >= (lock->total_drive_num - (lock->total_drive_num >> 1)))
		ret = -ETIME;

	if ((io_err + lock->fail_drive_num) >=
			(lock->total_drive_num - (lock->total_drive_num >> 1)))
		ret = -EIO;

	if (ret)
		ilm_raid_lock_dump("raid_unlock failed", lock);

	return ret;
}

int idm_raid_convert_lock(struct ilm_lock *lock, char *host_id, int mode)
{
	struct ilm_drive *drive;
	int io_err = 0;
	int i, score, timeout = 0;

	ilm_raid_lock_dump("raid_convert_lock", lock);

	/*
	 * If fail to convert mode previously, afterwards cannot convert
	 * mode anymore.
	 */
	if (lock->convert_failed == 1) {
		ilm_log_err("%s: failed to convert mode previously, directly bail out",
			    __func__);
		return -1;
	}

	idm_raid_multi_issue(lock, host_id, ILM_OP_CONVERT, mode, 0);

	score = 0;
	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];

		if (!drive->result && drive->state == IDM_LOCK)
			score++;

		if (drive->result == -EIO)
			io_err++;

		if (drive->result == -ETIME)
			timeout++;
	}

	/* Has achieved majoirty */
	if (score >= ((lock->total_drive_num >> 1) + 1)) {
		ilm_log_dbg("%s: success", __func__);
		return 0;
	}

	ilm_raid_lock_dump("raid_convert_lock failed", lock);

	/* Majority drives have been timeout */
	if (timeout >= (lock->total_drive_num - (lock->total_drive_num >> 1)))
		return -ETIME;

	/* Majority drives have I/O error */
	if ((io_err + lock->fail_drive_num) >=
			(lock->total_drive_num - (lock->total_drive_num >> 1)))
		return -EIO;

	/*
	 * Always return success when the mode is demotion, if it fails to
	 * acheive majority with demotion, set the flag 'convert_failed'
	 * so disallow to convert mode anymore, this can avoid further mess.
	 */
	if (lock->mode == IDM_MODE_EXCLUSIVE && mode == IDM_MODE_SHAREABLE) {
		lock->convert_failed = 1;
		ilm_log_warn("%s: emotion from ex to sh", __func__);
		return 0;
	}

	idm_raid_multi_issue(lock, host_id, ILM_OP_CONVERT, lock->mode, 0);

	score = 0;
	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];

		if (!drive->result && drive->state == IDM_LOCK)
			score++;
	}

	/* Unfortunately, fail to revert to old mode. */
	if (score < ((lock->total_drive_num >> 1) + 1)) {
		ilm_log_warn("Fail to revert lock mode, old %d vs new %d",
			     lock->mode, mode);
		ilm_log_warn("Defer to resolve this when renew the lock");
		lock->convert_failed = 1;
	}

	return -1;
}

int idm_raid_renew_lock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i;

	ilm_raid_lock_dump("raid_renew_lock", lock);

	do {
		idm_raid_multi_issue(lock, host_id, ILM_OP_RENEW, lock->mode, 1);

		score = 0;
		for (i = 0; i < lock->good_drive_num; i++) {
			drive = &lock->drive[i];

			if (!drive->result && drive->state == IDM_LOCK)
				score++;
		}

		/* Drives have even number */
		if (!(lock->total_drive_num & 1) &&
		    (score >= (lock->total_drive_num >> 1))) {
			ilm_log_dbg("%s: success", __func__);
			return 0;
		}

		/* Drives have odd number */
		if ((lock->total_drive_num & 1) &&
		    (score >= ((lock->total_drive_num >> 1) + 1))) {
			ilm_log_dbg("%s: success", __func__);
			return 0;
		}

	} while (ilm_curr_time() < timeout);

	ilm_raid_lock_dump("raid_renew_lock failed", lock);

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

	ilm_raid_lock_dump("raid_read_lvb", lock);

	assert(lvb_size == sizeof(uint64_t));

	for (i = 0; i < lock->good_drive_num; i++) {
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
		for (i = 0; i < lock->good_drive_num; i++) {
			drive = &lock->drive[i];

			if (!drive->result) {
				score++;
				/*
				 * FIXME: so far VB only has 8 bytes, so simply
				 * use uint64_t to compare the maximum value.
				 * Need to fix this if VB is longer than 8 bytes.
				 */
				memcpy(&vb, drive->vb, IDM_VALUE_LEN);

				if (vb > max_vb)
					max_vb = vb;

				ilm_log_dbg("%s: i %d vb=%lx max_vb=%lx",
					    __func__, i, vb, max_vb);
			}
		}

		if (score >= ((lock->total_drive_num >> 1) + 1)) {
			memcpy(lvb, (char *)&max_vb, sizeof(uint64_t));
			ilm_log_dbg("%s: LVB is %lx", __func__, max_vb);
			return 0;
		}
	} while (ilm_curr_time() < timeout);

	ilm_raid_lock_dump("raid_read_lvb failed", lock);

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

	ilm_raid_lock_dump("raid_count", lock);

	idm_raid_multi_issue(lock, host_id, ILM_OP_COUNT, lock->mode, 0);

	for (i = 0; i < lock->good_drive_num; i++) {
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
	if (no_ent == lock->good_drive_num) {
		ilm_raid_lock_dump("raid_count failed", lock);
		ilm_log_err("%s: no mutex entries", __func__);
		return -ENOENT;
	}

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

	ilm_raid_lock_dump("raid_mode", lock);

	idm_raid_multi_issue(lock, NULL, ILM_OP_MODE, lock->mode, 0);

	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];

		if (!drive->result) {
			m = drive->mode;
			if (m < 3 && m >= 0)
				stat_mode[m]++;
			else if (m >= 3)
				stat_mode[2]++;
			else
				ilm_log_warn("wrong idm mode %d", m);
		}

		if (drive->result)
			no_ent++;
	}

	/* The IDM doesn't exist */
	if (no_ent == lock->good_drive_num) {
		ilm_raid_lock_dump("raid_mode failed", lock);
		ilm_log_err("%s: no mutex entries", __func__);
		return -ENOENT;
	}

	/* Figure out which index is maximum */
	for (i = 0; i < 3; i++) {
		if (stat_mode[i] > stat_mode[mode_max])
			mode_max = i;
	}

	if (stat_mode[mode_max] >= ((lock->total_drive_num >> 1) + 1)) {
		*mode = mode_max;
		return 0;
	}

	ilm_raid_lock_dump("raid_mode failed", lock);
	return -1;
}
