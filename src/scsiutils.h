/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __SCSIUTILS_H__
#define __SCSIUTILS_H__

#include <dirent.h>

int ilm_scsi_dir_select(const struct dirent *s);
int ilm_scsi_change_sg_folder(const char *dir_name);
int ilm_scsi_parse_sg_node(unsigned int maj, unsigned int min,
			   char *dev);
int ilm_scsi_block_node_select(const struct dirent *s);
int ilm_scsi_find_block_node(const char *dir_name, char **blk_dev);
int ilm_scsi_get_value(const char *dir_name, const char *base_name,
		       char *value, int max_value_len);

#endif /* __SCSIUTILS_H__ */
