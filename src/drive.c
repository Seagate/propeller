/*
 * Copyright (C) 2020-2021 Seagate
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <blkid/blkid.h>
#include <ctype.h>
#include <dirent.h>
#include <libudev.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

#include "ilm_internal.h"
#include "ilm.h"

#include "drive.h"
#include "list.h"
#include "log.h"

#define SYSFS_ROOT		"/sys"
#define BUS_SCSI_DEVS		"/bus/scsi/devices"

struct ilm_hw_drive_path {
	char *blk_path;
	char *sg_path;
};

struct ilm_hw_drive {
	unsigned long wwn;
	uint32_t path_num;
	struct ilm_hw_drive_path path[ILM_DRIVE_MAX_NUM];
};

struct ilm_hw_drive_node {
	struct list_head list;
	struct ilm_hw_drive drive;
};

static pthread_t drive_thd;
static unsigned int drive_thd_done;
static struct list_head drive_list;
static pthread_mutex_t drive_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int drive_list_version = 0;

static char blk_str[PATH_MAX];

struct ilm_device_map {
	struct list_head list;
	char *dev_map;
	char *sg_path;
	unsigned long wwn;
};

static struct list_head dev_map_list = LIST_HEAD_INIT(dev_map_list);
static int dev_map_num = 0;
static pthread_mutex_t dev_map_mutex = PTHREAD_MUTEX_INITIALIZER;

char *ilm_find_cached_device_mapping(char *dev_map, unsigned long *wwn)
{
	struct ilm_device_map *pos;
	char *path;

	pthread_mutex_lock(&dev_map_mutex);

	list_for_each_entry(pos, &dev_map_list, list) {
		if (!strcmp(dev_map, pos->dev_map)) {
			ilm_log_dbg("Find cached device mapping %s->%s",
				    dev_map, pos->sg_path);
			*wwn = pos->wwn;
			path = strdup(pos->sg_path);
			pthread_mutex_unlock(&dev_map_mutex);
			return path;
		}
	}

	pthread_mutex_unlock(&dev_map_mutex);
	return NULL;
}

int ilm_add_cached_device_mapping(char *dev_map, char *sg_path,
				  unsigned long wwn)
{
	struct ilm_device_map *pos, *tmp;

	if (!wwn) {
		ilm_log_err("%s: cannot cache for %s with wwn is zero\n",
			    __func__, dev_map);
		return -1;
	}

	pthread_mutex_lock(&dev_map_mutex);

	list_for_each_entry(pos, &dev_map_list, list) {
		if (!strcmp(dev_map, pos->dev_map)) {
			if (strcmp(sg_path, pos->sg_path)) {
				ilm_log_warn("Find stale cached device mapping old=%s new=%s",
					     pos->sg_path, sg_path);
				/* Free the old sg path */
				free(pos->sg_path);

				/* Update for new found device mapping */
				pos->sg_path = strdup(sg_path);
				pos->wwn = wwn;
			}

			/* Find existed cached item, bail out */
			pthread_mutex_unlock(&dev_map_mutex);
			return 0;
		}
	}

	if (dev_map_num > 100) {
		tmp = list_first_entry(&dev_map_list,
				       struct ilm_device_map, list);
		if (tmp) {
			free(tmp->dev_map);
			free(tmp->sg_path);
			free(tmp);
		}
		dev_map_num--;
	}

	tmp = malloc(sizeof(struct ilm_device_map));
	tmp->dev_map = strdup(dev_map);
	tmp->sg_path = strdup(sg_path);
	tmp->wwn = wwn;
	list_add_tail(&tmp->list, &dev_map_list);

	pthread_mutex_unlock(&dev_map_mutex);
	return 0;
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
		ilm_log_warn("fail to lookup blkid value %s", dev);
		memset(uuid, 0x0, sizeof(uuid_t));
	} else {
		ilm_log_dbg("blkid uuid_str %s", uuid_str);
		uuid_parse(uuid_str, id);
		memcpy(uuid, &id, sizeof(uuid_t));
	}

	blkid_free_probe(probe);
	return ret;
#endif
}

int ilm_read_parttable_id(char *dev, uuid_t *uuid)
{
#ifdef IDM_PTHREAD_EMULATION
	*id = malloc(sizeof(uuid_t));

	uuid_generate(id);
	memcpy(uuid, &id, sizeof(uuid_t));
	return 0;
#else
	blkid_probe probe;
	blkid_partlist ls;
	blkid_parttable root_tab;
	const char *uuid_str;
	uuid_t id;

	probe = blkid_new_probe_from_filename(dev);
	if (!probe) {
		ilm_log_err("fail to create blkid probe for %s", dev);
		return -1;
	}

	/* Binary interface */
	ls = blkid_probe_get_partitions(probe);
	if (!ls) {
		ilm_log_err("fail to read partitions for %s", dev);
		return -1;
	}

	root_tab = blkid_partlist_get_table(ls);
	if (!root_tab) {
		ilm_log_err("doesn't contains any partition table %s", dev);
		return -1;
	}

	uuid_str = blkid_parttable_get_id(root_tab);
	if (!uuid_str) {
		ilm_log_err("fail to read partition table id %s", dev);
		return -1;
	}

	ilm_log_dbg("blkid parttable uuid_str %s", uuid_str);
	uuid_parse(uuid_str, id);
	memcpy(uuid, &id, sizeof(uuid_t));

	blkid_free_probe(probe);
	return 0;
#endif
}

int ilm_read_device_wwn(char *dev, unsigned long *wwn)
{
	char cmd[128];
	char buf[512];
	char tmp[128], tmp1[128];
	FILE *fp;
	int ret;

	snprintf(cmd, sizeof(cmd), "udevadm info %s", dev);
	ilm_log_dbg("%s: cmd=%s", __func__, cmd);

	if ((fp = popen(cmd, "r")) == NULL) {
		ilm_log_err("fail to find udev info for %s", dev);
		return -1;
	}

	*wwn = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		ret = sscanf(buf, "%s ID_WWN=%s", tmp, tmp1);
		if (ret != 2)
			continue;

		*wwn = strtol(tmp1, NULL, 16);
		ilm_log_dbg("%s: dev=%s wwn=0x%lx\n", __func__, dev, *wwn);
		break;
	}

	pclose(fp);

	if (!*wwn) {
		ilm_log_err("%s: cmd=%s", __func__, cmd);
		return -1;
	}

        return 0;
}

#ifndef IDM_PTHREAD_EMULATION
static int ilm_scsi_dir_select(const struct dirent *s)
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
			closedir(dirp);
			strcpy(dev, path);
			return 0;
		}
	}

	closedir(dirp);
	return -1;
}

static int ilm_scsi_block_node_select(const struct dirent *s)
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

	strncpy(blk_str, s->d_name, PATH_MAX);
	return 1;
}

static int ilm_scsi_find_block_node(const char *dir_name)
{
        int num, i;
        struct dirent **namelist;

        num = scandir(dir_name, &namelist, ilm_scsi_block_node_select, NULL);
        if (num < 0)
                return -1;

        for (i = 0; i < num; ++i)
                free(namelist[i]);
        free(namelist);
        return num;
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

	num = scandir(devs_path, &namelist, ilm_scsi_dir_select, NULL);
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

static int ilm_find_deepest_device_mapping(char *in, char *out)
{
	char in_tmp[128], out_tmp[128], tmp[128];
	struct stat stats;
	char cmd[128];
	char buf[512];
	FILE *fp;
	int num;
	int ret;

	if (stat(in, &stats)) {
		ilm_log_dbg("%s: Fail to stat %s", __func__, in);
		goto failed;
	}

	snprintf(cmd, sizeof(cmd), "dmsetup deps -o devname %s", in);
	ilm_log_dbg("%s: cmd=%s", __func__, cmd);

	if ((fp = popen(cmd, "r")) == NULL) {
		ilm_log_dbg("%s: Fail to execute command %s", __func__, cmd);
		goto failed;
	}

	if (fgets(buf, sizeof(buf), fp) == NULL) {
		ilm_log_dbg("%s: Fail to read command buffer %s", __func__, cmd);
		pclose(fp);
		goto failed;
	}

	pclose(fp);

	ilm_log_dbg("%s: buf=%s", __func__, buf);

	ret = sscanf(buf, "%u dependencies  : (%[a-zA-Z0-9_-])", &num, tmp);
	if (ret == EOF) {
		ilm_log_dbg("%s: Fail to sscanf string %s", __func__, buf);
		goto failed;
	}

	snprintf(in_tmp, sizeof(in_tmp), "/dev/mapper/%s", tmp);
	ilm_log_dbg("%s: device mapper %s", __func__, in_tmp);

	ret = ilm_find_deepest_device_mapping(in_tmp, out_tmp);
	if (ret < 0) {
		strcpy(out, tmp);
	} else {
		strcpy(out, out_tmp);
	}
	return 0;

failed:
	strcpy(out, in);
	return -1;
}

char *ilm_scsi_convert_blk_name(char *blk_dev)
{
	char in_tmp[128], tmp[128];
	char *base_name;
	char *blk_name;
	int i;

	strncpy(in_tmp, blk_dev, sizeof(in_tmp));
	ilm_find_deepest_device_mapping(in_tmp, tmp);

	i = strlen(tmp);
	if (!i)
		return NULL;

	/* Iterate all digital */
	while ((i > 0) && isdigit(tmp[i-1]))
		i--;

	tmp[i] = '\0';

	base_name = basename(tmp);
	blk_name = malloc(strlen(base_name) + 1);
	strncpy(blk_name, base_name, strlen(base_name) + 1);

	ilm_log_dbg("%s: block device mapping %s -> %s",
		    __func__, blk_dev, blk_name);
	return blk_name;
}

char *ilm_convert_sg(char *blk_dev)
{
#ifdef IDM_PTHREAD_EMULATION
	char *sg;

	sg = strdup(blk_dev);
	return sg;
#else
	char *sg;

	sg = ilm_find_sg(blk_dev);
	return sg;
#endif
}

char *ilm_scsi_get_first_sg(char *dev)
{
	struct ilm_hw_drive_node *pos, *found = NULL;
	char *tmp;
	int i;

	pthread_mutex_lock(&drive_list_mutex);

	list_for_each_entry(pos, &drive_list, list) {
		for (i = 0; i < pos->drive.path_num; i++) {
			if (!strcmp(dev, basename(pos->drive.path[i].blk_path))) {
				found = pos;
				break;
			}
		}
	}

	if (!found) {
		tmp = NULL;
		goto out;
	}

	tmp = strdup(found->drive.path[0].sg_path);

out:
	pthread_mutex_unlock(&drive_list_mutex);
	return tmp;
}

/* Read out the SG path strings */
int ilm_scsi_get_all_sgs(unsigned long wwn, char **sg_node, int sg_num)
{
	struct ilm_hw_drive_node *pos, *found = NULL;
	int i;

	pthread_mutex_lock(&drive_list_mutex);

	list_for_each_entry(pos, &drive_list, list) {
		if (pos->drive.wwn == wwn) {
			found = pos;
			break;
		}
	}

	if (!found) {
		sg_num = 0;
		goto out;
	}

	if (sg_num > found->drive.path_num)
		sg_num = found->drive.path_num;

	for (i = 0; i < sg_num; i++)
		sg_node[i] = strdup(found->drive.path[i].sg_path);

out:
	pthread_mutex_unlock(&drive_list_mutex);
	return sg_num;
}

#if 0
int ilm_scsi_get_part_table_uuid(char *dev, uuid_t *id)
{
	struct ilm_hw_drive_node *pos, *found = NULL;
	int i;

	list_for_each_entry(pos, &drive_list, list) {
		for (i = 0; i < pos->drive.path_num; i++) {
			if (!strcmp(basename(dev),
				    basename(pos->drive.path[i].blk_path))) {
				found = pos;
				break;
			}

			if (!strcmp(basename(dev),
				    basename(pos->drive.path[i].sg_path))) {
				found = pos;
				break;
			}
		}
	}

	if (!found)
		return -1;

	memcpy(id, &found->drive.id, sizeof(uuid_t));
	return 0;
}
#endif

static void ilm_scsi_dump_nodes(void)
{
	struct ilm_hw_drive_node *pos;
	int i;

	pthread_mutex_lock(&drive_list_mutex);

	list_for_each_entry(pos, &drive_list, list) {
		ilm_log_dbg("SCSI dev WWN: 0x%lx", pos->drive.wwn);

		for (i = 0; i < pos->drive.path_num; i++) {
			ilm_log_dbg("blk_path %s", pos->drive.path[i].blk_path);
			ilm_log_dbg("sg_path %s", pos->drive.path[i].sg_path);
		}
	}

	pthread_mutex_unlock(&drive_list_mutex);
}

int ilm_scsi_drive_version(void)
{
	int version;

	pthread_mutex_lock(&drive_list_mutex);
	version = drive_list_version;
	pthread_mutex_unlock(&drive_list_mutex);

	return version;
}

static int ilm_scsi_add_drive_path(char *dev_node, char *sg_node,
				   unsigned long wwn)
{
	struct ilm_hw_drive_node *pos, *found = NULL;
	struct ilm_hw_drive *drive;
	int i;

	pthread_mutex_lock(&drive_list_mutex);

	list_for_each_entry(pos, &drive_list, list) {
		if (pos->drive.wwn == wwn) {
			found = pos;
			break;
		}
	}

	if (!found) {
		found = malloc(sizeof(struct ilm_hw_drive_node));
		memset(found, 0x0, sizeof(struct ilm_hw_drive_node));
		found->drive.wwn = wwn;
		list_add(&found->list, &drive_list);
	}

	drive = &found->drive;

	for (i = 0; i < drive->path_num; i++) {
		/*
		 * If the device node has been added into the list,
		 * avoid to add the duplicate path.
		 */
		if (!strcmp(drive->path[i].blk_path, dev_node)) {
			pthread_mutex_unlock(&drive_list_mutex);
			return 0;
		}
	}

	/* Detect if it's overflow for the drive path array */
	if (drive->path_num >= (ILM_DRIVE_MAX_NUM - 1)) {
		pthread_mutex_unlock(&drive_list_mutex);
		return -1;
	}

	drive->path[drive->path_num].blk_path = strdup(dev_node);
	drive->path[drive->path_num].sg_path = strdup(sg_node);
	drive->path_num++;

	drive_list_version++;
	pthread_mutex_unlock(&drive_list_mutex);
	return 0;
}

static int ilm_scsi_del_drive_path(char *dev_node)
{
	struct ilm_hw_drive_node *pos, *found = NULL;
	struct ilm_hw_drive *drive;
	int i;

	pthread_mutex_lock(&drive_list_mutex);

	list_for_each_entry(pos, &drive_list, list) {
		drive = &pos->drive;
		for (i = 0; i < drive->path_num; i++) {
			if (!strcmp(drive->path[i].blk_path, dev_node)) {
				found = pos;

				/* Cleanup the path info */
				free(drive->path[i].blk_path);
				drive->path[i].blk_path = NULL;
				free(drive->path[i].sg_path);
				drive->path[i].sg_path = NULL;
				goto clean_node;
			}
		}
	}

	if (!found) {
		pthread_mutex_unlock(&drive_list_mutex);
		ilm_log_warn("%s: fail to find dev node %s", __func__, dev_node);
		return -1;
	}

clean_node:
	drive = &found->drive;
	for (i = 0; i < drive->path_num - 1; i++) {
		if (drive->path[i].blk_path)
			continue;

		drive->path[i].blk_path = drive->path[i + 1].blk_path;
		drive->path[i].sg_path = drive->path[i + 1].sg_path;
		drive->path[i + 1].blk_path = NULL;
		drive->path[i + 1].sg_path = NULL;
	}
	drive->path_num--;

	drive_list_version++;
	pthread_mutex_unlock(&drive_list_mutex);
	return 0;
}

static int ilm_scsi_release_drv_list(void)
{
	struct ilm_hw_drive_node *pos, *next;
	struct ilm_hw_drive *drive;
	int i;

	pthread_mutex_lock(&drive_list_mutex);

	list_for_each_entry_safe(pos, next, &drive_list, list) {
		list_del(&pos->list);

		drive = &pos->drive;
		for (i = 0; i < drive->path_num; i++) {
			free(drive->path[i].blk_path);
			free(drive->path[i].sg_path);
		}

		free(pos);
	}

	pthread_mutex_unlock(&drive_list_mutex);
	return 0;
}

static void *drive_thd_fn(void *arg __maybe_unused)
{
	struct udev *udev;
	struct udev_device *dev;
	struct udev_monitor *mon;
	int fd;

	/* create udev object */
	udev = udev_new();
	if (!udev) {
		ilm_log_err("%s: Can't create udev", __func__);
		goto out;
	}

	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL);
	udev_monitor_enable_receiving(mon);
	fd = udev_monitor_get_fd(mon);

	while (1) {
		fd_set fds;
		struct timeval tv;
		int ret;

		/* Exit if the main thread is exiting */
		if (drive_thd_done)
			break;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 1;		/* Timeout is 1s */
		tv.tv_usec = 0;

		ret = select(fd + 1, &fds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(fd, &fds)) {
			const char *action;
			char *dev_name;
			char *sg = NULL;
			char dev_node[64];
			unsigned long wwn;
			int i;

			dev = udev_monitor_receive_device(mon);
			if (!dev)
				continue;

			action = udev_device_get_action(dev);

			dev_name = strdup(udev_device_get_sysname(dev));

			i = strlen(dev_name);
			if (!i)
				continue;

			/* Iterate all digital */
			while ((i > 0) && isdigit(dev_name[i-1]))
				i--;

			dev_name[i] = '\0';

			ilm_log_dbg("%s: action=%s dev_name=%s", __func__,
				    action, dev_name);

			snprintf(dev_node, sizeof(dev_node), "/dev/%s", dev_name);

			if (!strcmp(action, "add")) {
				sg = ilm_find_sg(dev_name);
				if (!sg) {
					ilm_log_err("%s: Fail to find sg for %s",
						    __func__, dev_name);
					goto free_dev_ref;
				}

				ret = ilm_read_device_wwn(dev_node, &wwn);
				if (ret < 0) {
					ilm_log_err("%s: Fail to read wwn for %s",
						    __func__, dev_node);
					goto free_dev_ref;
				}

				ilm_scsi_add_drive_path(dev_node, sg, wwn);
			} else if (!strcmp(action, "remove")) {
				ilm_scsi_del_drive_path(dev_node);
			}

			ilm_scsi_dump_nodes();

free_dev_ref:
			if (sg)
				free(sg);
			free(dev_name);
			/* free dev */
			udev_device_unref(dev);
		}
	}

	/* free udev */
	udev_unref(udev);

out:
	pthread_exit(NULL);
}

int ilm_scsi_list_init(void)
{
	struct dirent **namelist;
	char devs_path[PATH_MAX];
	char dev_path[PATH_MAX];
	char blk_path[PATH_MAX];
	char dev_node[PATH_MAX];
	char sg_node[PATH_MAX];
	int i, num;
	int ret = 0;
	char value[64];
	unsigned int maj, min;
	unsigned long wwn;

	INIT_LIST_HEAD(&drive_list);

	snprintf(devs_path, sizeof(devs_path), "%s%s",
		 SYSFS_ROOT, BUS_SCSI_DEVS);

	num = scandir(devs_path, &namelist, ilm_scsi_dir_select, NULL);
	if (num < 0) {  /* scsi mid level may not be loaded */
		ilm_log_err("Attached devices: none");
		return -1;
	}

	for (i = 0; i < num; ++i) {
		ret = snprintf(dev_path, sizeof(dev_path), "%s/%s",
			       devs_path, namelist[i]->d_name);
		if (ret < 0) {
			ilm_log_err("string is out of memory");
			goto out;
		}

		ret = snprintf(blk_path, sizeof(blk_path), "%s/%s",
			       dev_path, "block");
		if (ret < 0) {
			ilm_log_err("string is out of memory");
			goto out;
		}

		ret = ilm_scsi_find_block_node(blk_path);
		if (ret < 0)
			continue;

		snprintf(dev_node, sizeof(dev_node), "/dev/%s", blk_str);

		ret = ilm_scsi_change_sg_folder(dev_path);
		if (ret < 0) {
			ilm_log_err("fail to change sg folder");
			continue;
		}

		if (NULL == getcwd(blk_path, sizeof(blk_path))) {
			ilm_log_err("generic_dev error");
			continue;
		}

		ret = ilm_scsi_get_value(blk_path, "dev", value, sizeof(value));
		if (ret < 0) {
			ilm_log_err("fail to get device value");
			continue;
		}

		sscanf(value, "%u:%u", &maj, &min);

		ret = ilm_scsi_parse_sg_node(maj, min, sg_node);
		if (ret < 0) {
			ilm_log_err("fail to find blk node %d:%d", maj, min);
			continue;
		}

		ilm_log_dbg("%s: dev_node=%s sg_node=%s", __func__,
			    dev_node, sg_node);

		ret = ilm_read_device_wwn(dev_node, &wwn);
		if (ret < 0) {
			ilm_log_err("fail to read parttable id");
			continue;
		}

		ret = ilm_scsi_add_drive_path(dev_node, sg_node, wwn);
		if (ret < 0) {
			ilm_log_err("fail to add scsi node");
			goto out;
		}
	}

	ret = pthread_create(&drive_thd, NULL, drive_thd_fn, NULL);
	if (ret) {
		ilm_log_err("Fail to create drive thread");
		goto out;
	}

	ilm_scsi_dump_nodes();
out:
        for (i = 0; i < num; i++)
                free(namelist[i]);
	free(namelist);

	if (ret)
		ilm_scsi_release_drv_list();

	return ret;
}

void ilm_scsi_list_exit(void)
{
	/* Notify drive thread to exit */
	drive_thd_done = 1;

	/* Wait for drive thread to exit */
	pthread_join(drive_thd, NULL);

	ilm_scsi_release_drv_list();
}
