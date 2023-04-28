/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <string.h>

#include "idm_cmd_common.h"
#include <ilm.h>
#include "test_conf.h"

struct idm_lock_id lock_id;
int need_exit = 0;

static void *test_thread(void *data)
{
	struct idm_lock_op lock_op;
	int ret, s;
	char host_id[32];
	int i;

	host_id[0] = *(char *)data;

	ret = ilm_connect(&s);
	if (ret) {
		printf("[%d] ilm_connect: FAIL line %d\n", __LINE__, ret);
		exit(-1);
	}

	ret = ilm_set_host_id(s, host_id, 32);
	if (ret) {
		printf("[%d] ilm_set_host_id: FAIL line %d\n", __LINE__, ret);
		exit(-1);
	}

	while (1) {
		if (need_exit)
			break;

		lock_op.mode = IDM_MODE_EXCLUSIVE;
		lock_op.drive_num = 4;
		lock_op.drives[0] = BLK_DEVICE1;
		lock_op.drives[1] = BLK_DEVICE2;
		lock_op.drives[2] = BLK_DEVICE3;
		lock_op.drives[3] = BLK_DEVICE4;
		lock_op.timeout = 3000;

		ret = ilm_lock(s, &lock_id, &lock_op);
		if (ret)
			continue;

		ret = ilm_convert(s, &lock_id, IDM_MODE_SHAREABLE);
		if (ret) {
			printf("[%d] ilm_convert: FAIL (EXCLUSIVE -> SHREABLE) %d\n",
			       __LINE__, ret);
			exit(-1);
		}

		ret = ilm_unlock(s, &lock_id);
		if (ret) {
			printf("[%d] ilm_unlock: FAIL %d\n", __LINE__, ret);
			exit(-1);
		}

		lock_op.mode = IDM_MODE_SHAREABLE;

		ret = ilm_lock(s, &lock_id, &lock_op);
		if (ret)
			continue;

		ret = ilm_convert(s, &lock_id, IDM_MODE_EXCLUSIVE);

		ret = ilm_unlock(s, &lock_id);
		if (ret) {
			printf("[%d] ilm_unlock: FAIL %d\n", __LINE__, ret);
			exit(-1);
		}
	}

	ret = ilm_disconnect(s);
	if (ret) {
		printf("[%d] ilm_disconnect: FAIL %d\n", __LINE__, ret);
		exit(-1);
	}

	return 0;
}

int main(void)
{
	pthread_t tid[4];
	int arg[4];
	int i;

	memset(lock_id.vg_uuid, 0x1, 32);
	memset(lock_id.lv_uuid, 0x2, 32);

	for (i = 0; i < 4; i++) {
		arg[i] = i;
		pthread_create(&tid[i], NULL, test_thread, &arg[i]);
	}

	sleep(600);

	need_exit = 1;

	for (i = 0; i < 4; i++)
		pthread_join(tid[i], NULL);

	return 0;
}
