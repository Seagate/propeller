/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * async_nvme.h - Header file for the IDM aync nvme interface (ANI).
 * This interface is for simulating NVMe asynchronous command behavior.
 * This is in-lieu of using Linux kernel-supplied NVMe asynchronous capability.
 * Currently bypassing the kernel-supplied async functionality due to Linux
 * kernel being too "new".
*
*/


/* ================== ASYNC NVME INTERFACE(ANI) ===================== */
int  ani_init();
void ani_destroy();

