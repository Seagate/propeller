/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2003-2020 D. Gilbert
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 *
 * Derived from the lsscsi file src/lsscsi.c.
 */

#include <libudev.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include "log.h"
#include "scsiutils.h"

int ilm_scsi_dir_select(const struct dirent *s)
{
	/* Following no longer needed but leave for early lk 2.6 series */
	if (strstr(s->d_name, "mt"))
		return 0;

	/* st auxiliary device names */
	if (strstr(s->d_name, "ot"))
		return 0;

	/* osst auxiliary device names */
	if (strstr(s->d_name, "gen"))
		return 0;

	/* SCSI host */
	if (!strncmp(s->d_name, "host", 4))
		return 0;

	/* SCSI target */
	if (!strncmp(s->d_name, "target", 6))
		return 0;

	/* Only select directory with x:x:x:x */
	if (strchr(s->d_name, ':'))
		return 1;

	return 0;
}

int ilm_scsi_change_sg_folder(const char *dir_name)
{
        const char *old_name = "generic";
        char b[PATH_MAX];
        struct stat a_stat;
	int ret;

        ret = snprintf(b, sizeof(b), "%s/%s", dir_name, old_name);
	if (ret < 0) {
		ilm_log_err("%s: spring is out of range", __func__);
		return -1;
	}

        if ((stat(b, &a_stat) >= 0) && S_ISDIR(a_stat.st_mode)) {
                if (chdir(b) < 0)
                        return -1;

                return 0;
        }

        return -1;
}

int ilm_scsi_parse_sg_node(unsigned int maj, unsigned int min,
			   char *dev)
{
	struct dirent *dep;
	DIR *dirp;
	char path[PATH_MAX];
	struct stat stats;

	dirp = opendir("/dev");
	if (dirp == NULL)
		return -1;

	while (1) {
		dep = readdir(dirp);
		if (dep == NULL)
			break;

		snprintf(path, sizeof(path), "%s/%s", "/dev", dep->d_name);

		/* This will bypass all symlinks in /dev */
		if (lstat(path, &stats))
			continue;

		/* Skip non-block/char files. */
		if ( (!S_ISBLK(stats.st_mode)) && (!S_ISCHR(stats.st_mode)) )
			continue;

		if (major(stats.st_rdev) == maj && minor(stats.st_rdev) == min) {
			closedir(dirp);
			strcpy(dev, path);
			return 0;
		}
	}

	closedir(dirp);
	return -1;
}

int ilm_scsi_block_node_select(const struct dirent *s)
{
	size_t len;

	if (DT_LNK != s->d_type && DT_DIR != s->d_type)
		return 0;

	if (DT_DIR == s->d_type) {
		len = strlen(s->d_name);

		if ((len == 1) && ('.' == s->d_name[0]))
			return 0;   /* this directory: '.' */

		if ((len == 2) &&
		    ('.' == s->d_name[0]) && ('.' == s->d_name[1]))
			return 0;   /* parent: '..' */
	}

	return 1;
}

int ilm_scsi_find_block_node(const char *dir_name, char **blk_dev)
{
        int num, i;
        struct dirent **namelist;

        num = scandir(dir_name, &namelist, ilm_scsi_block_node_select, NULL);
        if (num < 0)
                return -1;

	*blk_dev = strdup(namelist[0]->d_name);

        for (i = 0; i < num; ++i)
                free(namelist[i]);
        free(namelist);

	if (!*blk_dev)
		return -1;

        return num;
}

int ilm_scsi_get_value(const char *dir_name, const char *base_name,
		       char *value, int max_value_len)
{
        int len;
        FILE * f;
        char b[PATH_MAX];
	int ret;

        ret = snprintf(b, sizeof(b), "%s/%s", dir_name, base_name);
	if (ret < 0)
		return -1;

        if (NULL == (f = fopen(b, "r"))) {
                return -1;
        }

        if (NULL == fgets(value, max_value_len, f)) {
                /* assume empty */
                value[0] = '\0';
                fclose(f);
                return 0;
        }

        len = strlen(value);
        if ((len > 0) && (value[len - 1] == '\n'))
                value[len - 1] = '\0';

        fclose(f);
        return 0;
}
