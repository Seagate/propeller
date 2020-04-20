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
#include <unistd.h>

#include "ilm.h"

#include "idm_wrapper.h"
#include "inject_fault.h"
#include "lock.h"
#include "log.h"
#include "string.h"
#include "util.h"

#define EALL				0xDEADBEAF

/* Timeout 5s (5000ms) */
#define ILM_MAJORITY_TIMEOUT		5000

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
};

struct _raid_state_transition {
	int curr;
	int result;
	int next;
};

struct _raid_request_data {
	int op;

	struct ilm_lock *lock;
	char *host_id;
	struct ilm_drive *drive;

	char *lvb;
	int lvb_size;
	int count;
	int mode;
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
};

static int _raid_send_request(struct _raid_request_data *data, int op)
{
	struct ilm_lock *lock = data->lock;
	struct ilm_drive *drive = data->drive;
	int ret;

	switch (op) {
	case ILM_OP_LOCK:
		ret = idm_drive_lock(lock->id, data->mode,
				     data->host_id, drive->path,
				     lock->timeout);
		break;
	case ILM_OP_UNLOCK:
		ret = idm_drive_unlock(lock->id, data->host_id,
				       data->lvb, data->lvb_size,
				       drive->path);
		break;
	case ILM_OP_CONVERT:
		ret = idm_drive_convert_lock(lock->id, data->mode,
					     data->host_id, drive->path);
		break;
	case ILM_OP_BREAK:
		ret = idm_drive_break_lock(lock->id, data->mode, data->host_id,
					   drive->path, lock->timeout);
		break;
	case ILM_OP_RENEW:
		ret = idm_drive_renew_lock(lock->id, data->mode, data->host_id,
					   drive->path);
		break;
	case ILM_OP_READ_LVB:
		ret = idm_drive_read_lvb(lock->id, data->host_id,
					 data->lvb, data->lvb_size,
					 drive->path);
		break;
	case ILM_OP_COUNT:
		ret = idm_drive_lock_count(lock->id, &data->count, drive->path);
		break;
	case ILM_OP_MODE:
		ret = idm_drive_lock_mode(lock->id, &data->mode, drive->path);
		break;
	default:
		assert(1);
		break;
	}

	return ret;
}

static int _raid_state_machine_end(int state)
{
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
		       func == ILM_OP_MODE);
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
	default:
		ilm_log_err("%s: unsupported state %d", __func__, state);
		break;
	}

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

static int ilm_raid_handle_request(struct _raid_request_data *data)
{
	struct ilm_drive *drive = data->drive;
	int state, next_state = drive->state;
	int op, ret;

	do {
		state = drive->state;

		op = _raid_state_find_op(state, data->op);

		ret = _raid_send_request(data, op);

		if (next_state == IDM_INIT && op == ILM_OP_COUNT)
			next_state = _raid_state_lockup(next_state, 1);
		else if (next_state == IDM_INIT && op == ILM_OP_MODE)
			next_state = _raid_state_lockup(next_state, 1);
		else if (next_state == IDM_LOCK && op == ILM_OP_UNLOCK)
			next_state = _raid_state_lockup(next_state, 1);
		else
			next_state = _raid_state_lockup(next_state, ret);

		ilm_log_dbg("%s: prev_state=%d op=%d return=%d next_state=%d",
			    __func__, state, op, ret, next_state);

		drive->state = next_state;
	} while (!_raid_state_machine_end(next_state));

	return ret;
}

static void ilm_raid_lock_dump(char *str, struct ilm_lock *lock)
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
	int score, i, ret;
	struct _raid_request_data data;

	/* Initialize all drives state to NO_ACCESS */
	for (i = 0; i < lock->drive_num; i++)
		lock->drive[i].state = IDM_INIT;

	ilm_raid_lock_dump("Enter raid_lock", lock);

	do {
		score = 0;
		io_err = 0;
		for (i = 0; i < lock->drive_num; i++) {
			/* Update inject fault */
			ilm_inject_fault_update(lock->drive_num, i);

			drive = &lock->drive[i];

			data.op = ILM_OP_LOCK;
			data.lock = lock;
			data.host_id = host_id;
			data.drive = drive;
			data.mode = lock->mode;
			data.lvb = NULL;
			data.lvb_size = 0;
			data.count = 0;

			ret = ilm_raid_handle_request(&data);

			/* Make success in one drive */
			if (!ret && drive->state == IDM_LOCK)
				score++;

			if (ret == -EIO)
				io_err++;

			/*
			 * NOTE: don't change the drive state for any failure;
			 * keep the drive state as NO_ACCESS and later can try
			 * to acquire the lock.
			 */
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
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			if (drive->state != IDM_LOCK)
				continue;

			data.op = ILM_OP_UNLOCK;
			data.lock = lock;
			data.host_id = host_id;
			data.drive = drive;
			data.mode = lock->mode;
			data.lvb = NULL;
			data.lvb_size = 0;

			ilm_raid_handle_request(&data);
		}

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
	struct _raid_request_data data;
	int io_err = 0, timeout = 0;
	int i, ret;

	ilm_raid_lock_dump("Enter raid_unlock", lock);

	io_err = 0;
	for (i = 0; i < lock->drive_num; i++) {
		/* Update inject fault */
		ilm_inject_fault_update(lock->drive_num, i);

		drive = &lock->drive[i];
		if (drive->state == IDM_INIT)
			continue;

		data.op = ILM_OP_UNLOCK;
		data.lock = lock;
		data.host_id = host_id;
		data.drive = drive;
		data.mode = lock->mode;
		data.lvb = lock->vb;
		data.lvb_size = IDM_VALUE_LEN;

		ret = ilm_raid_handle_request(&data);

		if (ret == -ETIME)
			timeout++;

		if (ret == -EIO)
			io_err++;

		/* Always make success to unlock */
		assert(drive->state == IDM_INIT);
	}

	/* All drives have been timeout */
	if (timeout >= (lock->drive_num - (lock->drive_num >> 1)))
		return -ETIME;

	if (io_err >= (lock->drive_num - (lock->drive_num >> 1)))
		return -EIO;

	ilm_raid_lock_dump("Exit raid_unlock", lock);
	return ret;
}

int idm_raid_convert_lock(struct ilm_lock *lock, char *host_id, int mode)
{
	struct ilm_drive *drive;
	struct _raid_request_data data;
	int io_err = 0;
	int i, score, ret, timeout = 0;

	ilm_raid_lock_dump("Enter raid_convert_lock", lock);

	/*
	 * If fail to convert mode previously, afterwards cannot convert
	 * mode anymore.
	 */
	if (lock->convert_failed == 1)
		return -1;

	score = 0;
	for (i = 0; i < lock->drive_num; i++) {
		/* Update inject fault */
		ilm_inject_fault_update(lock->drive_num, i);

		drive = &lock->drive[i];

		data.op = ILM_OP_CONVERT;
		data.lock = lock;
		data.host_id = host_id;
		data.drive = drive;
		data.mode = mode;
		data.lvb = NULL;
		data.lvb_size = 0;

		ret = ilm_raid_handle_request(&data);

		if (!ret)
			score++;

		if (ret == -ETIME)
			timeout++;

		if (ret == -EIO)
			io_err++;
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

	score = 0;
	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		data.op = ILM_OP_CONVERT;
		data.lock = lock;
		data.host_id = host_id;
		data.drive = drive;
		data.mode = lock->mode;
		data.lvb = NULL;
		data.lvb_size = 0;

		ret = ilm_raid_handle_request(&data);
		if (!ret)
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
	struct _raid_request_data data;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;

	ilm_raid_lock_dump("Enter raid_renew_lock", lock);

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			/* Update inject fault */
			ilm_inject_fault_update(lock->drive_num, i);

			drive = &lock->drive[i];

			data.op = ILM_OP_RENEW;
			data.lock = lock;
			data.host_id = host_id;
			data.drive = drive;
			data.mode = lock->mode;
			data.lvb = NULL;
			data.lvb_size = 0;

			ret = ilm_raid_handle_request(&data);
			if (!ret)
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
	struct _raid_request_data data;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;
	uint64_t max_vb = 0, vb;

	ilm_raid_lock_dump("Enter raid_read_lvb", lock);

	assert(lvb_size == sizeof(uint64_t));

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			/* Update inject fault */
			ilm_inject_fault_update(lock->drive_num, i);

			drive = &lock->drive[i];
			data.op = ILM_OP_READ_LVB;
			data.lock = lock;
			data.host_id = host_id;
			data.drive = drive;
			data.mode = lock->mode;
			data.lvb = (void *)&vb;
			data.lvb_size = IDM_VALUE_LEN;

			ret = ilm_raid_handle_request(&data);
			if (!ret) {
				score++;

				/*
				 * FIXME: so far VB only has 8 bytes, so simply
				 * use uint64_t to compare the maximum value.
				 * Need to fix this if VB is longer than 8 bytes.
				 */
				if (vb > max_vb)
					max_vb = vb;

				ilm_log_err("%s: i %d vb=%lx max_vb=%lx\n",
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
int idm_raid_count(struct ilm_lock *lock, int *count)
{
	int ret, i;
	int cnt;
	int stat[3] = { 0 }, stat_max = 0, no_ent = 0;
	struct ilm_drive *drive;
	struct _raid_request_data data;

	for (i = 0; i < lock->drive_num; i++) {
		/* Update inject fault */
		ilm_inject_fault_update(lock->drive_num, i);

		drive = &lock->drive[i];

		data.op = ILM_OP_COUNT;
		data.lock = lock;
		data.host_id = NULL;
		data.drive = drive;
		data.mode = lock->mode;

		ret = ilm_raid_handle_request(&data);

		if (ret) {
			no_ent++;
			continue;
		}

		cnt = data.count;
		if (cnt >= 0 && cnt < 3)
			stat[cnt]++;
		else if (cnt >= 3)
			stat[2]++;
		else
			ilm_log_warn("wrong idm count %d\n", cnt);
	}

	/* The IDM doesn't exist */
	if (no_ent == lock->drive_num)
		return -ENOENT;

	/* Figure out which index is maximum */
	for (i = 0; i < 3; i++) {
		if (stat[i] > stat[stat_max])
			stat_max = i;
	}

	if (stat[stat_max] >= ((lock->drive_num >> 1) + 1)) {
		*count = stat_max;
		return 0;
	}

	/* Otherwise, cannot achieve majority and return failure */
	return -1;
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
	int ret, i;
	int m, t;
	int stat_mode[3] = { 0 }, mode_max = 0, no_ent = 0;
	struct ilm_drive *drive;
	struct _raid_request_data data;

	for (i = 0; i < lock->drive_num; i++) {
		/* Update inject fault */
		ilm_inject_fault_update(lock->drive_num, i);

		drive = &lock->drive[i];

		data.op = ILM_OP_MODE;
		data.lock = lock;
		data.host_id = NULL;
		data.drive = drive;

		ret = ilm_raid_handle_request(&data);
		if (ret) {
			no_ent++;
			continue;
		}

		m = data.mode;
		if (m < 3 && m >= 0)
			stat_mode[m]++;
		else if (m >= 3)
			stat_mode[2]++;
		else
			ilm_log_warn("wrong idm mode %d\n", m);
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

/*
 * TODO: statistic for hosts state is not useful at current stage.
 * Let's implement later for function idm_raid_host_state().
 */
