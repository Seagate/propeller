/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * ani_api.h - Header file for the IDM aync nvme interface (ANI).
 * This interface is for simulating NVMe asynchronous command behavior.
 * This is in-lieu of using Linux kernel-supplied NVMe asynchronous capability.
 * Currently bypassing the kernel-supplied async functionality due to Linux
 * kernel being too "new".
*
*/

#ifndef __ANI_API_H__
#define __ANI_API_H__

#include "idm_nvme_io.h"
// struct idm_nvme_request;	//forward declaration
// struct nvme_admin_cmd;		//forward declaration
//TODO: Which is better practice???
//		1.) #include "idm_nvme_io.h" HERE, or
//		2.) #include "idm_nvme_io.h" in ani_api.c
//			WITH 2 struct forward declarations HERE


struct arg_ioctl{
	int                     fd;
	unsigned long           ctrl;
	struct nvme_admin_cmd   *cmd;
};
//TODO: Need mutex in this struct?
//		Shouldn't, it's fire and forget.  So, I should never look at it again.

//TODO: Need wrapper function for ioctl >>>>>>>>>  ani_ioctl()
//
//		I NEED TO PASS ani_ioctl() TO THE THREAD POOL, NOT ioctl().
//
//		ani_send_cmd() is where I malloc() 2 things:
//			- struct arg_ioctl_thrd *arg
//			- struct nvme_admin_cmd *cmd
//			//TODO: May have to malloc() in IDM layer
//				If do, malloc()ing in IDM, but free()ing in ANI.  Problem??
//
//		ani_ioctl() will do the following:
//			- cast (void *)arg to (struct arg_ioctl_thrd*)arg, and then use it.
//			- free() struct arg_ioctl_thrd *arg
//			- free() struct nvme_admin_cmd *cmd

/* ================== ASYNC NVME INTERFACE(ANI) ===================== */

int  ani_init(void);
int  ani_send_cmd(struct idm_nvme_request *request_idm, int fd_nvme,
                  unsigned long ctrl, struct nvme_admin_cmd *cmd);
void ani_destroy(void);

#endif /*__ANI_API_H__ */
