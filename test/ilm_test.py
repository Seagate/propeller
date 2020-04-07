from __future__ import absolute_import

import errno
import io
import os
import time
import uuid

import pytest

import ilm

def test_lockspace(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock_shareable__smoke_test(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock_shareable__100_times(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    for i in range(100):
        lock_op = ilm.idm_lock_op()
        lock_op.mode = ilm.IDM_MODE_SHAREABLE
        lock_op.drive_num = 2
        lock_op.set_drive_names(0, "/dev/sda1")
        lock_op.set_drive_names(1, "/dev/sda2")
        lock_op.timeout = 0

        ret = ilm.ilm_lock(s, lock_id, lock_op)
        assert ret == 0

        ret = ilm.ilm_unlock(s, lock_id)
        assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock_exclusive__smoke_test(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock_exclusive__100_times(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    for i in range(100):
        lock_op = ilm.idm_lock_op()
        lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
        lock_op.drive_num = 2
        lock_op.set_drive_names(0, "/dev/sda1")
        lock_op.set_drive_names(1, "/dev/sda2")
        lock_op.timeout = 0

        ret = ilm.ilm_lock(s, lock_id, lock_op)
        assert ret == 0

        ret = ilm.ilm_unlock(s, lock_id)
        assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock_wrong_mode(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    lock_op = ilm.idm_lock_op()
    lock_op.mode = 0
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -1

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock_exclusive__one_fail_one_success(ilm_daemon):
    ret, s1 = ilm.ilm_connect()
    assert ret == 0
    assert s1 > 0

    host_id = "00000000000000010000000000000001"
    ret = ilm.ilm_set_host_id(s1, host_id, 32)

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 0

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == -16

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

def test_lock_exclusive__two_host_one_fail_one_success(ilm_daemon):
    ret, s1 = ilm.ilm_connect()
    assert ret == 0
    assert s1 > 0

    host_id = "00000000000000000000000000000000"
    ret = ilm.ilm_set_host_id(s1, host_id, 32)

    ret, s2 = ilm.ilm_connect()
    assert ret == 0
    assert s2 > 0

    host_id = "11111111111111111111111111111111"
    ret = ilm.ilm_set_host_id(s2, host_id, 32)

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 0

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == -1

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0
