/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <blkid/blkid.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

#include "log.h"
#include "util.h"

#define SYSFS_ROOT		"/sys"
#define BUS_SCSI_DEVS		"/bus/scsi/devices"

uint64_t ilm_read_utc_time(void)
{
	struct timeval cur_time;
	uint64_t utc_ms;

	gettimeofday(&cur_time, NULL);

	utc_ms = (uint64_t)(cur_time.tv_sec) * 1000 +
		 (uint64_t)(cur_time.tv_usec) / 1000;
	return utc_ms;
}

/* Get current time in millisecond */
uint64_t ilm_curr_time(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int ilm_rand(int min, int max)
{
        long int ret;

        ret = random();
        if (ret < 0)
                return min;

        return min + (int)(((float)(max - min + 1)) * ret / (RAND_MAX + 1.0));
}

int ilm_read_blk_uuid(char *dev, uuid_t *uuid)
{
#ifdef IDM_PTHREAD_EMULATION
	uuid_t id;

	uuid_generate(id);
	memcpy(uuid, &id, sizeof(uuid_t));
	return 0;
#else
	blkid_probe probe;
	const char *uuid_str;
	size_t uuid_str_size;
	uuid_t id;
	int ret;

	probe = blkid_new_probe_from_filename(dev);
	if (!probe) {
		ilm_log_err("fail to create blkid probe for %s", dev);
		return -1;
	}

	blkid_do_probe(probe);

	ret = blkid_probe_lookup_value(probe, "UUID",
				       &uuid_str, &uuid_str_size);
	if (ret) {
		ilm_log_err("fail to lookup blkid value %s", dev);
		goto out;
	}

	uuid_parse(uuid_str, id);

	memcpy(uuid, &id, sizeof(uuid_t));
out:
	blkid_free_probe(probe);
	return ret;
#endif
}

#ifndef IDM_PTHREAD_EMULATION
static int idm_scsi_dir_select(const struct dirent *s)
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

static int ilm_scsi_change_sg_folder(const char *dir_name)
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

static int ilm_scsi_parse_sg_node(unsigned int maj, unsigned int min,
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
			strcpy(dev, path);
			return 0;
		}
	}

	return -1;
}

static int ilm_scsi_get_value(const char *dir_name, const char *base_name,
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

static char *ilm_find_sg(char *blk_dev)
{
	struct dirent **namelist;
	char devs_path[PATH_MAX];
	char dev_path[PATH_MAX];
	char blk_path[PATH_MAX];
	int i, num;
	int ret;
	char value[64];
	unsigned int maj, min;
        struct stat a_stat;
	char *tmp = NULL;

	snprintf(devs_path, sizeof(devs_path), "%s%s",
		 SYSFS_ROOT, BUS_SCSI_DEVS);

	num = scandir(devs_path, &namelist, idm_scsi_dir_select, NULL);
	if (num < 0) {  /* scsi mid level may not be loaded */
		ilm_log_err("Attached devices: none");
		return NULL;
	}

	for (i = 0; i < num; ++i) {
		ret = snprintf(dev_path, sizeof(dev_path), "%s/%s",
			       devs_path, namelist[i]->d_name);
		if (ret < 0) {
			ilm_log_err("string is out of memory\n");
			goto out;
		}

		ret = snprintf(blk_path, sizeof(blk_path), "%s/%s/%s",
			       dev_path, "block", blk_dev);
		if (ret < 0) {
			ilm_log_err("string is out of memory");
			goto out;
		}

		ilm_log_err("blk_path=%s", blk_path);

		/* The folder doesn't exist */
		if ((stat(blk_path, &a_stat) < 0))
			continue;

		ret = ilm_scsi_change_sg_folder(dev_path);
		if (ret < 0) {
			ilm_log_err("fail to change sg folder");
			goto out;
		}

		if (NULL == getcwd(blk_path, sizeof(blk_path))) {
			ilm_log_err("generic_dev error");
			goto out;
		}

		ret = ilm_scsi_get_value(blk_path, "dev", value, sizeof(value));
		if (ret < 0) {
			ilm_log_err("fail to get device value");
			goto out;
		}

		sscanf(value, "%u:%u", &maj, &min);

		ret = ilm_scsi_parse_sg_node(maj, min, value);
		if (ret < 0) {
			ilm_log_err("fail to find blk node %d:%d\n", maj, min);
			goto out;
		}

		tmp = strdup(value);
	}

out:
        for (i = 0; i < num; i++)
                free(namelist[i]);
	free(namelist);
        return tmp;
}
#endif

char *ilm_convert_sg(char *blk_dev)
{
#ifdef IDM_PTHREAD_EMULATION
	char *sg;

	sg = strdup(blk_dev);
	return sg;
#else
	int i = strlen(blk_dev);
	char *tmp = strdup(blk_dev);
	char *sg;

	/* Iterate all digital */
	while (isdigit(tmp[i-1]))
		i--;

	tmp[i] = '\0';

	ilm_log_err("blk_dev=%s", tmp);

	sg = ilm_find_sg(basename(tmp));

	free(tmp);
	return sg;
#endif
}
