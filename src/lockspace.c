/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <uuid/uuid.h>
#include <unistd.h>

#include "cmd.h"
#include "list.h"
#include "log.h"

struct ilm_lockspace {
	struct list_head list;
	uuid_t host_uuid;

	struct list_head lock_list;

	int exit;
	pthread_t thd;
	pthread_mutex_t mutex;

	/* TODO: support event and timeout */
};

static struct list_head ls_list = LIST_HEAD_INIT(ls_list);
static pthread_mutex_t ls_mutex = PTHREAD_MUTEX_INITIALIZER;

static int _ls_is_valid(struct ilm_lockspace *ilm_ls)
{
	struct ilm_lockspace *pos;
	int ret = 0;

	pthread_mutex_lock(&ls_mutex);

	list_for_each_entry(pos, &ls_list, list) {

		if (pos == ilm_ls) {
			ret = 1;
			break;
		}
	}

	pthread_mutex_unlock(&ls_mutex);
	return ret;
}

static void *ilm_lockspace_thread(void *data)
{
	struct ilm_lockspace *ilm_ls = data;
	int exit;

	while (1) {
		pthread_mutex_lock(&ilm_ls->mutex);
		exit = ilm_ls->exit;
		pthread_mutex_unlock(&ilm_ls->mutex);

		if (exit)
			break;


		/* TODO: refresh lock membership */

		sleep(1);
	}

	return NULL;
}

int ilm_lockspace_create(struct ilm_cmd *cmd, struct ilm_lockspace **ls_out)
{
	struct ilm_lockspace *ilm_ls;
	int ret;

	ilm_ls = malloc(sizeof(struct ilm_lockspace));
	if (!ilm_ls)
		return -ENOMEM;
	memset(ilm_ls, 0, sizeof(struct ilm_lockspace));

	INIT_LIST_HEAD(&ilm_ls->lock_list);
	pthread_mutex_init(&ilm_ls->mutex, NULL);

	pthread_mutex_lock(&ls_mutex);
	list_add(&ilm_ls->list, &ls_list);
	pthread_mutex_unlock(&ls_mutex);

	ret = pthread_create(&ilm_ls->thd, NULL, ilm_lockspace_thread, ilm_ls);
	if (ret < 0) {
		ilm_log_err("%s: create thread failed", __func__);
		goto fail;
	}

	return 0;

fail:
	pthread_mutex_lock(&ls_mutex);
	list_del(&ilm_ls->list);
	pthread_mutex_unlock(&ls_mutex);

	free(ilm_ls);
	return -1;
}

int ilm_lockspace_delete(struct ilm_lockspace *ilm_ls)
{
	int ret;

	if (!_ls_is_valid(ilm_ls)) {
		ilm_log_err("%s: lockspace is invalid\n", __func__);
		return -1;
	}

	pthread_mutex_lock(&ilm_ls->mutex);
	ilm_ls->exit = 1;
	pthread_mutex_unlock(&ilm_ls->mutex);

	ret = pthread_join(ilm_ls->thd, NULL);

	pthread_mutex_lock(&ls_mutex);
	list_del(&ilm_ls->list);
	pthread_mutex_unlock(&ls_mutex);

	return ret;
}
