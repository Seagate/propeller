/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 */

#include <string.h>

#include "idm_api.h"
#include "idm_nvme_api.h"
#include "idm_scsi.h"
#include "log.h"

//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////

/**
 * idm_drive_version - Read out IDM version
 *
 * @version:	Lock mode (unlock, shareable, exclusive).
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_version(int *version, char *drive)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_read_version(version, drive);
	else
		ret = scsi_idm_read_version(version, drive);

	return ret;
}

/**
 * idm_drive_lock - acquire an IDM on a specified drive
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock(char *lock_id, int mode, char *host_id, char *drive,
		   uint64_t timeout)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_lock(lock_id, mode, host_id, drive,
					 timeout);
	else
		ret = scsi_idm_sync_lock(lock_id, mode, host_id, drive,
					 timeout);

	return ret;
}

/**
 * idm_drive_lock_async - acquire an IDM on a specified drive with async mode
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_async(char *lock_id, int mode, char *host_id, char *drive,
			 uint64_t timeout, uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_lock(lock_id, mode, host_id, drive,
					  timeout, handle);
	else
		ret = scsi_idm_async_lock(lock_id, mode, host_id, drive,
					  timeout, handle);

	return ret;
}

/**
 * idm_drive_unlock - release an IDM on a specified drive
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @lvb:	Lock value block pointer.
 * @lvb_size:	Lock value block size.
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_unlock(char *lock_id, int mode, char *host_id, char *lvb,
		     int lvb_size, char *drive)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_unlock(lock_id, mode, host_id, lvb,
					   lvb_size, drive);
	else
		ret = scsi_idm_sync_unlock(lock_id, mode, host_id, lvb,
					   lvb_size, drive);

	return ret;
}

/**
 * idm_drive_unlock_async - release an IDM on a specified drive with async mode
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @lvb:	Lock value block pointer.
 * @lvb_size:	Lock value block size.
 * @drive:	Drive path name.
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_unlock_async(char *lock_id, int mode, char *host_id, char *lvb,
			   int lvb_size, char *drive, uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_unlock(lock_id, mode, host_id, lvb,
					    lvb_size, drive, handle);
	else
		ret = scsi_idm_async_unlock(lock_id, mode, host_id, lvb,
					    lvb_size, drive, handle);

	return ret;
}

/**
 * idm_drive_convert_lock - Convert the lock mode for an IDM
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_convert_lock(char *lock_id, int mode, char *host_id, char *drive,
			   uint64_t timeout)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_lock_convert(lock_id, mode, host_id,
						 drive, timeout);
	else
		ret = scsi_idm_sync_lock_convert(lock_id, mode, host_id,
						 drive, timeout);

	return ret;
}

/**
 * idm_drive_convert_lock_async - Convert the lock mode with async
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_convert_lock_async(char *lock_id, int mode, char *host_id,
				 char *drive, uint64_t timeout,
				 uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_lock_convert(lock_id, mode, host_id,
						  drive, timeout, handle);
	else
		ret = scsi_idm_async_lock_convert(lock_id, mode, host_id,
						  drive, timeout, handle);

	return ret;
}

/**
 * idm_drive_renew_lock - Renew host's membership for an IDM
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_renew_lock(char *lock_id, int mode, char *host_id, char *drive,
			 uint64_t timeout)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_lock_renew(lock_id, mode, host_id, drive,
					       timeout);
	else
		ret = scsi_idm_sync_lock_renew(lock_id, mode, host_id, drive,
					       timeout);

	return ret;
}

/**
 * idm_drive_renew_lock_async - Renew host's membership for an IDM
 * with async mode
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_renew_lock_async(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_lock_renew(lock_id, mode, host_id, drive,
						timeout, handle);
	else
		ret = scsi_idm_async_lock_renew(lock_id, mode, host_id, drive,
						timeout, handle);

	return ret;
}

/**
 * idm_drive_break_lock - Break an IDM if before other hosts have
 * acquired this IDM.  This function is to allow a host_id to take
 * over the ownership if other hosts of the IDM is timeout, or the
 * countdown value is -1UL.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_break_lock(char *lock_id, int mode, char *host_id,
			 char *drive, uint64_t timeout)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_lock_break(lock_id, mode, host_id, drive,
					       timeout);
	else
		ret = scsi_idm_sync_lock_break(lock_id, mode, host_id, drive,
					       timeout);

	return ret;
}

/**
 * idm_drive_break_lock_async - Break an IDM with async mode.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Lock mode (unlock, shareable, exclusive).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @timeout:	Timeout for membership (unit: millisecond).
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_break_lock_async(char *lock_id, int mode, char *host_id,
			       char *drive, uint64_t timeout, uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_lock_break(lock_id, mode, host_id, drive,
						timeout, handle);
	else
		ret = scsi_idm_async_lock_break(lock_id, mode, host_id, drive,
						timeout, handle);

	return ret;
}

/**
 * idm_drive_write_lvb - Write value block which is associated to an IDM.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @host_id:	Host ID (32 bytes).
 * @lvb:	Lock value block pointer.
 * @lvb_size:	Lock value block size.
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_write_lvb(char *lock_id, char *host_id, char *lvb, int lvb_size,
			char *drive)
{
	/*
	NOT IMPLEMENTED
	From section 7.2 of the IDM Seagate/Linaro design doc:
	"This section considers to integrate Seagate IDM into DLM. Some
	filesystems (e.g. GFS2, OCFS2, etc) use DLM for their locking,
	thus an obvious benefit of integration Seagate IDM into DLM is
	that these components can apply the accelerated locking.

	DLM is based on networking protocol to implement locks and it
	requires to support six lock modes, and it allows to convert lock
	modes dynamically. Seagate IDM is implemented in drive, it would be
	difficult to apply DLM lock model in the firmware; another thing is
	we need to consider how to use the shared storage data to replace
	the DLM networking transaction. Considering the implementation
	complexity, it’s not preferred to integrate IDM into DLM."
	*/
	ilm_log_err("%s: NOT IMPLEMENTED!", __func__);
	return FAILURE;
}

/**
 * idm_drive_write_lvb_async - Read value block with async mode.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @host_id:	Host ID (32 bytes).
 * @lvb:	Lock value block pointer.
 * @lvb_size:	Lock value block size.
 * @drive:	Drive path name.
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_write_lvb_async(char *lock_id, char *host_id, char *lvb,
			      int lvb_size, char *drive, uint64_t *handle)
{
	/*
	NOT IMPLEMENTED
	From section 7.2 of the IDM Seagate/Linaro design doc:
	"This section considers to integrate Seagate IDM into DLM. Some
	filesystems (e.g. GFS2, OCFS2, etc) use DLM for their locking,
	thus an obvious benefit of integration Seagate IDM into DLM is
	that these components can apply the accelerated locking.

	DLM is based on networking protocol to implement locks and it
	requires to support six lock modes, and it allows to convert lock
	modes dynamically. Seagate IDM is implemented in drive, it would be
	difficult to apply DLM lock model in the firmware; another thing is
	we need to consider how to use the shared storage data to replace
	the DLM networking transaction. Considering the implementation
	complexity, it’s not preferred to integrate IDM into DLM."
	*/
	ilm_log_err("%s: NOT IMPLEMENTED!", __func__);
	return FAILURE;
}

/**
 * idm_drive_read_lvb - Read value block which is associated to an IDM.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @host_id:	Host ID (32 bytes).
 * @lvb:	Lock value block pointer.
 * @lvb_size:	Lock value block size.
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_read_lvb(char *lock_id, char *host_id, char *lvb, int lvb_size,
		       char *drive)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_read_lvb(lock_id, host_id, lvb, lvb_size,
					     drive);
	else
		ret = scsi_idm_sync_read_lvb(lock_id, host_id, lvb, lvb_size,
					     drive);

	return ret;
}

/**
 * idm_drive_read_lvb_async - Read value block with async mode.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_read_lvb_async(char *lock_id, char *host_id, char *drive,
			     uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_read_lvb(lock_id, host_id, drive, handle);
	else
		ret = scsi_idm_async_read_lvb(lock_id, host_id, drive, handle);

	return ret;
}

/**
 * idm_drive_read_lvb_async_result - Read the result for read_lvb operation with
 * async mode.
 *
 * @drive:	Drive path name.
 * @handle:	Handle for the previously sent device operation.
 * @lvb:	Lock value block pointer.
 * @lvb_size:	Lock value block size.
 * @result:	Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_read_lvb_async_result(char *drive, uint64_t handle, char *lvb,
				    int lvb_size, int *result)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_get_result_lvb(handle, lvb, lvb_size,
						    result);
	else
		ret = scsi_idm_async_get_result_lvb(handle, lvb, lvb_size,
						    result);

	return ret;
}

/**
 * idm_drive_lock_count - Read the host count for an IDM.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @host_id:	Host ID (32 bytes).
 * @count:	Returned count value's pointer.
 * @self:	Returned self count value's pointer.
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_count(char *lock_id, char *host_id, int *count, int *self,
			 char *drive)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_read_lock_count(lock_id, host_id, count,
						    self, drive);
	else
		ret = scsi_idm_sync_read_lock_count(lock_id, host_id, count,
						    self, drive);

	return ret;
}

/**
 * idm_drive_lock_count_async - Read the host count for an IDM with async mode.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @host_id:	Host ID (32 bytes).
 * @drive:	Drive path name.
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_count_async(char *lock_id, char *host_id, char *drive,
			       uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_read_lock_count(lock_id, host_id, drive,
						     handle);
	else
		ret = scsi_idm_async_read_lock_count(lock_id, host_id, drive,
						     handle);

	return ret;
}

/**
 * idm_drive_lock_count_async_result - Read the result for host count.
 *
 * @drive:	Drive path name.
 * @handle:	Handle for the previously sent device operation.
 * @count:	Returned count value's pointer.
 * @self:	Returned self count value's pointer.
 * @result:	Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_count_async_result(char *drive, uint64_t handle, int *count,
				      int *self, int *result)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_get_result_lock_count(handle, count,
							   self, result);
	else
		ret = scsi_idm_async_get_result_lock_count(handle, count,
							   self, result);

	return ret;
}

/**
 * idm_drive_lock_mode - Read back an IDM's current mode.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @mode:	Returned mode's pointer.
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_mode(char *lock_id, int *mode, char *drive)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_read_lock_mode(lock_id, mode, drive);
	else
		ret = scsi_idm_sync_read_lock_mode(lock_id, mode, drive);

	return ret;
}

/**
 * idm_drive_lock_mode_async - Read an IDM's mode with async mode.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @drive:	Drive path name.
 * @handle:	Returned request handle for device.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_mode_async(char *lock_id, char *drive, uint64_t *handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_read_lock_mode(lock_id, drive, handle);
	else
		ret = scsi_idm_async_read_lock_mode(lock_id, drive, handle);

	return ret;
}

/**
 * idm_drive_lock_mode_async_result - Read the result for lock mode.
 *
 * @drive:	Drive path name.
 * @handle:	Handle for the previously sent device operation.
 * @mode:	Returned mode's pointer.
 * @result:	Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_lock_mode_async_result(char *drive, uint64_t handle, int *mode,
				     int *result)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_get_result_lock_mode(handle, mode,
							  result);
	else
		ret = scsi_idm_async_get_result_lock_mode(handle, mode,
							  result);

	return ret;
}

/**
 * idm_drive_async_result - Read the result for normal operations.
 *
 * @drive:	Drive path name.
 * @handle:	Handle for the previously sent device operation.
 * @result:	Returned result for the operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_async_result(char *drive, uint64_t handle, int *result)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_async_get_result(handle, result);
	else
		ret = scsi_idm_async_get_result(handle, result);

	return ret;
}

/**
 * idm_drive_free_async_result - Free the async result
 *
 * @drive:	Drive path name.
 * @handle:	Handle for the previously sent device operation.
 *
 * No return value
 */
void idm_drive_free_async_result(char *drive, uint64_t handle)
{
	if (!strstr(drive, NVME_DEVICE_TAG))
		nvme_idm_async_free_result(handle);
	else
		scsi_idm_async_free_result(handle);
}

/**
 * idm_drive_host_state - Read back the host's state for an specific IDM.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @host_id:	Host ID (64 bytes).
 * @host_state:	Returned host state's pointer.
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_host_state(char *lock_id, char *host_id, int *host_state,
			 char *drive)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_read_host_state(lock_id, host_id,
						    host_state, drive);
	else
		ret = scsi_idm_sync_read_host_state(lock_id, host_id,
						    host_state, drive);

	return ret;
}

/**
 * idm_drive_whitelist - Read back hosts list for an specific drive.
 *
 * @drive:	Drive path name.
 * @whitelist:	Returned pointer for host's white list.
 * @whitelist:	Returned pointer for host num.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_whitelist(char *drive, char **whitelist, int *whitelist_num)
{
	ilm_log_err("%s: NOT IMPLEMENTED!", __func__);
	return FAILURE;
}

/**
 * idm_drive_read_group - Read back mutex group for all IDM in the drives
 *
 * @drive:	Drive path name.
 * @info_ptr:	Returned pointer for info list.
 * @info_num:	Returned pointer for info num.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_read_group(char *drive, struct idm_info **info_ptr, int *info_num)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_read_mutex_group(drive, info_ptr,
						     info_num);
	else
		ret = scsi_idm_sync_read_mutex_group(drive, info_ptr,
						     info_num);

	return ret;
}

/**
 * idm_drive_destroy - Destroy an IDM and release all associated resource.
 *
 * @lock_id:	Lock ID (64 bytes).
 * @drive:	Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_destroy(char *lock_id, int mode, char *host_id,
		      char *drive)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_sync_lock_destroy(lock_id, mode, host_id,
						 drive);
	else
		ret = nvme_idm_sync_lock_destroy(lock_id, mode, host_id,
						 drive);

	return ret;
}

/**
 * idm_drive_get_fd - Helper function to retrive the device file
 * descriptor from an existing device request handle.
 *
 * @drive:	Drive path name.
 * @handle:	Handle for the previously sent device operation.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int idm_drive_get_fd(char *drive, uint64_t handle)
{
	int ret;

	if (!strstr(drive, NVME_DEVICE_TAG))
		ret = nvme_idm_get_fd(handle);
	else
		ret = scsi_idm_get_fd(handle);

	return ret;
}


//TODO: Remove.  Temp use for standalone compile.
int main(){}
