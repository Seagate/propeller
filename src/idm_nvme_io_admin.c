/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_io_admin.c - NVMe Admin interface for In-drive Mutex (IDM)
 */

#include <fcntl.h>
#include <linux/nvme_ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "idm_cmd_common.h"
#include "idm_nvme_io_admin.h"
#include "log.h"

////////////////////////////////////////////////////////////////////////////////
// COMPILE FLAGS
////////////////////////////////////////////////////////////////////////////////

/* For using internal main() for stand-alone debug compilation.
Setup to be gcc-defined (-D) in make file */
#ifdef DBG__NVME_IO_ADMIN_MAIN_ENABLE
#define DBG__NVME_IO_ADMIN_MAIN_ENABLE 1
#else
#define DBG__NVME_IO_ADMIN_MAIN_ENABLE 0
#endif

////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/**
 * nvme_admin_identify - Send NVMe Identify Controller command to specified
 * device.
 *
 * @drive:  Drive path name.
 * @id_ctrl: Returned NVMe Identify struct
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_admin_identify(char *drive, struct nvme_id_ctrl *id_ctrl)
{
	struct nvme_admin_cmd cmd_admin;
	int ret;

	_gen_nvme_cmd_identify(&cmd_admin, id_ctrl);

	ret = _send_nvme_cmd_admin(drive, &cmd_admin);

	return ret;
}

/**
 * _gen_nvme_cmd_identify - Setup the NVMe ADmin command struct for Identify
 * Controller (opcode=0x6)
 *
 * @cmd_admin:          NVMe Admin Command struct to fill
 * @id_ctrl: NVMe Admin Commmand data struct.
 *                      This is the cmd output destination.
 *
 */
void _gen_nvme_cmd_identify(struct nvme_admin_cmd *cmd_admin,
                            struct nvme_id_ctrl *id_ctrl)
{
	memset(cmd_admin, 0, sizeof(struct nvme_admin_cmd));
	memset(id_ctrl,   0, sizeof(struct nvme_id_ctrl));

	cmd_admin->opcode     = NVME_ADMIN_CMD_IDENTIFY;
	cmd_admin->addr       = (uint64_t)(uintptr_t)(id_ctrl);
	cmd_admin->data_len   = NVME_IDENTIFY_DATA_LEN_BYTES;
	cmd_admin->cdw10      = NVME_IDENTIFY_CNS_CTRL;         // Set CNS
	cmd_admin->timeout_ms = ADMIN_CMD_TIMEOUT_MS_DEFAULT;
}

/**
 * _send_nvme_cmd_admin - Send NVMe Admin command to specified device.
 * @drive:      Drive path name.
 * @cmd_admin:  NVMe Admin Command struct
 *
  * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
*/
int _send_nvme_cmd_admin(char *drive, struct nvme_admin_cmd *cmd_admin)
{
	int nvme_fd;
	int ret;

	if ((nvme_fd = open(drive, O_RDWR | O_NONBLOCK)) < 0) {
		ilm_log_err("%s: error opening drive %s fd %d",
		            __func__, drive, nvme_fd);
		return nvme_fd;
	}

	ret = ioctl(nvme_fd, NVME_IOCTL_ADMIN_CMD, cmd_admin);
	if(ret) {
		ilm_log_err("%s: ioctl failed: %d(0x%X)", __func__, ret, ret);
		goto out;
	}

	ilm_log_dbg("%s: ioctl ret=%d\n", __func__, ret);
out:
	close(nvme_fd);
	return ret;
}



#if DBG__NVME_IO_ADMIN_MAIN_ENABLE
////////////////////////////////////////////////////////////////////////////////
// DEBUG MAIN
////////////////////////////////////////////////////////////////////////////////
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1";

//To compile:
//gcc idm_nvme_io_admin.c -o idm_nvme_io_admin
int main(int argc, char *argv[])
{
	struct nvme_id_ctrl id_ctrl;
	char *drive;
	int  ret = 0;

	if(argc >= 3){
		drive = argv[2];
	}
	else {
		drive = DRIVE_DEFAULT_DEVICE;
	}

	//cli usage: idm_nvme_io_admin identify
	if(argc >= 2){
		if(strcmp(argv[1], "identify") == 0){
			ret = nvme_admin_identify(drive, &id_ctrl);
			printf("%s exiting with %d\n", argv[1], ret);
			if (!ret) {
				printf("%s: id_ctrl.subnqn = %s\n",
				       __func__, id_ctrl.subnqn);
				printf("%s: id_ctrl.vs[1023] = %u\n",
				       __func__, id_ctrl.vs[1023]);
			}

		}

	}

	return 0;
}
#endif
