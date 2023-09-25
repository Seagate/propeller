/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "ilm.h"

#include "idm_api.h"
#include "inject_fault.h"
#include "list.h"
#include "log.h"
#include "util.h"

#define IDM_STATE_INIT			0
#define IDM_STATE_RUN			1
#define IDM_STATE_TIMEOUT		2

struct idm_async_op {
	int fd;
	struct list_head list;
	int result;
	int mode;
	int count;
	int self;
	char vb[IDM_VALUE_LEN];
};

/**
 * struct idm_host - the host information which has acquired idm
 * @list:		list is added to idm's host_list.
 * @id:			host ID.
 * @countdown:		count down value for timeout.
 * @last_renew_time:	Last renewal's time.
 * @state:		host state (init, run, timeout).
 */
struct idm_host {
	struct list_head list;
	char id[IDM_HOST_ID_LEN];

	/* Timeout */
	uint64_t countdown;
	uint64_t last_renew_time;

	int state;
};

/**
 * struct idm_emulation - the idm emulation management structure
 * @list:		List is added to global's idm_list.
 * @user_count:		Track the user count.
 * @drive_path:		The targeted drive path name.
 * @id:			Lock ID.
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_list:		List head which is to track the associated hosts.
 * @vb:			Lock's value block.
 */
struct idm_emulation {
	struct list_head list;

	/* Mutex is used to protect the data structure */
	pthread_mutex_t mutex;

	/* Lock ID */
	char id[IDM_LOCK_ID_LEN];

	/* Drive path */
	char drive_path[PATH_MAX];

	int user_count;
	int mode;

	struct list_head host_list;

	char vb[IDM_VALUE_LEN];
};

static struct list_head idm_list = LIST_HEAD_INIT(idm_list);
static pthread_mutex_t idm_list_mutex = PTHREAD_MUTEX_INITIALIZER;

static int idm_async_index = 0;
static struct list_head idm_async_list = LIST_HEAD_INIT(idm_async_list);
static pthread_mutex_t idm_async_list_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct idm_emulation *_idm_get(char *lock_id, char *drive, int alloc)
{
        struct idm_emulation *pos, *idm = NULL;

	pthread_mutex_lock(&idm_list_mutex);

	/* Firstly search if has existed idm */
	list_for_each_entry(pos, &idm_list, list) {

		if (!memcmp(pos->id, lock_id, IDM_LOCK_ID_LEN) &&
		    !strcmp(pos->drive_path, drive)) {
			/* Find the matched idm */
			idm = pos;

			pthread_mutex_lock(&idm->mutex);
			idm->user_count++;
			pthread_mutex_unlock(&idm->mutex);

			goto out;
		}
	}

	if (!alloc)
		goto out;

	/* Failed to find idm; allocate a new one! */
	idm = malloc(sizeof(struct idm_emulation));
	if (!idm) {
		ilm_log_err("Fail to alloc idm\n");
		goto out;
	}

	memset(idm, 0, sizeof(struct idm_emulation));
	memcpy(idm->id, lock_id, IDM_LOCK_ID_LEN);
	strncpy(idm->drive_path, drive, PATH_MAX);
	pthread_mutex_init(&idm->mutex, NULL);
	INIT_LIST_HEAD(&idm->host_list);
	INIT_LIST_HEAD(&idm->list);

	pthread_mutex_lock(&idm->mutex);
	idm->mode = IDM_MODE_UNLOCK;
	idm->user_count++;
	pthread_mutex_unlock(&idm->mutex);

	/* Add a new idm into list */
	list_add(&idm->list, &idm_list);

out:
	pthread_mutex_unlock(&idm_list_mutex);
	return idm;
}

static int _idm_put(char *lock_id, char *drive, int is_free)
{
        struct idm_emulation *pos, *idm = NULL;
	int user_count, ret = 0;

	pthread_mutex_lock(&idm_list_mutex);

	list_for_each_entry(pos, &idm_list, list) {
		if (!memcmp(pos->id, lock_id, IDM_LOCK_ID_LEN) &&
		    !strcmp(pos->drive_path, drive)) {
			/* Find the matched idm */
			idm = pos;
			break;
		}
	}

	if (!idm) {
		ilm_log_err("%s: fail to find idm\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	pthread_mutex_lock(&idm->mutex);
	idm->user_count--;
	user_count = idm->user_count;
	pthread_mutex_unlock(&idm->mutex);

	if (is_free && !user_count) {
		list_del(&idm->list);
		free(idm);
	}

out:
	pthread_mutex_unlock(&idm_list_mutex);
	return ret;
}

static struct idm_emulation *idm_get_and_alloc(char *lock_id, char *drive)
{
	return _idm_get(lock_id, drive, 1);
}

static int idm_put_and_free(char *lock_id, char *drive)
{
	return _idm_put(lock_id, drive, 1);
}

static struct idm_emulation *idm_get(char *lock_id, char *drive)
{
	return _idm_get(lock_id, drive, 0);
}

static int idm_put(char *lock_id, char *drive)
{
	return _idm_put(lock_id, drive, 0);
}

static struct idm_host *_idm_host_get(struct idm_emulation *idm,
				      char *host_id,
				      uint64_t timeout,
				      int alloc)
{
        struct idm_host *pos, *host = NULL;

	pthread_mutex_lock(&idm->mutex);

	list_for_each_entry(pos, &idm->host_list, list) {
		if (!memcmp(pos->id, host_id, IDM_HOST_ID_LEN)) {
			host = pos;
			goto out;
		}
	}

	if (!alloc)
		goto out;

	host = malloc(sizeof(struct idm_host));
	if (!host) {
		ilm_log_err("Fail to alloc idm\n");
		goto out;
	}

	memset(host, 0, sizeof(struct idm_host));

	memcpy(host->id, host_id, IDM_HOST_ID_LEN);
	host->countdown = timeout;
	host->last_renew_time = ilm_curr_time();
	host->state = IDM_STATE_INIT;
	INIT_LIST_HEAD(&host->list);

	list_add(&host->list, &idm->host_list);

out:
	pthread_mutex_unlock(&idm->mutex);
	return host;
}

static int _idm_host_put(struct idm_emulation *idm, char *host_id, int is_free)
{
        struct idm_host *pos, *host = NULL;

	pthread_mutex_lock(&idm->mutex);

	list_for_each_entry(pos, &idm->host_list, list) {
		if (!memcmp(pos->id, host_id, IDM_HOST_ID_LEN)) {
			host = pos;
		}
	}

	if (is_free && host) {
		list_del(&host->list);
		free(host);
	}

	pthread_mutex_unlock(&idm->mutex);
	return 0;
}

static struct idm_host *idm_host_get_and_alloc(struct idm_emulation *idm,
					       char *host_id,
					       uint64_t timeout)
{
	return _idm_host_get(idm, host_id, timeout, 1);
}

static int idm_host_put_and_free(struct idm_emulation *idm, char *host_id)
{
	return _idm_host_put(idm, host_id, 1);
}

static struct idm_host *idm_host_get(struct idm_emulation *idm, char *host_id)
{
	return _idm_host_get(idm, host_id, 0, 0);
}

static int idm_host_put(struct idm_emulation *idm, char *host_id)
{
	return _idm_host_put(idm, host_id, 0);
}

static int idm_lock_mode_is_permitted(struct idm_emulation *idm, int mode)
{
	if (idm->mode == IDM_MODE_UNLOCK)
		return 1;

	if (idm->mode == IDM_MODE_SHAREABLE && mode == IDM_MODE_SHAREABLE)
		return 1;

	return 0;
}

static int idm_host_is_expired(struct idm_host *host)
{
	uint64_t now = ilm_curr_time();

	if (now > host->last_renew_time + host->countdown)
		return 1;

	return 0;
}

static int idm_host_count(struct idm_emulation *idm)
{
	struct idm_host *host;
	int count = 0;

	list_for_each_entry(host, &idm->host_list, list) {
		if (host->state == IDM_STATE_TIMEOUT ||
		    idm_host_is_expired(host))
			continue;

		count++;
	}

	return count;
}

static int idm_self_count(struct idm_emulation *idm, char *host_id)
{
	struct idm_host *host;

	assert(host_id);

	list_for_each_entry(host, &idm->host_list, list) {
		if (host->state == IDM_STATE_TIMEOUT ||
		    idm_host_is_expired(host))
			continue;

		if (!memcmp(host->id, host_id, IDM_HOST_ID_LEN))
			return 1;
	}

	return 0;
}

/**
 * idm_drive_version - Read out IDM spec version
 * @drive:		Drive path name.
 * @version_major:	Returned major version, using "major.minor" version format
 * @version_minor:	Returned minor version, using 'major.minor" version format
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_version(char *drive, uint8_t *version_major, uint8_t *version_minor)
{
	*version_major = MIN_IDM_SPEC_VERSION_MAJOR;
	*version_minor = MIN_IDM_SPEC_VERSION_MINOR;
	return 0;
}

/**
 * idm_drive_lock - acquire an IDM on a specified drive
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock(char *lock_id, int mode, char *host_id,
                   char *drive, uint64_t timeout)
{
	struct idm_emulation *idm;
	struct idm_host *host;
	int ret = 0;
	int alloc_idm = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (mode == IDM_MODE_UNLOCK)
		return -EINVAL;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm) {
		idm = idm_get_and_alloc(lock_id, drive);
		if (!idm)
			return -ENOMEM;
		alloc_idm = 1;
	} else {
		/*
		 * Let's firstly find if the idm and the host has been existed:
		 *
		 * If idm is existed and the corresponding host can be found on
		 * the idm's host list, this means the same host is trying to
		 * acquire the same idm twice; for this case, directly bail out
		 * with error -EAGAIN.
		 *
		 * If idm is existed but host is not existed, let it to continue
		 * and check mode permission in idm_lock_mode_is_permitted().
		 */
		host = idm_host_get(idm, host_id);
		/* Acquire the same idm twice? */
		if (host) {
			idm_host_put(idm, host_id);
			idm_put(lock_id, drive);
			return -EAGAIN;
		}
	}

	host = idm_host_get_and_alloc(idm, host_id, timeout);
	if (!host) {
		ret = -ENOMEM;
		goto fail_idm;
	}

	pthread_mutex_lock(&idm->mutex);

	ret = idm_lock_mode_is_permitted(idm, mode);
	if (!ret) {
		pthread_mutex_unlock(&idm->mutex);
		ret = -EBUSY;
		goto fail_host;
	}

	/* Update the lock mode */
	idm->mode = mode;

	/* Update the host state */
	host->state = IDM_STATE_RUN;
	host->last_renew_time = ilm_curr_time();

	pthread_mutex_unlock(&idm->mutex);

	idm_host_put(idm, host_id);
	idm_put(lock_id, drive);
	return 0;

fail_host:
	idm_host_put_and_free(idm, host_id);
fail_idm:
	if (alloc_idm)
		idm_put_and_free(lock_id, drive);
	else
		idm_put(lock_id, drive);
	return ret;
}

/**
 * idm_drive_lock_async - acquire an IDM on a specified drive with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @fd:			File descriptor (emulated with an index).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_async(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_lock(lock_id, mode, host_id, drive, timeout);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);

	return 0;
}

/**
 * idm_drive_unlock - release an IDM on a specified drive
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_unlock(char *lock_id, int mode, char *host_id,
		     char *lvb, int lvb_size, char *drive)
{
	struct idm_emulation *idm;
	struct idm_host *host, *pos;
	int hosts_count = 0;
	int ret = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	if (mode != IDM_MODE_EXCLUSIVE && mode != IDM_MODE_SHAREABLE)
		return -EINVAL;

	if (lvb_size > IDM_VALUE_LEN)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	if (idm->mode != mode)
		return -EINVAL;

	host = idm_host_get(idm, host_id);
	if (!host)
		return -EINVAL;

	pthread_mutex_lock(&idm->mutex);

	/*
	 * Even detects the host has expired, this is a good chance
	 * to release the resources associated with the expired host.
	 */
	if (idm_host_is_expired(host))
		ret = -ETIME;

	/* Calculate how many alive hosts */
	list_for_each_entry(pos, &idm->host_list, list) {
		hosts_count++;
	}

	if (hosts_count == 1)
		idm->mode = IDM_MODE_UNLOCK;

	if (lvb)
		memcpy(idm->vb, lvb, lvb_size);

	pthread_mutex_unlock(&idm->mutex);

	/* Remove host from IDM's host list  */
	idm_host_put_and_free(idm, host_id);

	/* Decrease the user count */
	idm_put(lock_id, drive);
	return ret;
}

/**
 * idm_drive_unlock_async - release an IDM on a specified drive with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_unlock_async(char *lock_id, int mode, char *host_id,
			   char *lvb, int lvb_size, char *drive, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_unlock(lock_id, mode, host_id,
					 lvb, lvb_size, drive);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);

	return 0;
}

/**
 * idm_drive_convert_lock - Convert the lock mode for an IDM
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_convert_lock(char *lock_id, int mode, char *host_id, char *drive)
{
	struct idm_emulation *idm;
	struct idm_host *host;
	int ret = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	host = idm_host_get(idm, host_id);
	if (!host) {
		idm_put(lock_id, drive);
		return -EINVAL;
	}

	pthread_mutex_lock(&idm->mutex);

	if (idm_host_is_expired(host)) {
		host->state = IDM_STATE_TIMEOUT;
		ret = -ETIME;
		goto out;
	}

	/*
	 * It's possible to convert to the same mode, e.g. after a drive
	 * has failed and recoveried back, it misses to change mode during
	 * the failure but receive a later request to covert lock mode.
	 */
	if (idm->mode == mode) {
		ilm_log_warn("%s: Lock mode conversion is same %d\n",
			     __func__, idm->mode);
		goto out;
	}

	/*
	 * Should never hit the condition that the mode is IDM_MODE_UNLOCK.
	 */
	if (idm->mode == IDM_MODE_UNLOCK) {
		ilm_log_err("%s: old mode %d new mode %d\n",
			    __func__, idm->mode, mode);
		ret = -EPERM;
		goto out;
	}

	/*
	 * If mulitple hosts use the same IDM with shareable mode, cannot
	 * convert to exclusive mode.
	 */
	if (idm->mode == IDM_MODE_SHAREABLE && mode == IDM_MODE_EXCLUSIVE) {
		if (idm_host_count(idm) > 1) {
			ilm_log_err("%s: old mode %d new mode %d\n",
				    __func__, idm->mode, mode);
			ret = -EPERM;
			goto out;
		}
	}

	/*
	 * Lock mode domotion is always allowed, and can convert from
	 * shareable mode to exclusive mode for single user case.
	 */
	idm->mode = mode;

out:
	pthread_mutex_unlock(&idm->mutex);
	idm_host_put(idm, host_id);
	idm_put(lock_id, drive);
	return ret;
}

/**
 * idm_drive_convert_lock_async - Convert the lock mode with async
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_convert_lock_async(char *lock_id, int mode, char *host_id,
				 char *drive, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_convert_lock(lock_id, mode, host_id, drive);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);

	return 0;
}

/**
 * idm_drive_renew_lock - Renew host's membership for an IDM
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_renew_lock(char *lock_id, int mode, char *host_id, char *drive)
{
	struct idm_emulation *idm;
	struct idm_host *host;
	int ret = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	host = idm_host_get(idm, host_id);
	if (!host) {
		idm_put(lock_id, drive);
		return -EINVAL;
	}

	pthread_mutex_lock(&idm->mutex);

	if (idm_host_is_expired(host)) {
		host->state = IDM_STATE_TIMEOUT;
		ret = -ETIME;
		goto out;
	}

	if (mode != idm->mode) {
		ret = -EFAULT;
		goto out;
	}

	host->last_renew_time = ilm_curr_time();

out:
	pthread_mutex_unlock(&idm->mutex);
	idm_host_put(idm, host_id);
	idm_put(lock_id, drive);
	return ret;
}

/**
 * idm_drive_renew_lock_async - Renew host's membership for an IDM
 * 				with async mode
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL, ETIME).
 */
int idm_drive_renew_lock_async(char *lock_id, int mode,
			       char *host_id, char *drive, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_renew_lock(lock_id, mode, host_id, drive);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);

	return 0;
}

/**
 * idm_drive_break_lock - Break an IDM if before other hosts have
 * acquired this IDM.  This function is to allow a host_id to take
 * over the ownership if other hosts of the IDM is timeout, or the
 * countdown value is -1UL.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_break_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout)
{
	struct idm_emulation *idm;
	struct idm_host *host, *next;
	int ret = 0, can_break;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	host = idm_host_get(idm, host_id);
	/*
	 * The host_id should not be the owner at this moment,
	 * though we can permit this situation to happen, so
	 * one host can break itself.  But this is _not_ the
	 * designed purpose for this function, thus return
	 * error.
	 */
	if (host) {
		ret = -EINVAL;
		goto fail_host;
	}

	pthread_mutex_lock(&idm->mutex);

	can_break = 1;
	list_for_each_entry_safe(host, next, &idm->host_list, list) {
		/* If the host is timeout, remove it from the host list. */
		if (host->state == IDM_STATE_TIMEOUT ||
		    idm_host_is_expired(host)) {
			list_del(&host->list);
			free(host);
			continue;
		}

		/*
		 * If the host is not timeout and countdown is not -1ULL,
		 * this host cannot be broken.  So set the flag for this.
		 *
		 * At this timeout, don't exit the loop and take chance to
		 * remove other hosts have been timeout.
		 */
		if (host->countdown != -1ULL)
			can_break = 0;
	}

	if (!can_break) {
		ret = -EBUSY;
		goto fail_break;
	}

	/* Remove the host with its countdown is -1ULL. */
	list_for_each_entry_safe(host, next, &idm->host_list, list) {
		if (host->countdown == -1ULL) {
			list_del(&host->list);
			free(host);
		}
	}

	/* Still have alive hosts, cannot break it */
	if (!list_empty(&idm->host_list)) {
		ret = -EBUSY;
		goto fail_break;
	}

	pthread_mutex_unlock(&idm->mutex);

	/* No other hosts, let's acquire it */
	host = idm_host_get_and_alloc(idm, host_id, timeout);
	if (!host) {
		ret = -ENOMEM;
		goto fail_host;
	}

	pthread_mutex_lock(&idm->mutex);

	/* Update the lock mode */
	idm->mode = mode;

	/* Update the host state */
	host->state = IDM_STATE_RUN;
	host->last_renew_time = ilm_curr_time();

fail_break:
	pthread_mutex_unlock(&idm->mutex);
fail_host:
	idm_put(lock_id, drive);
	return ret;
}

/**
 * idm_drive_break_lock_async - Break an IDM with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Lock mode (unlock, shareable, exclusive).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @timeout:		Timeout for membership (unit: millisecond).
 * @fd:			File descriptor (emulated in index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_break_lock_async(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_break_lock(lock_id, mode, host_id,
					     drive, timeout);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);

	return 0;
}

/**
 * idm_drive_write_lvb - Write value block which is associated to an IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_write_lvb(char *lock_id, char *host_id,
			char *lvb, int lvb_size, char *drive)
{
	struct idm_emulation *idm;
	struct idm_host *host;
	int ret = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !drive)
		return -EINVAL;

	if (lvb_size > IDM_VALUE_LEN)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	host = idm_host_get(idm, host_id);
	if (!host) {
		idm_put(lock_id, drive);
		return -EINVAL;
	}

	pthread_mutex_lock(&idm->mutex);

	if (idm_host_is_expired(host)) {
		host->state = IDM_STATE_TIMEOUT;
		ret = -ETIME;
		goto out;
	}

	/* The host is not running state? */
	if (host->state != IDM_STATE_RUN) {
		ret = -ETIME;
		goto out;
	}

	memcpy(idm->vb, lvb, lvb_size);

out:
	pthread_mutex_unlock(&idm->mutex);
	idm_host_put(idm, host_id);
	idm_put(lock_id, drive);
	return ret;

}

/**
 * idm_drive_write_lvb_async - Write value block with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated in index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_write_lvb_async(char *lock_id, char *host_id,
			      char *lvb, int lvb_size,
			      char *drive, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_write_lvb(lock_id, host_id,
					    lvb, lvb_size,
					    drive);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);
	return 0;
}

/**
 * idm_drive_read_lvb - Read value block which is associated to an IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_read_lvb(char *lock_id, char *host_id,
		       char *lvb, int lvb_size, char *drive)
{
	struct idm_emulation *idm;
	struct idm_host *host;
	int ret = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !drive)
		return -EINVAL;

	if (lvb_size > IDM_VALUE_LEN)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	host = idm_host_get(idm, host_id);
	if (!host) {
		idm_host_put(idm, host_id);
		return -EINVAL;
	}

	pthread_mutex_lock(&idm->mutex);

	if (idm_host_is_expired(host)) {
		host->state = IDM_STATE_TIMEOUT;
		ret = -ETIME;
		goto out;
	}

	/* The host is not running state? */
	if (host->state != IDM_STATE_RUN) {
		ret = -EINVAL;
		goto out;
	}

	memcpy(lvb, idm->vb, lvb_size);

out:
	pthread_mutex_unlock(&idm->mutex);
	idm_host_put(idm, host_id);
	idm_put(lock_id, drive);
	return ret;
}

/**
 * idm_drive_read_lvb_async - Read value block with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_read_lvb_async(char *lock_id, char *host_id, char *drive, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_read_lvb(lock_id, host_id,
					   async->vb, IDM_VALUE_LEN,
					   drive);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);
	return 0;
}

/**
 * idm_drive_read_lvb_async_result - Read the result for read_lvb operation with
 * 				     async mode.
 * @fd:			File descriptor (emulated with index).
 * @lvb:		Lock value block pointer.
 * @lvb_size:		Lock value block size.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_read_lvb_async_result(uint64_t handle, char *lvb, int lvb_size,
				    int *result)
{
	struct idm_async_op *async, *next;

	pthread_mutex_lock(&idm_async_list_mutex);

	list_for_each_entry_safe(async, next, &idm_async_list, list) {

		if (async->fd == (int)handle) {
			list_del(&async->list);
			*result = async->result;
			memcpy(lvb, async->vb, lvb_size);
			free(async);
			pthread_mutex_unlock(&idm_async_list_mutex);
			return 0;
		}
	}

	pthread_mutex_unlock(&idm_async_list_mutex);
	return -1;
}

/**
 * idm_drive_lock_count - Read the host count for an IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @count:		Returned count value's pointer.
 * @self:		Returned self count value's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_count(char *lock_id, char *host_id,
			 int *count, int *self, char *drive)
{
	struct idm_emulation *idm;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!count)
		return -EINVAL;

	if (!lock_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -ENOENT;

	pthread_mutex_lock(&idm->mutex);
	*count = idm_host_count(idm);
	*self = idm_self_count(idm, host_id);
	*count -= *self;
	pthread_mutex_unlock(&idm->mutex);
	idm_put(lock_id, drive);
	return 0;
}

/**
 * idm_drive_lock_count_async - Read the host count for an IDM with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (32 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_count_async(char *lock_id, char *host_id,
			       char *drive, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_lock_count(lock_id, host_id, &async->count,
					     &async->self, drive);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);

	return 0;
}

/**
 * idm_drive_lock_count_async_result - Read the result for host count.
 * @fd:			File descriptor (emulated with index).
 * @count:		Returned count value's pointer.
 * @self:		Returned self count value's pointer.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_count_async_result(uint64_t handle, int *count, int *self,
				      int *result)
{
	struct idm_async_op *async, *next;

	pthread_mutex_lock(&idm_async_list_mutex);

	list_for_each_entry_safe(async, next, &idm_async_list, list) {

		if (async->fd == (int)(handle)) {
			list_del(&async->list);
			*count = async->count;
			*self = async->self;
			*result = async->result;
			free(async);
			break;
		}
	}

	pthread_mutex_unlock(&idm_async_list_mutex);
	return 0;
}

/**
 * idm_drive_lock_mode - Read back an IDM's current mode.
 * @lock_id:		Lock ID (64 bytes).
 * @mode:		Returned mode's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_mode(char *lock_id, int *mode, char *drive)
{
	struct idm_emulation *idm;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!mode)
		return -EINVAL;

	if (!lock_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -ENOENT;

	pthread_mutex_lock(&idm->mutex);
	*mode = idm->mode;
	pthread_mutex_unlock(&idm->mutex);
	idm_put(lock_id, drive);
	return 0;
}

/**
 * idm_drive_lock_mode_async - Read an IDM's mode with async mode.
 * @lock_id:		Lock ID (64 bytes).
 * @drive:		Drive path name.
 * @fd:			File descriptor (emulated with index).
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_mode_async(char *lock_id, char *drive, uint64_t *handle)
{
	struct idm_async_op *async;

	async = malloc(sizeof(struct idm_async_op));

	async->result = idm_drive_lock_mode(lock_id, &async->mode, drive);

	pthread_mutex_lock(&idm_async_list_mutex);
	*handle = idm_async_index;
	async->fd = idm_async_index;
	idm_async_index++;
	list_add_tail(&async->list, &idm_async_list);
	pthread_mutex_unlock(&idm_async_list_mutex);

	return 0;
}

/**
 * idm_drive_lock_mode_async_result - Read the result for lock mode.
 * @fd:			File descriptor (emulated with index).
 * @mode:		Returned mode's pointer.
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_lock_mode_async_result(uint64_t handle, int *mode, int *result)
{
	struct idm_async_op *async, *next;

	pthread_mutex_lock(&idm_async_list_mutex);

	list_for_each_entry_safe(async, next, &idm_async_list, list) {

		if (async->fd == (int)handle) {
			list_del(&async->list);
			*mode = async->mode;
			*result = async->result;
			free(async);
			pthread_mutex_unlock(&idm_async_list_mutex);
			return 0;
		}
	}

	pthread_mutex_unlock(&idm_async_list_mutex);
	return 0;
}

/**
 * idm_drive_async_result - Read the result for normal operations.
 * @fd:			File descriptor (emulated with index).
 * @result:		Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_async_result(uint64_t handle, int *result)
{
	struct idm_async_op *async, *next;

	ilm_log_dbg("%s: fd=%d", __func__, (int)(handle));

	pthread_mutex_lock(&idm_async_list_mutex);

	list_for_each_entry_safe(async, next, &idm_async_list, list) {

		ilm_log_dbg("%s: list fd=%d", __func__, async->fd);

		if (async->fd == (int)(handle)) {
			list_del(&async->list);
			*result = async->result;
			free(async);
			pthread_mutex_unlock(&idm_async_list_mutex);
			ilm_log_dbg("%s: foudn async result", __func__);
			return 0;
		}
	}

	pthread_mutex_unlock(&idm_async_list_mutex);
	return -1;
}

/**
 * idm_drive_host_state - Read back the host's state for an specific IDM.
 * @lock_id:		Lock ID (64 bytes).
 * @host_id:		Host ID (64 bytes).
 * @host_state:		Returned host state's pointer.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_host_state(char *lock_id,
			 char *host_id,
			 int *host_state,
			 char *drive)
{
	struct idm_emulation *idm;
	struct idm_host *host;

	if (!lock_id || !host_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	host = idm_host_get(idm, host_id);
	if (!host) {
		idm_put(lock_id, drive);
		return -EINVAL;
	}

	pthread_mutex_lock(&idm->mutex);
	*host_state = host->state;
	pthread_mutex_unlock(&idm->mutex);
	idm_host_put(idm, host_id);
	idm_put(lock_id, drive);
	return 0;
}

/**
 * idm_drive_whitelist - Read back hosts list for an specific drive.
 * @drive:		Drive path name.
 * @whitelist:		Returned pointer for host's white list.
 * @whitelist:		Returned pointer for host num.
 *
 * Returns zero or a negative error (ie. ENOMEM).
 */
int idm_drive_whitelist(char *drive, char **whitelist, int *whitelist_num)
{
	char *wl;
	struct idm_emulation *idm;
	struct idm_host *host;
	int i = 0, j, max_idx, max_alloc = 100;
	int ret;
	int matched;

	/* Let's firstly assume to allocet for 100 hosts */
	wl = malloc(IDM_HOST_ID_LEN * max_alloc);
	if (!wl)
		return -ENOMEM;

	pthread_mutex_lock(&idm_list_mutex);

	/* Iterate the global idm list */
	list_for_each_entry(idm, &idm_list, list) {
		/* Skip if not the required drive */
		if (strcmp(idm->drive_path, drive))
			continue;

		pthread_mutex_lock(&idm->mutex);

		/* Iterate every idm for its granted hosts */
		list_for_each_entry(host, &idm->host_list, list) {
			if (i >= max_alloc) {
				max_alloc += 100;

				wl = realloc(wl, IDM_HOST_ID_LEN * max_alloc);
				if (!wl) {
					ret = -ENOMEM;
					break;
				}
			}

			/* Copy host ID */
			memcpy(wl + i * IDM_HOST_ID_LEN, host->id,
			       IDM_HOST_ID_LEN);
			i++;
		}

		pthread_mutex_unlock(&idm->mutex);

		if (ret != 0)
			break;
	}

	pthread_mutex_unlock(&idm_list_mutex);

	/* Failed to alloc memory, directly bail out */
	if (ret != 0) {
		free(wl);
		return ret;
	}

	/* Remove the duplicated hosts from the array */
	max_idx = i;
	for (i = 1; i < max_idx; i++) {
		matched = 0;
		for (j = 0; j < i; j++) {
			if (!memcmp(wl + i * IDM_HOST_ID_LEN,
				    wl + j * IDM_HOST_ID_LEN,
				    IDM_HOST_ID_LEN)) {
				matched = 1;
				break;
			}
		}

		if (!matched)
			continue;

		for (j = i+1; j < max_idx; j++) {
			memcpy(wl + (j-1) * IDM_HOST_ID_LEN,
			       wl + j * IDM_HOST_ID_LEN,
			       IDM_HOST_ID_LEN);
		}

		max_idx--;
	}

	*whitelist_num = max_idx;
	return 0;
}

/**
 * idm_drive_read_group - Read back mutex group for all IDM in the drives
 * @drive:		Drive path name.
 * @info_ptr:		Returned pointer for info list.
 * @info_num:		Returned pointer for info num.
 *
 * Returns zero or a negative error (ie. ENOMEM).
 */
int idm_drive_read_group(char *drive, struct idm_info **info_ptr, int *info_num)
{
	struct idm_info *info_list, *info;
	struct idm_emulation *idm;
	struct idm_host *host;
	int i = 0, max_alloc = 8;
	int ret;

	/* Let's firstly assume to allocet for 8 items */
	info_list = malloc(sizeof(struct idm_info) * max_alloc);
	if (!info_list)
		return -ENOMEM;

	pthread_mutex_lock(&idm_list_mutex);

	/* Iterate the global idm list */
	list_for_each_entry(idm, &idm_list, list) {
		/* Skip if not the required drive */
		if (strcmp(idm->drive_path, drive))
			continue;

		pthread_mutex_lock(&idm->mutex);

		/* Generate an item if without host */
		if (list_empty(&idm->host_list)) {
			info = info_list + i;

			/* Copy host ID */
			memcpy(info->id, idm->id, IDM_LOCK_ID_LEN);
			info->mode = idm->mode;

			memset(info->host_id, 0, IDM_HOST_ID_LEN);
			info->last_renew_time = 0;
			info->timeout = 1;
			i++;
			continue;
		}

		/* Iterate every idm for its granted hosts */
		list_for_each_entry(host, &idm->host_list, list) {
			if (i >= max_alloc) {
				max_alloc += 8;

				info_list = realloc(info_list,
						sizeof(struct idm_info) * max_alloc);
				if (!info_list) {
					ret = -ENOMEM;
					break;
				}
			}

			info = info_list + i;

			/* Copy host ID */
			memcpy(info->id, idm->id, IDM_LOCK_ID_LEN);
			info->mode = idm->mode;

			memcpy(info->host_id, host->id, IDM_HOST_ID_LEN);
			info->last_renew_time = host->last_renew_time;
			info->timeout = idm_host_is_expired(host);
			i++;
		}

		pthread_mutex_unlock(&idm->mutex);

		if (ret != 0)
			break;
	}

	pthread_mutex_unlock(&idm_list_mutex);

	/* Failed to alloc memory, directly bail out */
	if (ret != 0) {
		free(info_list);
		return ret;
	}

	*info_ptr = info_list;
	*info_num = i;
	return 0;
}

/**
 * idm_drive_destroy - Destroy an IDM and release all associated resource.
 * @lock_id:		Lock ID (64 bytes).
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL).
 */
int idm_drive_destroy(char *lock_id, char *drive)
{
	struct idm_emulation *idm;
	struct idm_host *host, *next;
	int ret = 0;

	if (ilm_inject_fault_is_hit())
		return -EIO;

	if (!lock_id || !drive)
		return -EINVAL;

	idm = idm_get(lock_id, drive);
	if (!idm)
		return -EINVAL;

	pthread_mutex_lock(&idm_list_mutex);
	pthread_mutex_lock(&idm->mutex);

	if (idm->user_count > 1) {
		ret = -EBUSY;
		goto fail;
	}

	list_for_each_entry_safe(host, next, &idm->host_list, list) {
		/* If the host is timeout, remove it from the host list. */
		if (host->state == IDM_STATE_TIMEOUT ||
		    idm_host_is_expired(host)) {
			list_del(&host->list);
			free(host);
			continue;
		}
	}

	/* Still have alive hosts, cannot break it */
	if (!list_empty(&idm->host_list)) {
		ret = -EBUSY;
		goto fail;
	}

	list_del(&idm->list);
	free(idm);

	pthread_mutex_unlock(&idm->mutex);
	pthread_mutex_unlock(&idm_list_mutex);
	return 0;

fail:
	pthread_mutex_unlock(&idm->mutex);
	pthread_mutex_unlock(&idm_list_mutex);
	idm_put(lock_id, drive);
	return ret;
}

int idm_drive_get_fd(uint64_t handle)
{
	return (int)handle;
}
