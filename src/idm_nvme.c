/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme.c - NVMe interface for In-drive Mutex (IDM)
 */

//IDM-specific
#include <fcntl.h>
#include <linux/nvme_ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "idm_nvme.h"

#define SUCCESS 0;
#define FAILURE 1;  // Make -1??

//TODO: Keep this (and the corresponding #ifdef's)???
#define NVME_STANDALONE


/**
 * fill_cmd_identify - Setup the NVMe command struct Identify Controller (opcode=0x6)
 * @admin_cmd:		        NVMe Admin Command struct to fill
 * @data_identify_ctrl:		NVMe Admin Commmand data struct.  This is the cmd output destination.
 *
 */
void fill_cmd_identify(struct nvme_admin_cmd *admin_cmd, nvmeIDCtrl *data_identify_ctrl) {

    memset(admin_cmd,          0, sizeof(struct nvme_admin_cmd));
    memset(data_identify_ctrl, 0, sizeof(nvmeIDCtrl));

    admin_cmd->opcode     = NVME_ADMIN_CMD_IDENTIFY;
    admin_cmd->addr       = C_CAST(uint64_t, C_CAST(uintptr_t, data_identify_ctrl));
    admin_cmd->data_len   = NVME_IDENTIFY_DATA_LEN;
    admin_cmd->cdw10      = NVME_IDENTIFY_CTRL;         // Set CNS
    admin_cmd->timeout_ms = ADMIN_CMD_TIMEOUT_DEFAULT;
}

/**
 * nvme_identify - Send NVMe Identify Controller command to specified device.
 * @drive:		Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_identify(char *drive) {

    printf("%s: IN: drive=%s\n", __func__, drive);

    struct nvme_admin_cmd admin_cmd;
    nvmeIDCtrl data_identify_ctrl;
    int ret = SUCCESS;


//TODO: This should ALWAYS be 4096.  Make a compile check for this??
    printf("sizeof(nvmeIDCtrl) = %d\n", sizeof(nvmeIDCtrl));

    fill_cmd_identify(&admin_cmd, &data_identify_ctrl);

    ret = send_nvme_admin_cmd(drive, &admin_cmd);

    printf("%s: data_identify_ctrl.subnqn = %s\n", __func__, data_identify_ctrl.subnqn);

    return ret;
}

/**
 * send_nvme_admin_cmd - Send NVMe Admin command to specified device.
 * @drive:		Drive path name.
 * @admin_cmd:	NVMe Admin Command struct
 *
  * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
*/
int send_nvme_admin_cmd(char *drive, struct nvme_admin_cmd *admin_cmd) {

    int nvme_fd;
    int ret = SUCCESS;


	if ((nvme_fd = open(drive, O_RDWR | O_NONBLOCK)) < 0) {
        #ifndef NVME_STANDALONE
		ilm_log_err("%s: error opening drive %s fd %d",
			    __func__, drive, nvme_fd);
        #else
		printf("%s: error opening drive %s fd %d\n",
			    __func__, drive, nvme_fd);
        #endif
		return nvme_fd;
	}

    ret = ioctl(nvme_fd, NVME_IOCTL_ADMIN_CMD, admin_cmd);
    if(ret) {
        #ifndef NVME_STANDALONE
        ilm_log_err("%s: ioctl failed: %d", __func__, ret);
        #else
        printf("%s: ioctl failed: %d\n", __func__, ret);
        #endif
        goto out;
    }

//TODO: Keep this debug??
    printf("%s: ioctl ret=%d\n", __func__, ret);
    printf("%s: ioctl admin_cmd->result=%d\n", __func__, admin_cmd->result);

//SIDE-NOTE:
// dw0: admin_cmd->result
// dw3: ret << 17

out:
    close(nvme_fd);
    return ret;
}







/*#########################################################################################
########################### STAND-ALONE MAIN ##############################################
#########################################################################################*/
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1";

//To compile:
//gcc idm_nvme.c -o idm_nvme
int main(int argc, char *argv[])
{
    char *drive;

    if(argc >= 3){
        drive = argv[2];
    }
    else {
        drive = DRIVE_DEFAULT_DEVICE;
    }

    if(argc >= 2){
        if(strcmp(argv[1], "identify") == 0){
            //cli usage: idm_nvme identify
            nvme_identify(drive);
        }

    }


    return 0;
}
