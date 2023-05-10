/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
 */

#include <stdio.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <string.h>

#include <ilm.h>
#include "test_conf.h"

int main(void)
{
	int ret, s;
	struct idm_lock_id lock_id;
	struct idm_lock_op lock_op;

	ret = ilm_connect(&s);
	if (ret == 0) {
		printf("ilm_connect: SUCCESS\n");
	} else {
		printf("ilm_connect: FAIL: %d\n", ret);
		exit(-1);
	}

	memset(lock_id.vg_uuid, 0x1, 32);
	memset(lock_id.lv_uuid, 0x2, 32);

	lock_op.mode = 1;
	lock_op.drive_num = 2;
	lock_op.drives[0] = BLK_DEVICE1;
	lock_op.drives[1] = BLK_DEVICE2;
	lock_op.timeout = 3000;

	ret = ilm_lock(s, &lock_id, &lock_op);
	if (ret == 0) {
		printf("ilm_lock: SUCCESS\n");
	} else {
		printf("ilm_lock: FAIL: %d\n", ret);
		exit(-1);
	}

	ret = ilm_unlock(s, &lock_id);
	if (ret == 0) {
		printf("ilm_unlock: SUCCESS\n");
	} else {
		printf("ilm_unlock: FAIL: %d\n", ret);
		exit(-1);
	}

	ret = ilm_disconnect(s);
	if (ret == 0) {
		printf("ilm_disconnect: SUCCESS\n");
	} else {
		printf("ilm_disconnect: FAIL: %d\n", ret);
		exit(-1);
	}

	return 0;
}
