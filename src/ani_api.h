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

struct idm_nvme_request;//forward declaration

struct arg_ioctl{
	int                        fd;
	unsigned long              ctrl;
	struct nvme_passthru_cmd   *cmd;
};

/* ================== ASYNC NVME INTERFACE(ANI) ===================== */

int  ani_init(void);
int  ani_data_rcv(struct idm_nvme_request *request_idm, int *result);
int  ani_send_cmd(struct idm_nvme_request *request_idm, unsigned long ctrl);
void ani_destroy(void);

#endif /*__ANI_API_H__ */
