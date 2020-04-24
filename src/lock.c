/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <uuid/uuid.h>
#include <unistd.h>

#include "client.h"
#include "cmd.h"
#include "idm_wrapper.h"
#include "list.h"
#include "lockspace.h"
#include "lock.h"
#include "log.h"
#include "raid_lock.h"
#include "util.h"

static int ilm_lock_payload_read(struct ilm_cmd *cmd,
				 struct ilm_lock_payload *payload)
{
	int ret;

	ret = recv(cmd->cl->fd, payload,
		   sizeof(struct ilm_lock_payload), MSG_WAITALL);
	if (!ret || (ret != sizeof(struct ilm_lock_payload))) {
		ilm_log_err("Client fd %d recv lock payload errno %d\n",
			    cmd->cl->fd, errno);
		return -EIO;
	}

	if (payload->magic != ILM_LOCK_MAGIC) {
	        ilm_log_err("Client fd %d ret %d magic %x vs %x\n",
			    cmd->cl->fd, ret, payload->magic, ILM_LOCK_MAGIC);
		return -EINVAL;
	}

	return ret;
}

static int ilm_sort_drives(struct ilm_lock *lock)
{
	int drive_num = lock->drive_num;
	int i, j, ret;
	char *tmp;
	uuid_t uuid;

	for (i = 1; i < drive_num; i++) {
		for (j = i; j > 0; j--) {
			ret = memcmp(&lock->drive[j].uuid,
				     &lock->drive[j - 1].uuid,
				     sizeof(uuid_t));

			if (ret == 0) {
				ilm_log_err("Two drives have same UUID?");
				ilm_log_err("drive path=%s",
					    lock->drive[j].path);
				ilm_log_array_err("drive UUID",
						  (char *)&lock->drive[j].uuid,
						  sizeof(uuid_t));
				ilm_log_err("drive path=%s",
					    lock->drive[j - 1].path);
				ilm_log_array_err("drive UUID",
						  (char *)&lock->drive[j - 1].uuid,
						  sizeof(uuid_t));
				return -1;
			}

			if (ret > 0)
				continue;

			/* Swap two drives */
			tmp = lock->drive[j].path;
			lock->drive[j].path = lock->drive[j - 1].path;
			lock->drive[j - 1].path = tmp;

			memcpy(&uuid, &lock->drive[j].uuid, sizeof(uuid_t));
			memcpy(&lock->drive[j].uuid, &lock->drive[j - 1].uuid,
			       sizeof(uuid_t));
			memcpy(&lock->drive[j - 1].uuid, &uuid, sizeof(uuid_t));
		}
	}

	for (i = 0; i < drive_num; i++) {
		ilm_log_dbg("Index %d drive path=%s", i, lock->drive[i].path);
		ilm_log_array_dbg("drive UUID",
				  (char *)&lock->drive[i].uuid, sizeof(uuid_t));
	}

	return 0;
}

static struct ilm_lock *ilm_alloc(struct ilm_cmd *cmd,
				  struct ilm_lockspace *ls,
				  int drive_num, int *pos)
{
	char path[PATH_MAX];
	struct ilm_lock *lock;
	int ret, i, copied;

	lock = malloc(sizeof(struct ilm_lock));
	if (!lock) {
	        ilm_log_err("No spare memory to allocate lock\n");
		return NULL;
	}
	memset(lock, 0, sizeof(struct ilm_lock));

	INIT_LIST_HEAD(&lock->list);
	pthread_mutex_init(&lock->mutex, NULL);

	for (i = 0, copied = 0; i < drive_num; i++, copied++) {
		ret = recv(cmd->cl->fd, &path, sizeof(path), MSG_WAITALL);
		if (ret <= 0) {
			ilm_log_err("Fail to read out drive path\n");
			goto drive_fail;
		}

		*pos += ret;

		lock->drive[i].path = strdup(path);
		if (!lock->drive[i].path) {
			ilm_log_err("Fail to copy drive path\n");
			goto drive_fail;
		}

		ret = ilm_read_blk_uuid(lock->drive[i].path,
					&lock->drive[i].uuid);
		if (ret) {
			ilm_log_err("Fail to read drive uuid\n");
			goto drive_fail;
		}

		lock->drive[i].index = i;
	}

	lock->drive_num = drive_num;

	ret = ilm_sort_drives(lock);
	if (ret < 0)
		goto drive_fail;

	ret = ilm_lockspace_add_lock(ls, lock);
	if (ret < 0)
		goto drive_fail;

	lock->raid_th = ls->raid_thd;
	return lock;

drive_fail:
	for (i = 0; i < copied; i++)
		free(lock->drive[i].path);
	free(lock);
	return NULL;
}

static int ilm_free(struct ilm_lockspace *ls, struct ilm_lock *lock)
{
	int ret;

	ret = ilm_lockspace_del_lock(ls, lock);
	if (ret < 0)
		return ret;

	free(lock);
	return 0;
}

static void ilm_lock_dump(char *str, struct ilm_lock *lock)
{
	int i;

	ilm_log_dbg("Lock context in %s", str);
	ilm_log_dbg("drive_num=%d", lock->drive_num);
	for (i = 0; i < lock->drive_num; i++)
		ilm_log_dbg("drive_path[%d]=%s", i, lock->drive[i].path);
	ilm_log_dbg("mode=%d", lock->mode);
	ilm_log_array_dbg("lock ID:", lock->id, IDM_LOCK_ID_LEN);
}

int ilm_lock_acquire(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	struct ilm_lock *lock;
	int ret, pos = 0;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto out;
	pos += ret;

	if (payload.drive_num > ILM_DRIVE_MAX_NUM) {
	        ilm_log_err("Drive list is out of scope: drive_num %d\n",
			    payload.drive_num);
		ret = -EINVAL;
		goto out;
	}

	if (payload.mode != IDM_MODE_EXCLUSIVE &&
	    payload.mode != IDM_MODE_SHAREABLE) {
	        ilm_log_err("Lock mode is not supported: %d\n", payload.mode);
		ret = -EINVAL;
		goto out;
	}

	ret = ilm_lockspace_find_lock(ls, payload.lock_id, NULL);
	if (!ret) {
		ilm_log_err("Has acquired the lock yet!\n");
		ilm_log_array_err("Lock ID:", payload.lock_id, IDM_LOCK_ID_LEN);
		ret = -EBUSY;
		goto out;
	}

	lock = ilm_alloc(cmd, ls, payload.drive_num, &pos);
	if (!lock) {
		ret = -ENOMEM;
		goto out;
	}

	pthread_mutex_lock(&lock->mutex);
	memcpy(lock->id, payload.lock_id, IDM_LOCK_ID_LEN);
	lock->mode = payload.mode;
	lock->timeout = payload.timeout;
	pthread_mutex_unlock(&lock->mutex);

	ilm_lock_dump("lock_acquire", lock);

	ret = idm_raid_lock(lock, ls->host_id);
	if (ret) {
	        ilm_log_err("Fail to acquire raid lock %d\n", ret);
		ilm_free(ls, lock);
	} else {
		pthread_mutex_lock(&lock->mutex);
		lock->last_renewal_success = ilm_curr_time();
		pthread_mutex_unlock(&lock->mutex);
	}

out:
	ilm_client_recv_all(cmd->cl, cmd->sock_msg_len, pos);
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lock_release(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	struct ilm_lock *lock;
	int ret;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto out;

	ret = ilm_lockspace_find_lock(ls, payload.lock_id, &lock);
	if (ret < 0) {
		ilm_log_err("%s: Don't find data for the lock ID!\n", __func__);
		ilm_log_array_err("Lock ID:", payload.lock_id, IDM_LOCK_ID_LEN);
		goto out;
	}

	ilm_lock_dump("lock_release", lock);

	ret = idm_raid_unlock(lock, ls->host_id);

	ilm_free(ls, lock);
out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lock_convert_mode(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	struct ilm_lock *lock;
	int ret;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto out;

	ret = ilm_lockspace_find_lock(ls, payload.lock_id, &lock);
	if (ret < 0) {
		ilm_log_err("%s: Don't find data for the lock ID!\n", __func__);
		ilm_log_array_err("Lock ID:", payload.lock_id, IDM_LOCK_ID_LEN);
		goto out;
	}

	ilm_lock_dump("lock_convert", lock);
	ilm_log_dbg("new mode %d", payload.mode);

	ret = idm_raid_convert_lock(lock, ls->host_id, payload.mode);
	if (ret)
	        ilm_log_err("Fail to convert raid lock %d mode %d vs %d\n",
			    ret, lock->mode, payload.mode);
	else {
		/* Update after convert mode successfully */
		pthread_mutex_lock(&lock->mutex);
		lock->mode = payload.mode;
		pthread_mutex_unlock(&lock->mutex);
	}

out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lock_vb_write(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	struct ilm_lock *lock;
	char buf[IDM_VALUE_LEN];
	int ret;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto out;

	ret = ilm_lockspace_find_lock(ls, payload.lock_id, &lock);
	if (ret < 0) {
		ilm_log_err("%s: Don't find data for the lock ID!\n", __func__);
		ilm_log_array_err("Lock ID:", payload.lock_id, IDM_LOCK_ID_LEN);
		goto out;
	}

	ret = recv(cmd->cl->fd, buf, IDM_VALUE_LEN, MSG_WAITALL);
	if (ret != ILM_LVB_SIZE) {
		ilm_log_err("Fail to receive LVB fd %d errno %d\n",
			    cmd->cl->fd, errno);
		ret = -EIO;
		goto out;
	}

	ilm_lock_dump("lock_vb_write", lock);
	ilm_log_array_dbg("value buffer:", buf, IDM_VALUE_LEN);

#if 0
	ret = idm_raid_write_lvb(lock, ls->host_id, buf, IDM_VALUE_LEN);
	if (ret) {
		ilm_log_err("Fail to write lvb %d\n", ret);
	} else {
		/* Update after convert mode successfully */
		pthread_mutex_lock(&lock->mutex);
		memcpy(lock->vb, buf, IDM_VALUE_LEN);
		pthread_mutex_unlock(&lock->mutex);
	}
#else
	/*
	 * Update the cached LVB, which will be deferred to write
	 * into IDM when unlock it.
	 */
	pthread_mutex_lock(&lock->mutex);
	memcpy(lock->vb, buf, IDM_VALUE_LEN);
	pthread_mutex_unlock(&lock->mutex);
	ret = 0;
#endif

out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lock_vb_read(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	struct ilm_lock *lock;
	char buf[IDM_VALUE_LEN];
	int ret;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto fail;

	ret = ilm_lockspace_find_lock(ls, payload.lock_id, &lock);
	if (ret < 0) {
		ilm_log_err("%s: Don't find data for the lock ID!\n", __func__);
		ilm_log_array_err("Lock ID:", payload.lock_id, IDM_LOCK_ID_LEN);
		goto fail;
	}

	ilm_lock_dump("lock_vb_read", lock);

	ret = idm_raid_read_lvb(lock, ls->host_id, buf, IDM_VALUE_LEN);
	if (ret) {
		ilm_log_err("Fail to read lvb %d\n", ret);
		goto fail;
	} else {
		/* Update the cached LVB */
		pthread_mutex_lock(&lock->mutex);
		memcpy(lock->vb, buf, IDM_VALUE_LEN);
		pthread_mutex_unlock(&lock->mutex);
	}

	ilm_log_array_dbg("value buffer:", buf, IDM_VALUE_LEN);

	ilm_send_result(cmd->cl->fd, 0, buf, IDM_VALUE_LEN);
	return 0;

fail:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lock_host_count(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	struct ilm_lock *lock = NULL;
	int count, allocated = 0, pos = 0, ret;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto out;
	pos += ret;

	if (payload.drive_num > ILM_DRIVE_MAX_NUM) {
	        ilm_log_err("Drive list is out of scope: drive_num %d\n",
			    payload.drive_num);
		ret = -EINVAL;
		goto out;
	}

	ret = ilm_lockspace_find_lock(ls, payload.lock_id, &lock);
	if (ret < 0) {
		ilm_log_warn("%s: Fail find lock!\n", __func__);
		ilm_log_array_warn("Lock ID:", payload.lock_id, IDM_LOCK_ID_LEN);
		lock = ilm_alloc(cmd, ls, payload.drive_num, &pos);
		if (!lock) {
			ret = -ENOMEM;
			goto out;
		}
		memcpy(lock->id, payload.lock_id, IDM_LOCK_ID_LEN);
		allocated = 1;
	}

	ilm_lock_dump("lock_host_count", lock);

	ret = idm_raid_count(lock, &count);
	if (ret) {
		ilm_log_err("Fail to read count %d\n", ret);
		goto out;
	}
	ilm_log_dbg("Lock host count %d\n", count);

out:
	if (allocated)
		ilm_free(ls, lock);

	ilm_client_recv_all(cmd->cl, cmd->sock_msg_len, pos);
	if (!ret)
		ilm_send_result(cmd->cl->fd, ret, (char *)&count, sizeof(count));
	else
		ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lock_mode(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	struct ilm_lock *lock = NULL;
	int mode, allocated = 0, pos = 0, ret;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto out;
	pos += ret;

	if (payload.drive_num > ILM_DRIVE_MAX_NUM) {
	        ilm_log_err("Drive list is out of scope: drive_num %d\n",
			    payload.drive_num);
		ret = -EINVAL;
		goto out;
	}

	ret = ilm_lockspace_find_lock(ls, payload.lock_id, &lock);
	if (ret < 0) {
		ilm_log_warn("%s: Fail find lock!\n", __func__);
		ilm_log_array_warn("Lock ID:", payload.lock_id, IDM_LOCK_ID_LEN);
		lock = ilm_alloc(cmd, ls, payload.drive_num, &pos);
		if (!lock) {
			ret = -ENOMEM;
			goto out;
		}
		memcpy(lock->id, payload.lock_id, IDM_LOCK_ID_LEN);
		allocated = 1;
	}
	ilm_lock_dump("lock_host_mode", lock);

	ret = idm_raid_mode(lock, &mode);
	if (ret) {
		ilm_log_err("Fail to read mode %d\n", ret);
		goto out;
	}
	ilm_log_dbg("Lock mode %d\n", mode);

out:
	if (allocated)
		ilm_free(ls, lock);

	ilm_client_recv_all(cmd->cl, cmd->sock_msg_len, pos);
	if (!ret)
		ilm_send_result(cmd->cl->fd, ret, (char *)&mode, sizeof(mode));
	else
		ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

int ilm_lock_terminate(struct ilm_lockspace *ls, struct ilm_lock *lock)
{
	idm_raid_unlock(lock, ls->host_id);
	free(lock);

	return 0;
}

int ilm_lock_version(struct ilm_cmd *cmd, struct ilm_lockspace *ls)
{
	struct ilm_lock_payload payload;
	char path[PATH_MAX];
	int version, pos = 0, ret;

	ret = ilm_lock_payload_read(cmd, &payload);
	if (ret < 0)
		goto out;
	pos += ret;

	if (payload.drive_num != 1) {
	        ilm_log_err("%s: only can read version from single drive %d\n",
			    __func__, payload.drive_num);
		ret = -EINVAL;
		goto out;
	}

	ret = recv(cmd->cl->fd, &path, sizeof(path), MSG_WAITALL);
	if (ret <= 0) {
		ilm_log_err("Fail to read out drive path\n");
		goto out;
	}
	pos += ret;

	ret = idm_drive_version(&version, path);
	if (ret <= 0)
		ilm_log_err("Fail to read out version\n");

out:
	ilm_client_recv_all(cmd->cl, cmd->sock_msg_len, pos);
	if (!ret)
		ilm_send_result(cmd->cl->fd, ret,
				(char *)&version, sizeof(version));
	else
		ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}
