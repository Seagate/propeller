/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 */

#include <dirent.h>

#define NVME_NAME		"nvme"
#define NVME_WWN_PREFIX		"eui."
#define NVME_NAMESPACE_TAG	"n"
#define NVME_PARTITION_TAG	"p"

int ilm_nvme_dir_select(const struct dirent *s);
