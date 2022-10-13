/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 *
 * idm_nvme_io.c - Contains the lowest-level function that allow the IDM In-drive Mutex (IDM)
 *                  to talk to the Linux kernel (via ioctl(), read() or write())
 */

#include <fcntl.h>
#include <linux/nvme_ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>       //TODO: Do I need BOTH ioctl includes?
#include <unistd.h>

#include "idm_nvme_io.h"


//////////////////////////////////////////
// COMPILE FLAGS
//////////////////////////////////////////
//TODO: Keep this (and the corresponding #ifdef's)???
#define COMPILE_STANDALONE
// #define MAIN_ACTIVATE


//////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////
int nvme_idm_write(nvmeIdmRequest *request_idm) {

    printf("%s: START\n", __func__);

    nvmeIdmVendorCmd *cmd_idm  = request_idm->cmd_idm;
    idmData *data_idm          = request_idm->data_idm;
    int ret                    = SUCCESS;

    ret = _nvme_idm_cmd_init_wrt(request_idm);
    //TODO: Error handling (HERE -OR- push DOWN 1 level??)

    ret = _nvme_idm_data_init_wrt(request_idm);
    //TODO: Error handling (HERE -OR- push DOWN 1 level??)

    ret = _nvme_send_cmd_idm(request_idm);
    //TODO: Error handling (HERE -OR- push DOWN 1 level??)

//TODO: what do with this debug code?
    printf("%s: data_idm_write.resource_id = %s\n", __func__, data_idm->resource_id);
    printf("%s: data_idm_write.time_now = %s\n"   , __func__, data_idm->time_now);

    return ret;
}

//TODO: Attempt at a "common" write initialization, with some caveats (of course).
//TODO: KEEP THIS?????
int nvme_idm_write_init(nvmeIdmRequest *request_idm, nvmeIdmVendorCmd *cmd_idm,
                         idmData *data_idm, char *lock_id, int mode, char *host_id,
                         char *drive, uint64_t timeout, char *lvb, int lvb_size) {

//TODO: Leave these in under DEBUG flag??
    printf("%s: START\n", __func__);

    int ret = SUCCESS;

    memset(request_idm, 0, sizeof(nvmeIdmRequest));
    memset(cmd_idm,     0, sizeof(nvmeIdmVendorCmd));
    memset(data_idm,    0, sizeof(idmData));

    request_idm->lock_id  = lock_id;
    request_idm->mode_idm = mode;
    request_idm->host_id  = host_id;
    request_idm->drive    = drive;

    request_idm->cmd_idm  = cmd_idm;
    request_idm->data_idm = data_idm;

//TODO: Command dependent variables: Leave here -OR- move up 1 level?
    //kludge for inconsistent input params
    if(timeout)
        request_idm->timeout  = timeout;
    if(lvb)
        request_idm->lvb = lvb;
    if(lvb_size)
        request_idm->lvb_size  = lvb_size;

    return ret;
}

int _nvme_idm_cmd_init(nvmeIdmRequest *request_idm, uint8_t opcode_nvme) {

    printf("%s: START\n", __func__);

    nvmeIdmVendorCmd *cmd_idm  = request_idm->cmd_idm;
    idmData *data_idm          = request_idm->data_idm;
    int ret                    = SUCCESS;

    cmd_idm->opcode_nvme        = opcode_nvme;
    cmd_idm->addr               = (uint64_t)(uintptr_t)data_idm;
    cmd_idm->data_len           = IDM_VENDOR_CMD_DATA_LEN_BYTES;  //Should be: sizeof(idmData) which should always be 512
    cmd_idm->ndt                = IDM_VENDOR_CMD_DATA_LEN_DWORDS;
//TODO: Change spec so don't have to do this 4-bit shift
    cmd_idm->opcode_idm_bits7_4 = request_idm->opcode_idm << 4;
    cmd_idm->group_idm          = request_idm->group_idm;       //TODO: This isn't yet getting set anywhere for lock
    cmd_idm->timeout_ms         = VENDOR_CMD_TIMEOUT_DEFAULT;  //TODO: ??  THis vs the timout param passed in??

    return ret;
}

//TODO: Bring this back in when doing "read" side.
// int _nvme_idm_cmd_init_rd(nvmeIdmRequest *request_idm) {
//     return _nvme_idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_READ);
// }

int _nvme_idm_cmd_init_wrt(nvmeIdmRequest *request_idm) {
    printf("%s: START\n", __func__);

    return _nvme_idm_cmd_init(request_idm, NVME_IDM_VENDOR_CMD_OP_WRITE);
}

int _nvme_idm_data_init_wrt(nvmeIdmRequest *request_idm) {

    printf("%s: START\n", __func__);

    nvmeIdmVendorCmd *cmd_idm = request_idm->cmd_idm;
    idmData *data_idm         = request_idm->data_idm;
    int ret                   = SUCCESS;

//TODO: ?? __bswap_64() the next 3 rhs values??
    #ifndef COMPILE_STANDALONE
  	data_idm->time_now  = ilm_read_utc_time();
    #else
  	data_idm->time_now  = 0;
    #endif//COMPILE_STANDALONE
	data_idm->countdown = request_idm->timeout;
	data_idm->class_idm = request_idm->class_idm;

//TODO: ?? reverse order of next 3 rhs arrays ??  (on scsi-side, using _scsi_data_swap())
    memcpy(data_idm->resource_id,  request_idm->lock_id, IDM_LOCK_ID_LEN_BYTES);
    memcpy(data_idm->host_id,      request_idm->host_id, IDM_HOST_ID_LEN_BYTES);
    memcpy(data_idm->resource_ver, request_idm->lvb,     request_idm->lvb_size);  //TOO: Not always needed.  Copy anyway?  Conditional IF?

	data_idm->resource_ver[0] = request_idm->res_ver_type;                     //TODO: What the heck is going on HERE?!?

    return ret;
}

int _nvme_send_cmd_idm(nvmeIdmRequest *request_idm) {

    printf("%s: START\n", __func__);

    int nvme_fd;
    int ret = SUCCESS;

    if ((nvme_fd = open(request_idm->drive, O_RDWR | O_NONBLOCK)) < 0) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: error opening drive %s fd %d",
                __func__, request_idm->drive, nvme_fd);
        #else
        printf("%s: error opening drive %s fd %d\n",
                __func__, request_idm->drive, nvme_fd);
        #endif//COMPILE_STANDALONE
        return nvme_fd;
    }

    ret = ioctl(nvme_fd, NVME_IOCTL_IO_CMD, request_idm->cmd_idm);
    if(ret) {
        #ifndef COMPILE_STANDALONE
        ilm_log_err("%s: ioctl failed: %d", __func__, ret);
        #else
        printf("%s: ioctl failed: %d\n", __func__, ret);
        #endif//COMPILE_STANDALONE
        goto out;
    }

//TODO: Keep this debug??
    printf("%s: ioctl ret=%d\n", __func__, ret);
    printf("%s: ioctl cmd_idm->result=%d\n", __func__, request_idm->cmd_idm->result);

//Completion Queue Entry (CQE) SIDE-NOTE:
// CQE DW0[31:0]  == cmd_admin->result
// CQE DW3[31:17] == ret                //?? is "ret" just [24:17]??

out:
//TODO: Possible ASYNC flag HERE??  -OR-  do I need to use write() and read() for async?? (like scsi)
    // if(async) {
    //     request_idm->fd = nvme_fd;  //async, so save nvme_fd for later
    // }
    // else {
    //     close(nvme_fd);              //sunc, so done with nvme_fd.
    // }
    close(nvme_fd);
    return ret;
}



#ifdef MAIN_ACTIVATE
/*#########################################################################################
########################### STAND-ALONE MAIN ##############################################
#########################################################################################*/
#define DRIVE_DEFAULT_DEVICE "/dev/nvme0n1";

//To compile:
//gcc idm_nvme_io.c -o idm_nvme_io
int main(int argc, char *argv[])
{
    char *drive;

    if(argc >= 3){
        drive = argv[2];
    }
    else {
        drive = DRIVE_DEFAULT_DEVICE;
    }

    //cli usage: idm_nvme_io write
    if(argc >= 2){
        if(strcmp(argv[1], "write") == 0){

            //Simulated ILM input
            char        lock_id[64] = "lock_id";
            int         mode        = IDM_MODE_EXCLUSIVE;
            char        host_id[32] = "host_id";
            char        drive[256]  = "/dev/nvme0n1";
            uint64_t    timeout     = 10;

            //Create required input structs the IDM API would normally create
            nvmeIdmRequest   request_idm;
            nvmeIdmVendorCmd cmd_idm;
            idmData          data_idm;
            int              ret = SUCCESS;

            ret = nvme_idm_write_init(&request_idm, &cmd_idm, &data_idm,
                                      lock_id, mode, host_id,drive, timeout, 0, 0);
            printf("%s exiting with %d\n", argv[1], ret);

            ret = nvme_idm_write(&request_idm);
            printf("%s exiting with %d\n", argv[1], ret);
        }

    }


    return 0;
}
#endif//MAIN_ACTIVATE
