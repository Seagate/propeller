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

static int _raid_lock(struct ilm_lock *lock, int mode, char *host_id,
		      struct ilm_drive *drive)
{
	int host_state, ret;

	ret = idm_drive_lock(lock->id, mode, host_id, drive->path,
			     lock->timeout);
	if (!ret) {
		return 0;
	/*
	 * The idm has been acquired successfully before, but for some reaons
	 * (e.g. the host lost connection with drive and reconnect again) the
	 * host tries to acquire it again and the host's membership isn't
	 * expired.  So it reports the error -EAGAIN.
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
		 * still take it can unlock successfully.
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
		if (score >= (lock->drive_num >> 1 + 1))
			return 0;

		/*
                 * Fail to achieve majority, release IDMs has been
		 * acquired; race for next round.
		 */
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			if (drive->state != ILM_DRIVE_ACCESSED)
				continue;

			ret = idm_drive_unlock(lock->id, host_id, drive->path);
			if (!ret)
				continue;

			drive->state = ILM_DRIVE_FAILED;
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

	/* Timeout, fail to acquire lock with majoirty */
	return -1;
}

int idm_raid_unlock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	int i;

	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];
		idm_drive_unlock(lock->id, host_id, drive->path);

		/* Always Make success to unlock */
		drive->state = ILM_DRIVE_NOACCESS;
	}

	/* Always success */
	return 0;
}

static int _raid_convert_lock(struct ilm_lock *lock, int mode,
			      char *host_id, struct ilm_drive *drive)
{
	int i, ret;

	switch (drive->state) {
	case ILM_DRIVE_ACCESSED:
		ret = idm_drive_convert_lock(lock->id, mode,
					     host_id, drive->path);
		if (!ret)
			return 0;

		if (ret == -EIO)
			drive->state = ILM_DRIVE_FAILED;
		break;
	/* Handle for failed drive */
	case ILM_DRIVE_FAILED:
		/* Try to release lock firstly */
		ret = idm_drive_unlock(lock->id, host_id, drive->path);
		if (ret && ret != -ETIME)
			return ret;

		drive->state = ILM_DRIVE_NOACCESS;

		/* fall through */

	/*
	 * The drive has not been acquired when the lock is granted,
	 * take this chance to extend majority.
	 */
	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, mode, host_id, drive);
		if (!ret)
			drive->state = ILM_DRIVE_ACCESSED;
		break;
	default:
		ilm_log_err("%s: unexpected drive state %d\n",
			    __func__, drive->state);
		break;
	}

	return ret;
}

int idm_raid_convert_lock(struct ilm_lock *lock, char *host_id, int mode)
{
	struct ilm_drive *drive;
	int i, score, ret;

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
	}

	/* Has achieved majoirty */
	if (score >= (lock->drive_num >> 1 + 1))
		return 0;

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
		ret = _raid_convert_lock(lock, mode, host_id, drive);
		if (!ret)
			score++;
	}

	/* Unfortunately, fail to revert to old mode. */
	if (score < (lock->drive_num >> 1 + 1)) {
		ilm_log_warn("Fail to revert lock mode, old %d vs new %d\n",
			     lock->mode, mode);
		ilm_log_warn("Defer to resolve this when renew the lock\n");
		lock->convert_failed = 1;
	}

	return -1;
}

static int _raid_renew_lock(struct ilm_lock *lock, char *host_id,
			    struct ilm_drive *drive)
{
	int ret;

	switch (drive->state) {
	case ILM_DRIVE_ACCESSED:
		ret = idm_drive_renew_lock(lock->id, lock->mode, host_id,
					   drive->path);
		if (!ret) {
			drive->state = ILM_DRIVE_ACCESSED;
		} else if (ret == -EIO) {
			drive->state = ILM_DRIVE_FAILED;
		} else if (ret == -EFAULT) {
			/* Try to release lock firstly */
			ret = idm_drive_unlock(lock->id, host_id, drive->path);
			if (ret && ret != -ETIME)
				return ret;

			drive->state = ILM_DRIVE_NOACCESS;

			ret = idm_drive_lock(lock->id, lock->mode, host_id,
					     drive->path, lock->timeout);
			if (!ret)
				drive->state = ILM_DRIVE_ACCESSED;
		} else {
			ilm_log_err("%s: fail to renew %d\n", __func__, ret);
		}
		break;
	/* Handle for failed drive */
	case ILM_DRIVE_FAILED:
		/* Try to release lock firstly */
		ret = idm_drive_unlock(lock->id, host_id, drive->path);
		if (ret && ret != -ETIME)
			return ret;

		drive->state = ILM_DRIVE_NOACCESS;

		/* fall through */

	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, lock->mode, host_id, drive);
		if (!ret)
			drive->state = ILM_DRIVE_ACCESSED;
		break;
	default:
		ilm_log_err("%s: unexpected drive state %d\n",
			    __func__, drive->state);
		break;
	}

	return ret;
}

int idm_raid_renew_lock(struct ilm_lock *lock, char *host_id)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;

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
		    (score >= (lock->drive_num >> 1)))
			return 0;

		/* Drives have odd number */
		if ((lock->drive_num & 1) &&
		    (score >= (lock->drive_num >> 1 + 1)))
			return 0;

	} while (ilm_curr_time() < timeout);

	/* Timeout, fail to acquire lock with majoirty */
	return -1;
}

static int _raid_write_lvb(struct ilm_lock *lock, char *host_id,
			   char *lvb, int lvb_size,
			   struct ilm_drive *drive)
{
	int ret;

	switch (drive->state) {
	case ILM_DRIVE_FAILED:
		/* Try to release lock firstly */
		ret = idm_drive_unlock(lock->id, host_id, drive->path);
		if (ret && ret != -ETIME)
			return ret;

		drive->state = ILM_DRIVE_NOACCESS;

		/* fall through */

	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, lock->mode, host_id, drive);
		if (!ret)
			drive->state = ILM_DRIVE_ACCESSED;
		break;
	case ILM_DRIVE_ACCESSED:
	default:
		break;
	};

	if (drive->state != ILM_DRIVE_ACCESSED)
		return ret;

	ret = idm_drive_write_lvb(lock->id, lvb, lvb_size, drive->path);
	if (ret)
		drive->state = ILM_DRIVE_FAILED;

	return ret;
}

int idm_raid_write_lvb(struct ilm_lock *lock, char *host_id,
		       char *lvb, int lvb_size)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			ret = _raid_write_lvb(lock, host_id, lvb, lvb_size,
					      drive);
			if (!ret)
				score++;
		}

		if (score >= (lock->drive_num >> 1 + 1))
			return 0;
	} while (ilm_curr_time() < timeout);

	/* Rollback to the old lvb */
	for (i = 0; i < lock->drive_num; i++) {
		drive = &lock->drive[i];
		_raid_write_lvb(lock, host_id, lock->vb, lvb_size, drive);
	}

	/* Timeout, return failure */
	return -1;
}

static int _raid_read_lvb(struct ilm_lock *lock, char *host_id,
			  char *lvb, int lvb_size,
			  struct ilm_drive *drive)
{
	int ret;

	switch (drive->state) {
	case ILM_DRIVE_FAILED:
		/* Try to release lock firstly */
		ret = idm_drive_unlock(lock->id, host_id, drive->path);
		if (ret && ret != -ETIME)
			return ret;

		drive->state = ILM_DRIVE_NOACCESS;

		/* fall through */

	case ILM_DRIVE_NOACCESS:
		ret = _raid_lock(lock, lock->mode, host_id, drive);
		if (!ret)
			drive->state = ILM_DRIVE_ACCESSED;
		break;
	case ILM_DRIVE_ACCESSED:
	default:
		break;
	};

	if (drive->state != ILM_DRIVE_ACCESSED)
		return ret;

	ret = idm_drive_read_lvb(lock->id, lvb, lvb_size, drive->path);
	if (ret)
		drive->state = ILM_DRIVE_FAILED;

	return ret;
}

int idm_raid_read_lvb(struct ilm_lock *lock, char *host_id,
		      char *lvb, int lvb_size)
{
	struct ilm_drive *drive;
	uint64_t timeout = ilm_curr_time() + ILM_MAJORITY_TIMEOUT;
	int score, i, ret;

	do {
		score = 0;
		for (i = 0; i < lock->drive_num; i++) {
			drive = &lock->drive[i];
			ret = _raid_read_lvb(lock, host_id, lvb, lvb_size,
					     drive);
			if (!ret)
				score++;
		}

		if (score >= (lock->drive_num >> 1 + 1))
			return 0;
	} while (ilm_curr_time() < timeout);

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

	if (stat[stat_max] >= (lock->drive_num >> 1 + 1)) {
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

	if (stat_mode[mode_max] >= (lock->drive_num >> 1 + 1)) {
		*mode = mode_max;
		return 0;
	}

	return -1;
}

/*
 * TODO: statistic for hosts state is not useful at current stage.
 * Let's implement later for function idm_raid_host_state().
 */
