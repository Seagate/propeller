/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * async_nvme.h - Interface for simulating NVMe asynchronous command behavior.
 * This is in-lieu of using Linux kernel-supplied NVMe asynchronous capability.
 * Currently bypassing the kernel-supplied async functionality due to Linux
 * kernel being too "new".
*
*/


/* ===================== ASYNC NVME INTERFACE ======================= */
int  async_nvme_init();
void async_nvme_destroy();

