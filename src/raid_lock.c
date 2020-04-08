/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <unistd.h>

#include "ilm.h"

#include "idm_wrapper.h"
#include "lock.h"
#include "log.h"
#include "util.h"

#define ILM_DRIVE_NOACCESS		0
#define ILM_DRIVE_ACCESSED		1
#define ILM_DRIVE_FAILED		2

/* Timeout 5s (5000ms) */
#define ILM_MAJORITY_TIMEOUT		5000

static void ilm_raid_lock_dump(char *str, struct ilm_lock *lock)
{
	int i;

	ilm_log_dbg("RAID lock dump: %s", str);
	for (i = 0; i < lock->drive_num; i++)
		ilm_log_dbg("drive[%d]: path=%s state=%d",
			    i, lock->drive[i].path, lock->drive[i].state);
}

/*
 * Every IDM drive's state machine is maintained as below:
 *
 *              +---->  NOACCESS  <----------------------------+
 *              |          |                                   |
 *  Timeout, or |          |  Acquired IDM, needs to break the |
 *  release the |          |  ownership or handle duplicate    |
 *  lock        |          |  acquisition.                     |  Timeout,
 *              |          V                                   |  release
 *              +----  ACCESSED                                |  the lock
 *                         |                                   |
 *                         |  Errors (e.g -EIO, -EFAULT, etc)  |
 *                         |                                   |
 *                         |                                   |
 *                         V                                   |
 *                      FAILED  -------------------------------+
 */
static int _raid_lock(struct ilm_lock *lock, int mode, char *host_id,
		      struct ilm_drive *drive)
{
	int host_state, ret;

	ret = idm_drive_lock(lock->id, mode, host_id, drive->path,
			     lock->timeout);
	if (!ret) {
		return 0;
	/*
	 * The idm has been acquired by other hosts, but not sure if these
	 * hosts have been timeout or not or if set an infinite timeout
	 * value.  If all hosts have been timeout, we can break the idm's
	 * current granting and has chance to take this idm.
	 */
	} else if (ret == -EBUSY) {
#if 0
		/* TBD: if really need to know lock mode or host state?? */
		ret = idm_drive_host_state(lock->lock_id,
					   host_id,
					   &host_state,
					   lock->drive[i]);
		if (ret) {
			ilm_log_warn("%s: fail to get host state\n", __func__);
			return ret;
		}
#endif

		/* Try to break the busy lock and gain it */
		ret = idm_drive_break_lock(lock->id, mode, host_id,
					   drive->path, lock->timeout);
		if (ret) {
			ilm_log_warn("%s: fail to break lock %d\n",
				     __func__, ret);
			return ret;
		}

		/* The host has been granted the idm */
		return 0;

	/*
	 * The idm has been acquired successfully before, but for some reaons
	 * (e.g. the host lost connection with drive and reconnect again) the
	 * host tries to acquire it again and the host's membership isn't
	 * expired.  So it reports the error -EAGAIN.
	 */
	} else if (ret == -EAGAIN) {
		ret = idm_drive_unlock(lock->id, host_id, drive->path);
		/*
		 * If the host has been expired for its membership,
		 * take it as being unlocked.
		 */
		if (ret && ret != -ETIME)
			return ret;

		ret = idm_drive_lock(lock->id, mode, host_id,
				     drive->path, lock->timeout);
		if (ret)
			return ret;

		return 0;
	}

	/* Otherwise, the return value is unsupported */
	return ret;
}

int idm_raid_lock(struct ilm_lock *lock, char *host_id)
{
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	struct ilm_drive *drive;
	int rand_sleep;
	int score, i, ret;

	/* Initialize all drives state to NO_ACCESS */
	for (i = 0; i < lock->drive_num; i++)
		lock->drive[i].state = ILM_DRIVE_NOACCESS;

	ilm_raid_lock_dump("Enter raid_lock", lock);

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			ret = _raid_lock(lock, lock->mode, host_id, drive);
			/* Make success in one drive */
			if (!ret) {
				drive->state = ILM_DRIVE_ACCESSED;
				score++;
			}

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
			if (drive->state != ILM_DRIVE_ACCESSED)
				continue;

			idm_drive_unlock(lock->id, host_id, drive->path);

			/* Always make success to unlock */
			drive->state = ILM_DRIVE_NOACCESS;
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

	/* Timeout, fail to acquire lock with majoirty */
	return -1;
}

int idm_raid_unlock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	int i, ret;

	ilm_raid_lock_dump("Enter raid_unlock", lock);

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];
		ret = idm_drive_unlock(lock->id, host_id, drive->path);

		/* Always make success to unlock */
		drive->state = ILM_DRIVE_NOACCESS;
	}

	ilm_raid_lock_dump("Exit raid_unlock", lock);
	return ret;
}

static int _raid_convert_lock(struct ilm_lock *lock, int mode,
			      char *host_id, struct ilm_drive *drive)
{
	int i, ret;

	switch (drive->state) {
	/*
	 * The drive has not been acquired when the lock is granted,
	 * take this chance to extend majority.
	 */
	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, mode, host_id, drive);
		if (ret)
			return ret;

		drive->state = ILM_DRIVE_ACCESSED;

		/* fall through */

	case ILM_DRIVE_ACCESSED:
	case ILM_DRIVE_FAILED:
		ret = idm_drive_convert_lock(lock->id, mode,
					     host_id, drive->path);
		/* Convert mode successfully */
		if (!ret) {
			drive->state = ILM_DRIVE_ACCESSED;
			return 0;
		}

		/* Drive failure, directly bail out */
		if (ret == -EIO) {
			drive->state = ILM_DRIVE_FAILED;
			return ret;
		}

		/*
		 * If the membership is timeout, needs to release lock and
		 * try to acquire again (in next loop or any new request
		 * later.
		 */
		if (ret == -ETIME) {
			ret = idm_drive_unlock(lock->id, host_id, drive->path);
			drive->state = ILM_DRIVE_NOACCESS;
			return ret;
		}

		/* If returns -EPERM, will return -1 */
		break;
	default:
		ilm_log_err("%s: unexpected drive state %d\n",
			    __func__, drive->state);
		break;
	}

	return -1;
}

int idm_raid_convert_lock(struct ilm_lock *lock, char *host_id, int mode)
{
	struct ilm_drive *drive;
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
		drive = &lock->drive[i];
		ret = _raid_convert_lock(lock, mode, host_id, drive);
		if (!ret)
			score++;

		if (ret == -ETIME)
			timeout++;
	}

	ilm_raid_lock_dump("Finish raid_convert_lock", lock);

	/* Has achieved majoirty */
	if (score >= ((lock->drive_num >> 1) + 1))
		return 0;

	/* All drives have been timeout */
	if (timeout == lock->drive_num)
		return -ETIME;

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
		/*
		 * There have two reasons for the drive state is NOACCESS:
		 *
		 * the first reason is the I/O errors, even have retried at the
		 * beginning of this function, it fails to convert to a new mode
		 * thus it's not necessary to revert to old mode.
		 *
		 * the second reason is timeout, for this reason the IDM has
		 * been released.
		 *
		 * Thus skip to revert to old lock mode when drive state is
		 * NOACCESS.
		 */
		if (drive->state == ILM_DRIVE_NOACCESS) {
			score++;
			continue;
		}

		ret = _raid_convert_lock(lock, mode, host_id, drive);
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

static int _raid_renew_lock(struct ilm_lock *lock, char *host_id,
			    struct ilm_drive *drive)
{
	int ret;

	switch (drive->state) {
	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, lock->mode, host_id, drive);
		/* Fail to acquire a lock */
		if (ret)
			return ret;

		drive->state = ILM_DRIVE_ACCESSED;

		/* fall through */

	case ILM_DRIVE_ACCESSED:
	case ILM_DRIVE_FAILED:
		ret = idm_drive_renew_lock(lock->id, lock->mode, host_id,
					   drive->path);
		if (!ret) {
			drive->state = ILM_DRIVE_ACCESSED;
			return 0;
		}

		if (ret == -EIO) {
			drive->state = ILM_DRIVE_FAILED;
			return ret;
		}

		/*
		 * The lock mode cahed in lock manager is not same with the mode
		 * in the drive, it's possible that it's caused by the drive
		 * failure before and the lock manager missed to update the
		 * drive's lock mode.  For this case, release the idm and
		 * acquire it again to get a clean context for the idm.
		 */
		if (ret == -EFAULT) {
			idm_drive_unlock(lock->id, host_id, drive->path);
			drive->state = ILM_DRIVE_NOACCESS;

			ret = idm_drive_lock(lock->id, lock->mode, host_id,
					     drive->path, lock->timeout);
			if (!ret)
				drive->state = ILM_DRIVE_ACCESSED;
		}

		if (ret == -ETIME) {
			idm_drive_unlock(lock->id, host_id, drive->path);
			drive->state = ILM_DRIVE_NOACCESS;
		}

		ilm_log_err("%s: fail to renew lock: ret=%d\n", __func__, ret);
		break;
	default:
		ilm_log_err("%s: unexpected drive state %d\n",
			    __func__, drive->state);
		break;
	}

	return -1;
}

int idm_raid_renew_lock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;

	ilm_raid_lock_dump("Enter raid_renew_lock", lock);

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			ret = _raid_renew_lock(lock, host_id, drive);
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

static int _raid_write_lvb(struct ilm_lock *lock, char *host_id,
			   char *lvb, int lvb_size,
			   struct ilm_drive *drive)
{
	int ret;

	switch (drive->state) {
	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, lock->mode, host_id, drive);
		if (ret)
			return ret;

		drive->state = ILM_DRIVE_ACCESSED;

		/* fall through */

	case ILM_DRIVE_ACCESSED:
	case ILM_DRIVE_FAILED:
		ret = idm_drive_write_lvb(lock->id, host_id,
					  lvb, lvb_size, drive->path);
		if (!ret) {
			drive->state = ILM_DRIVE_ACCESSED;
			return 0;
		}

		if (ret == -EIO) {
			drive->state = ILM_DRIVE_FAILED;
			return ret;
		}

		if (ret == -ETIME) {
			idm_drive_unlock(lock->id, host_id, drive->path);
			drive->state = ILM_DRIVE_NOACCESS;
		}

		ilm_log_err("%s: fail to write lvb: ret=%d\n", __func__, ret);
		break;
	default:
		break;
	};

	return ret;
}

int idm_raid_write_lvb(struct ilm_lock *lock, char *host_id,
		       char *lvb, int lvb_size)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;

	ilm_raid_lock_dump("Enter raid_write_lvb", lock);

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			ret = _raid_write_lvb(lock, host_id, lvb, lvb_size,
					      drive);
			if (!ret)
				score++;
		}

		if (score >= ((lock->drive_num >> 1) + 1)) {
			ilm_raid_lock_dump("Exit raid_write_lvb", lock);
			return 0;
		}
	} while (ilm_curr_time() < timeout);

	/* Rollback to the old lvb */
	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];
		_raid_write_lvb(lock, host_id, lock->vb, lvb_size, drive);
	}

	ilm_raid_lock_dump("Rollback raid_write_lvb", lock);

	/* Timeout, return failure */
	return -1;
}

static int _raid_read_lvb(struct ilm_lock *lock, char *host_id,
			  char *lvb, int lvb_size,
			  struct ilm_drive *drive)
{
	int ret;

	switch (drive->state) {
	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, lock->mode, host_id, drive);
		if (ret)
			return ret;

		drive->state = ILM_DRIVE_ACCESSED;

		/* fall through */

	case ILM_DRIVE_ACCESSED:
	case ILM_DRIVE_FAILED:
		ret = idm_drive_read_lvb(lock->id, host_id,
					 lvb, lvb_size, drive->path);
		if (!ret) {
			drive->state = ILM_DRIVE_ACCESSED;
			return 0;
		}

		if (ret == -EIO) {
			drive->state = ILM_DRIVE_FAILED;
			return ret;
		}

		if (ret == -ETIME) {
			idm_drive_unlock(lock->id, host_id, drive->path);
			drive->state = ILM_DRIVE_NOACCESS;
		}

		ilm_log_err("%s: fail to write lvb: ret=%d\n", __func__, ret);
		break;
	default:
		break;
	};

	return ret;
}

int idm_raid_read_lvb(struct ilm_lock *lock, char *host_id,
		      char *lvb, int lvb_size)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;

	ilm_raid_lock_dump("Enter raid_read_lvb", lock);

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			ret = _raid_read_lvb(lock, host_id, lvb, lvb_size,
					     drive);
			if (!ret)
				score++;
		}

		if (score >= ((lock->drive_num >> 1) + 1)) {
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
	int stat[3] = { 0 }, stat_max = 0;
	struct ilm_drive *drive;

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		if (drive->state != ILM_DRIVE_ACCESSED)
			continue;

		ret = idm_drive_lock_count(lock->id, &cnt, drive->path);
		if (ret)
			continue;

		if (cnt >= 0 && cnt < 3)
			stat[cnt]++;
		else if (cnt >= 3)
			stat[2]++;
		else
			ilm_log_warn("wrong idm count %d\n", cnt);
	}

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
	int stat_mode[3] = { 0 }, mode_max = 0;
	struct ilm_drive *drive;

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];

		if (drive->state != ILM_DRIVE_ACCESSED)
			continue;

		ret = idm_drive_lock_mode(lock->id, &m, drive->path);
		if (ret)
			continue;

		if (m < 3 && m >= 0)
			stat_mode[m]++;
		else if (m >= 3)
			stat_mode[2]++;
		else
			ilm_log_warn("wrong idm mode %d\n", m);
	}

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
