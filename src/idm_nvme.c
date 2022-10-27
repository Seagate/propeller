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
 * gen_nvme_cmd_identify - Setup the NVMe ADmin command struct for Identify Controller (opcode=0x6)
 * @cmd_admin:          NVMe Admin Command struct to fill
 * @data_identify_ctrl: NVMe Admin Commmand data struct.  This is the cmd output destination.
 *
 */
void gen_nvme_cmd_identify(struct nvme_admin_cmd *cmd_admin, nvmeIDCtrl *data_identify_ctrl) {
//TODO: Should this return "void"???
    memset(cmd_admin,          0, sizeof(struct nvme_admin_cmd));
    memset(data_identify_ctrl, 0, sizeof(nvmeIDCtrl));

    cmd_admin->opcode     = NVME_ADMIN_CMD_IDENTIFY;
    cmd_admin->addr       = C_CAST(uint64_t, C_CAST(uintptr_t, data_identify_ctrl));
    cmd_admin->data_len   = NVME_IDENTIFY_DATA_LEN;
    cmd_admin->cdw10      = NVME_IDENTIFY_CTRL;         // Set CNS
    cmd_admin->timeout_ms = ADMIN_CMD_TIMEOUT_DEFAULT;
}

/**
 * gen_nvme_cmd_idm_read - Setup the NVMe Vendor-Specific command struct for
 *                         a IDM read (opcode=0xC2)
 * @cmd_idm_read:   The NVMe command struct to fill
 * @data_idm_read:  The NVMe commmand data struct.  This is the cmd output destination.
 * @idm_opcode:     The specific read-related operation for the IDM firmware to execute.
 * @idm_group:
 *
 */
void gen_nvme_cmd_idm_read(nvmeIdmVendorCmd *cmd_idm_read,
                           idmData *data_idm_read,
                           uint8_t idm_opcode,
                           uint8_t idm_group) {

    memset(cmd_idm_read,  0, sizeof(nvmeIdmVendorCmd));
    memset(data_idm_read, 0, sizeof(idmData));

    cmd_idm_read->opcode             = NVME_IDM_VENDOR_CMD_OP_READ;
    cmd_idm_read->addr               = C_CAST(uint64_t, C_CAST(uintptr_t, data_idm_read));
    cmd_idm_read->data_len           = IDM_VENDOR_CMD_DATA_LEN_BYTES;
    cmd_idm_read->ndt                = IDM_VENDOR_CMD_DATA_LEN_DWORDS;
//TODO: Change spec so don't have to do this 4-bit shift
    cmd_idm_read->idm_opcode_bits7_4 = idm_opcode << 4;
    cmd_idm_read->idm_group          = idm_group;
    cmd_idm_read->timeout_ms         = ADMIN_CMD_TIMEOUT_DEFAULT;
}

/**
 * gen_nvme_cmd_idm_write - Setup the NVMe Vendor-Specific command struct for
 *                          a IDM write (opcode=0xC1)
 * @cmd_idm_write:  The NVMe command struct to fill
 * @data_idm_write: The NVMe commmand data struct.  This is the cmd output destination.
 * @idm_opcode:     The specific read-related operation for the IDM firmware to execute.
 * @idm_group:
 *
 */
void gen_nvme_cmd_idm_write(nvmeIdmVendorCmd *cmd_idm_write,
                            idmData *data_idm_write,
                            uint8_t idm_opcode,
                            uint8_t idm_group) {

    memset(cmd_idm_write,  0, sizeof(nvmeIdmVendorCmd));
    memset(data_idm_write, 0, sizeof(idmData));

    cmd_idm_write->opcode             = NVME_IDM_VENDOR_CMD_OP_WRITE;
    cmd_idm_write->addr               = C_CAST(uint64_t, C_CAST(uintptr_t, data_idm_write));
    cmd_idm_write->data_len           = IDM_VENDOR_CMD_DATA_LEN_BYTES;
    cmd_idm_write->ndt                = IDM_VENDOR_CMD_DATA_LEN_DWORDS;
//TODO: Change spec so don't have to do this 4-bit shift
    cmd_idm_write->idm_opcode_bits7_4 = idm_opcode << 4;
    cmd_idm_write->idm_group          = idm_group;
    cmd_idm_write->timeout_ms         = ADMIN_CMD_TIMEOUT_DEFAULT;
}

/**
 * nvme_admin_identify - Send NVMe Identify Controller command to specified device.
 * @drive:  Drive path name.
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_admin_identify(char *drive) {

    printf("%s: IN: drive=%s\n", __func__, drive);

    struct nvme_admin_cmd cmd_admin;
    nvmeIDCtrl data_identify_ctrl;
    int ret = SUCCESS;

//TODO: This should ALWAYS be 4096.  Make a compile check for this??
    printf("sizeof(nvmeIDCtrl) = %d\n", sizeof(nvmeIDCtrl));

    gen_nvme_cmd_identify(&cmd_admin, &data_identify_ctrl);

    ret = send_nvme_cmd_admin(drive, &cmd_admin);

//TODO: Keep this debug??
    printf("%s: data_identify_ctrl.subnqn = %s\n", __func__, data_identify_ctrl.subnqn);

    return ret;
}

/**
 * nvme_idm_read - Using the NVMe Vendor Command format, send a IDM-related read request to
 *                 the drive.
 * @drive:      Drive path name.
 * @idm_opcode: The specific read-related operation for the IDM firmware to execute.
 * @idm_group:
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_read(char *drive, uint8_t idm_opcode, uint8_t idm_group) {

    printf("%s: IN: drive=%s,opcode=%d,group=%d\n", __func__, drive, idm_opcode, idm_group);

//TODO: Structual problem
//          "Layers" above this one do NOT have access to:
//              any returned data
//              result info in CDW17 of cmd struct
//          Likely need to move these up a "layer"
//          Likely need to start combining passed in params into a single common struct
    nvmeIdmVendorCmd cmd_idm_read;
    idmData data_idm_read;
    int ret = SUCCESS;

    gen_nvme_cmd_idm_read(&cmd_idm_read, &data_idm_read, idm_opcode, idm_group);

    ret = send_nvme_cmd_idm(drive, &cmd_idm_read);

    printf("%s: data_idm_read.resource_id = %s\n", __func__, data_idm_read.resource_id);
    printf("%s: data_idm_read.state = %s\n"      , __func__, data_idm_read.state);

    return ret;
}

/**
 * nvme_idm_write - Using the NVMe Vendor Command format, send a IDM-related write request to
 *                  the drive.
 * @drive:      Drive path name.
 * @idm_opcode: The specific write-related operation for the IDM firmware to execute.
 * @idm_group:
 *
 * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
 */
int nvme_idm_write(char *drive, uint8_t idm_opcode, uint8_t idm_group) {

    printf("%s: IN: drive=%s,opcode=%d,group=%d\n", __func__, drive, idm_opcode, idm_group);

//TODO: Structual problem
//          "Layers" above this one do NOT have access to:
//              any returned data
//              result info in CDW17 of cmd struct
//          Likely need to move these up a "layer"
//          Likely need to start combining passed in params into a single common struct
    nvmeIdmVendorCmd cmd_idm_write;
    idmData data_idm_write;
    int ret = SUCCESS;

    gen_nvme_cmd_idm_write(&cmd_idm_write, &data_idm_write, idm_opcode, idm_group);

    ret = send_nvme_cmd_idm(drive, &cmd_idm_write);

    printf("%s: data_idm_write.resource_id = %s\n", __func__, data_idm_write.resource_id);
    printf("%s: data_idm_write.time_now = %s\n"   , __func__, data_idm_write.time_now);

    return ret;
}

/**
 * send_nvme_cmd_admin - Send NVMe Admin command to specified device.
 * @drive:      Drive path name.
 * @cmd_admin:  NVMe Admin Command struct
 *
  * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
*/
int send_nvme_cmd_admin(char *drive, struct nvme_admin_cmd *cmd_admin) {
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

    ret = ioctl(nvme_fd, NVME_IOCTL_ADMIN_CMD, cmd_admin);
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
    printf("%s: ioctl cmd_admin->result=%d\n", __func__, cmd_admin->result);

//Completion Queue Entry (CQE) SIDE-NOTE:
// CQE DW0[31:0]  == cmd_admin->result
// CQE DW3[31:17] == ret                //?? is "ret" just [24:17]??

out:
    close(nvme_fd);
    return ret;
}

/**
 * send_nvme_cmd_idm - Send NVMe Vendor-Specific command to specified device.
 * @drive:      Drive path name.
 * @cmd_nvme:   NVMe command struct directed at the IDM firmware.
 *
  * Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
*/
int send_nvme_cmd_idm(char *drive, nvmeIdmVendorCmd *cmd_nvme) {

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

    ret = ioctl(nvme_fd, NVME_IOCTL_IO_CMD, cmd_nvme);
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
    printf("%s: ioctl cmd_nvme->result=%d\n", __func__, cmd_nvme->result);

//Completion Queue Entry (CQE) SIDE-NOTE:
// CQE DW0[31:0]  == cmd_admin->result
// CQE DW3[31:17] == ret                //?? is "ret" just [24:17]??

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

    //cli usage: idm_nvme identify
    if(argc >= 2){
        if(strcmp(argv[1], "identify") == 0){
            nvme_admin_identify(drive);
        }
        else if(strcmp(argv[1], "read") == 0){
            nvme_idm_read(drive, 0x0, 0x1);
        }
        else if(strcmp(argv[1], "write") == 0){
            nvme_idm_write(drive, 0x0, 0x1);
        }

    }


    return 0;
}
