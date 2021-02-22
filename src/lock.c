/*
 * Copyright (C) 2020-2021 Seagate
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <uuid/uuid.h>
#include <unistd.h>

#include "client.h"
#include "cmd.h"
#include "drive.h"
#include "idm_wrapper.h"
#include "list.h"
#include "lockspace.h"
#include "lock.h"
#include "log.h"
#include "raid_lock.h"
#include "util.h"

struct ilm_drive_info {
	char *path;
	uuid_t uuid;
};

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

static int ilm_sort_drive_uuid(uuid_t *uuid, int drive_num)
{
	int i, j, ret;
	char uuid_str[37];	/* uuid string is 36 chars + '\0' */
	uuid_t uuid_tmp;

	for (i = 1; i < drive_num; i++) {
		for (j = i; j > 0; j--) {
			ret = memcmp(&uuid[j], &uuid[j - 1], sizeof(uuid_t));

			if (ret == 0) {
				ilm_log_err("Two drives have same UUID?");

				uuid_unparse(uuid[j], uuid_str);
				uuid_str[36] = '\0';
				ilm_log_err("drive UUID: %s", uuid_str);
				continue;
			}

			if (ret > 0)
				continue;

			/* Swap two uuids */
			memcpy(&uuid_tmp, &uuid[j], sizeof(uuid_t));
			memcpy(&uuid[j], &uuid[j - 1], sizeof(uuid_t));
			memcpy(&uuid[j - 1], &uuid_tmp, sizeof(uuid_t));
		}
	}

	return 0;
}

static int ilm_insert_drive_multi_paths(struct ilm_lock *lock,
					uuid_t *uuid,
					int uuid_num)
{
	int i, j;
	struct ilm_drive *drive;
	int found;
	char uuid_str[37];	/* uuid string is 36 chars + '\0' */

	for (i = 0; i < uuid_num; i++) {

		/* Check if the drive paths have been initialized yet */
		found = 0;
		for (j = 0; j < lock->good_drive_num; j++) {
			/* Find a matched drive */
			if (!memcmp(&lock->drive[j].uuid, &uuid[i],
				    sizeof(uuid_t))) {
				found = 1;
				break;
			}
		}

		/*
		 * The drive has been initialized in previous loop,
		 * continue to serve next uuid.
		 */
		if (found)
			continue;

		drive = &lock->drive[lock->good_drive_num];
		memcpy(&drive->uuid, &uuid[i], sizeof(uuid_t));
		drive->path_num = ilm_scsi_get_all_sgs(drive->uuid, drive->path,
						       IDM_DRIVE_PATH_NUM);
		if (drive->path_num) {
			drive->index = lock->good_drive_num;
			lock->good_drive_num++;
		} else {
			uuid_unparse(drive->uuid, uuid_str);
			uuid_str[36] = '\0';
			ilm_log_err("Drive with UUID %s failed to parse sgs",
				    uuid_str);
			lock->fail_drive_num++;
		}
	}

	ilm_log_dbg("Final info for drives:");
	ilm_log_dbg(" Good drive num=%d", lock->good_drive_num);
	ilm_log_dbg(" Fail drive num=%d", lock->fail_drive_num);

	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];
		uuid_unparse(drive->uuid, uuid_str);
		uuid_str[36] = '\0';
		ilm_log_dbg("Drive %d UUID: %s", i, uuid_str);
		for (j = 0; j < drive->path_num; j++)
			ilm_log_dbg("  Path [%d] is %s", j, drive->path[j]);
	}

	return 0;
}

static char *ilm_find_sg_path(char *path, uuid_t *id)
{
	char dev_path[PATH_MAX];
	char *tmp, *sg_path;
	int ret;

	tmp = ilm_scsi_convert_blk_name(path);
	if (!tmp) {
		ilm_log_err("Fail to convert block name %s", path);
		goto try_cached_dev_map;
	}

	sg_path = ilm_convert_sg(tmp);
	if (!sg_path) {
		ilm_log_err("Fail to get sg for %s", tmp);
		free(tmp);
		goto try_cached_dev_map;
	}

	snprintf(dev_path, sizeof(dev_path), "/dev/%s", tmp);
	free(tmp);

	ret = ilm_read_parttable_id(dev_path, id);
	if (ret) {
		ilm_log_err("Fail to read uuid for drive (%s %s)",
			    tmp, sg_path);
		free(sg_path);
		goto try_cached_dev_map;
	}

	/* Find sg path successfully */
	return sg_path;

try_cached_dev_map:
	return ilm_find_cached_device_mapping(path, id);
}

static struct ilm_lock *ilm_alloc(struct ilm_cmd *cmd,
				  struct ilm_lockspace *ls,
				  int drive_num, int *pos)
{
	char path[PATH_MAX];
	struct ilm_lock *lock;
	struct ilm_drive *drive;
	uuid_t *uuid;
	int ret, i, j, copied = 0, failed = 0;
	char *sg_path;
	uuid_t id;

	lock = malloc(sizeof(struct ilm_lock));
	if (!lock) {
	        ilm_log_err("No spare memory to allocate lock\n");
		return NULL;
	}
	memset(lock, 0, sizeof(struct ilm_lock));

	uuid = malloc(sizeof(uuid_t) * drive_num);
	if (!uuid) {
	        ilm_log_err("Failed to allocate uuid array, drive_num=%d\n",
			    drive_num);
		free(lock);
		return NULL;
	}

	INIT_LIST_HEAD(&lock->list);
	pthread_mutex_init(&lock->mutex, NULL);

	for (i = 0; i < drive_num; i++) {
		ret = recv(cmd->cl->fd, &path, sizeof(path), MSG_WAITALL);
		if (ret <= 0) {
			ilm_log_err("Fail to read out drive path\n");
			goto drive_fail;
		}

		*pos += ret;

		sg_path = ilm_find_sg_path(path, &id);
		if (!sg_path) {
			failed++;
			continue;
		}

		memcpy(&uuid[copied], &id, sizeof(uuid_t));
		copied++;

		ilm_add_cached_device_mapping(path, sg_path, &id);
	}

	lock->fail_drive_num = failed;

	ret = ilm_sort_drive_uuid(uuid, copied);
	if (ret < 0)
		goto drive_fail;

	ret = ilm_insert_drive_multi_paths(lock, uuid, copied);
	if (ret < 0)
		goto drive_fail;

	lock->total_drive_num = lock->good_drive_num + lock->fail_drive_num;

	/*
	 * If good drives is less than or equal the half of drives,
	 * it's no chance to achieve majority.  Directly return failure
	 * for this case.
	 */
	if (lock->good_drive_num <= (lock->total_drive_num >> 1))
		goto drive_fail;

	ret = ilm_lockspace_add_lock(ls, lock);
	if (ret < 0)
		goto drive_fail;

	lock->raid_th = ls->raid_thd;
	free(uuid);
	return lock;

drive_fail:
	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];
		for (j = 0; j < drive->path_num; j++)
			free(drive->path[j]);
	}

	for (i = 0; i < copied; i++)
		free(lock->drive[i].path);
	free(lock);
	free(uuid);
	return NULL;
}

static int ilm_free(struct ilm_lockspace *ls, struct ilm_lock *lock)
{
	struct ilm_drive *drive;
	int i, j;
	int ret;

	ret = ilm_lockspace_del_lock(ls, lock);
	if (ret < 0)
		return ret;

	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];
		for (j = 0; j < drive->path_num; j++)
			free(drive->path[j]);
	}

	free(lock);
	return 0;
}

static void ilm_lock_dump(const char *str, struct ilm_lock *lock)
{
	int i, j;
	struct ilm_drive *drive;
	char uuid_str[39];	/* uuid string is 39 chars + '\0' */

	ilm_log_err("<<<<< Lock dump: %s <<<<<", str);
	ilm_log_err("Drive number statistics: Total=%d Good=%d Fail=%d",
		    lock->total_drive_num, lock->good_drive_num,
		    lock->fail_drive_num);

	for (i = 0; i < lock->good_drive_num; i++) {
		drive = &lock->drive[i];
		uuid_unparse(drive->uuid, uuid_str);
		uuid_str[36] = '\0';
		ilm_log_dbg("Drive %d UUID: %s", i, uuid_str);
		for (j = 0; j < drive->path_num; j++)
			ilm_log_dbg("  Path [%d] is %s", j, drive->path[j]);
	}

	ilm_log_err("Lock mode=%d", lock->mode);

	ilm_log_array_err("Lock ID", lock->id, 64);

	ilm_id_write_format(lock->id, uuid_str, sizeof(uuid_str));
	if (strlen(uuid_str))
		ilm_log_err("lock ID (VG): %s", uuid_str);
	else
		ilm_log_err("lock ID (VG): Empty string");

	ilm_id_write_format(lock->id + 32, uuid_str, sizeof(uuid_str));
	if (strlen(uuid_str))
		ilm_log_err("lock ID (LV): %s", uuid_str);
	else
		ilm_log_err("lock ID (LV): Empty string");

	ilm_log_err(">>>>> Lock dump: %s >>>>>", str);
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

	if (!payload.drive_num || payload.drive_num > ILM_DRIVE_MAX_NUM) {
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
		goto out;
	}

	ilm_lockspace_start_lock(ls, lock, ilm_curr_time());
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

	ilm_lockspace_stop_lock(ls, lock, NULL);

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
	uint64_t time;
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

	/*
	 * IDM uses the same operation "refresh" for both converting lock mode
	 * and renewal lock, for this reason is might cause the interleave
	 * commands which is invoked by two threads (one thread is for
	 * converting mode and another thread is for renewal), so disable the
	 * lock renewal temporarily during converting the lock mode.
	 */
	ilm_lockspace_stop_lock(ls, lock, &time);

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

	/* Restart the lock renewal */
	ilm_lockspace_start_lock(ls, lock, time);
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
	int allocated = 0, pos = 0, ret;
	struct _account {
		int count;
		int self;
	} account;

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

	ret = idm_raid_count(lock, ls->host_id, &account.count, &account.self);
	if (ret) {
		ilm_log_err("Fail to read count %d\n", ret);
		goto out;
	}
	ilm_log_dbg("Lock host count %d self %d\n", account.count, account.self);

out:
	if (allocated)
		ilm_free(ls, lock);

	ilm_client_recv_all(cmd->cl, cmd->sock_msg_len, pos);
	if (!ret)
		ilm_send_result(cmd->cl->fd, ret,
				(char *)&account, sizeof(account));
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
	if (ret < 0)
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
