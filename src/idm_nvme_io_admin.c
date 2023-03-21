/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme.c - NVMe interface for In-drive Mutex (IDM)
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

//////////////////////////////////////////
// COMPILE FLAGS
//////////////////////////////////////////
//TODO: Keep this (and the corresponding #ifdef's)???
#define COMPILE_STANDALONE
#define MAIN_ACTIVATE


//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////
/**
 * nvme_admin_identify - Send NVMe Identify Controller command to specified
 * device.
 *
 * @drive:  Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_admin_identify(char *drive)
{
	printf("%s: IN: drive=%s\n", __func__, drive);

	struct nvme_admin_cmd cmd_admin;
	nvmeIDCtrl data_identify_ctrl;
	int ret;
//TODO: This should ALWAYS be 4096.  Make a compile check for this??
	printf("sizeof(nvmeIDCtrl) = %d\n", sizeof(nvmeIDCtrl));

	_gen_nvme_cmd_identify(&cmd_admin, &data_identify_ctrl);

	ret = _send_nvme_cmd_admin(drive, &cmd_admin);

//TODO: Keep this debug??
	printf("%s: data_identify_ctrl.subnqn = %s\n",
	       __func__, data_identify_ctrl.subnqn);

	return ret;
}

/**
 * gen_nvme_cmd_identify - Setup the NVMe ADmin command struct for Identify
 * Controller (opcode=0x6)
 *
 * @cmd_admin:          NVMe Admin Command struct to fill
 * @data_identify_ctrl: NVMe Admin Commmand data struct.
 *                      This is the cmd output destination.
 *
 */
void _gen_nvme_cmd_identify(struct nvme_admin_cmd *cmd_admin,
                            nvmeIDCtrl *data_identify_ctrl)
{
	memset(cmd_admin,          0, sizeof(struct nvme_admin_cmd));
	memset(data_identify_ctrl, 0, sizeof(nvmeIDCtrl));

	cmd_admin->opcode     = NVME_ADMIN_CMD_IDENTIFY;
	cmd_admin->addr       = (uint64_t)(uintptr_t)(data_identify_ctrl);
	cmd_admin->data_len   = NVME_IDENTIFY_DATA_LEN_BYTES;
	cmd_admin->cdw10      = NVME_IDENTIFY_CNS_CTRL;         // Set CNS
	cmd_admin->timeout_ms = ADMIN_CMD_TIMEOUT_MS_DEFAULT;
}

/**
 * send_nvme_cmd_admin - Send NVMe Admin command to specified device.
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
		#ifndef COMPILE_STANDALONE
		ilm_log_err("%s: error opening drive %s fd %d",
		            __func__, drive, nvme_fd);
		#else
		printf("%s: error opening drive %s fd %d\n",
		       __func__, drive, nvme_fd);
		#endif
		return nvme_fd;
	}

	ret = ioctl(nvme_fd, NVME_IOCTL_ADMIN_CMD, cmd_admin);
	if(ret) {
		#ifndef COMPILE_STANDALONE
		ilm_log_err("%s: ioctl failed: %d", __func__, ret);
		#else
		printf("%s: ioctl failed: %d\n", __func__, ret);
		#endif
		goto out;
	}

//TODO: Keep this debug??
	printf("%s: ioctl ret=%d\n", __func__, ret);
	printf("%s: ioctl cmd_admin->result=%d\n",
	       __func__, cmd_admin->result);

//Completion Queue Entry (CQE) SIDE-NOTE:
// CQE DW0[31:0]  == cmd_admin->result
// CQE DW3[31:17] == ret                //?? is "ret" just [24:17]??

out:
	close(nvme_fd);
	return ret;
}







#ifdef MAIN_ACTIVATE
/*#########################################################################################
########################### STAND-ALONE MAIN ##############################################
#########################################################################################*/
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1";

//To compile:
//gcc idm_nvme_io_admin.c -o idm_nvme_io_admin
int main(int argc, char *argv[])
{
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
			ret = nvme_admin_identify(drive);
			printf("%s exiting with %d\n", argv[1], ret);
		}

	}

	return 0;
}
#endif//MAIN_ACTIVATE
