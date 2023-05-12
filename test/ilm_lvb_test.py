# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.


import errno
import io
import os
import time
import uuid

import pytest

import ilm
from test_conf import *     # Normally bad practice, but only importing 'constants' here

def test_lock__lvb_read(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    a = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s, lock_id, a, 8)
    assert ret == 0
    assert ord(a[0]) == 0
    assert ord(a[1]) == 0
    assert ord(a[2]) == 0
    assert ord(a[3]) == 0
    assert ord(a[4]) == 0
    assert ord(a[5]) == 0
    assert ord(a[6]) == 0
    assert ord(a[7]) == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__lvb_read_two_hosts(ilm_daemon, reset_devices):
    ret, s1 = ilm.ilm_connect()
    assert ret == 0
    assert s1 > 0

    host_id = HOST1
    ret = ilm.ilm_set_host_id(s1, host_id, 32)

    ret, s2 = ilm.ilm_connect()
    assert ret == 0
    assert s2 > 0

    host_id = HOST2
    ret = ilm.ilm_set_host_id(s2, host_id, 32)

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    a = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s1, lock_id, a, 8)
    assert ret == 0
    assert ord(a[0]) == 0
    assert ord(a[1]) == 0
    assert ord(a[2]) == 0
    assert ord(a[3]) == 0
    assert ord(a[4]) == 0
    assert ord(a[5]) == 0
    assert ord(a[6]) == 0
    assert ord(a[7]) == 0

    a[0] = 'a'
    a[1] = 'b'
    a[2] = 'c'
    a[3] = 'd'
    a[4] = 'e'
    a[5] = 'f'
    a[6] = 'g'
    a[7] = 'h'

    ret = ilm.ilm_write_lvb(s1, lock_id, a, 8)
    assert ret == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret, s1 = ilm.ilm_connect()
    assert ret == 0
    assert s1 > 0

    host_id = HOST1
    ret = ilm.ilm_set_host_id(s1, host_id, 32)

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    b = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s1, lock_id, b, 8)
    assert ret == 0
    assert b[0] == 'a'
    assert b[1] == 'b'
    assert b[2] == 'c'
    assert b[3] == 'd'
    assert b[4] == 'e'
    assert b[5] == 'f'
    assert b[6] == 'g'
    assert ord(b[7]) == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    c = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s2, lock_id, c, 8)
    assert ret == 0
    assert c[0] == 'a'
    assert c[1] == 'b'
    assert c[2] == 'c'
    assert c[3] == 'd'
    assert c[4] == 'e'
    assert c[5] == 'f'
    assert c[6] == 'g'
    assert ord(c[7]) == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__lvb_write(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    a = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s, lock_id, a, 8)
    assert ret == 0

    a[0] = 'a'
    ret = ilm.ilm_write_lvb(s, lock_id, a, 8)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    a[0] = '0'
    ret = ilm.ilm_read_lvb(s, lock_id, a, 8)
    assert ret == 0
    assert a[0] == 'a'

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__lvb_write_two_hosts(ilm_daemon, reset_devices):
    ret, s1 = ilm.ilm_connect()
    assert ret == 0
    assert s1 > 0

    host_id = HOST1
    ret = ilm.ilm_set_host_id(s1, host_id, 32)

    ret, s2 = ilm.ilm_connect()
    assert ret == 0
    assert s2 > 0

    host_id = HOST2
    ret = ilm.ilm_set_host_id(s2, host_id, 32)

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    a = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s1, lock_id, a, 8)
    assert ret == 0

    a[0] = 'a'
    a[1] = 'b'
    a[2] = 'c'
    a[3] = 'd'
    a[4] = 'e'
    a[5] = 'f'
    a[6] = 'g'
    a[7] = 'h'

    ret = ilm.ilm_write_lvb(s1, lock_id, a, 8)
    assert ret == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    b = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s2, lock_id, b, 8)
    assert ret == 0
    assert b[0] == 'a'
    assert b[1] == 'b'
    assert b[2] == 'c'
    assert b[3] == 'd'
    assert b[4] == 'e'
    assert b[5] == 'f'
    assert b[6] == 'g'
    assert ord(b[7]) == 0

    b[0] = 'U'
    b[1] = 'U'
    b[2] = 'U'
    b[3] = 'U'
    b[4] = 'U'
    b[5] = 'U'
    b[6] = 'U'
    b[7] = 'U'

    ret = ilm.ilm_write_lvb(s2, lock_id, b, 8)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_read_lvb(s1, lock_id, a, 8)
    assert ret == 0
    assert a[0] == 'U'
    assert a[1] == 'U'
    assert a[2] == 'U'
    assert a[3] == 'U'
    assert a[4] == 'U'
    assert a[5] == 'U'
    assert a[6] == 'U'
    assert ord(a[7]) == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__lvb_read_timeout(ilm_daemon, reset_devices):
    ret, s1 = ilm.ilm_connect()
    assert ret == 0
    assert s1 > 0

    host_id = HOST1
    ret = ilm.ilm_set_host_id(s1, host_id, 32)

    ret, s2 = ilm.ilm_connect()
    assert ret == 0
    assert s2 > 0

    host_id = HOST2
    ret = ilm.ilm_set_host_id(s2, host_id, 32)

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 5000     # Timeout: 5s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_stop_renew(s1)
    assert ret == 0

    time.sleep(6)

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    a = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(s2, lock_id, a, 8)

    assert ret == 0
    assert a[0] == '\udcff'
    assert a[1] == '\udcff'
    assert a[2] == '\udcff'
    assert a[3] == '\udcff'
    assert a[4] == '\udcff'
    assert a[5] == '\udcff'
    assert a[6] == '\udcff'
    assert a[7] == '\udcff'

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0
