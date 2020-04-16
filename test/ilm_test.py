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

def test_lock__shareable_smoke_test(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__shareable_100_times(ilm_daemon):
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
        lock_op.timeout = 60000     # Timeout: 60s

        ret = ilm.ilm_lock(s, lock_id, lock_op)
        assert ret == 0

        ret = ilm.ilm_unlock(s, lock_id)
        assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__exclusive_smoke_test(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__exclusive_100_times(ilm_daemon):
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
        lock_op.timeout = 60000     # Timeout: 60s

        ret = ilm.ilm_lock(s, lock_id, lock_op)
        assert ret == 0

        ret = ilm.ilm_unlock(s, lock_id)
        assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__wrong_mode(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -22

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__one_host_exclusive_twice(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -16

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__one_host_shareable_twice(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -16

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__one_host_release_twice(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -1

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__two_hosts_exclusive_success_exclusive_fail(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

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


def test_lock__two_hosts_shareable_success_shareable_success(ilm_daemon):
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
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__two_hosts_shareable_success_exclusive_fail(ilm_daemon):
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
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_EXCLUSIVE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == -1

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == -1

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__two_hosts_exclusive_success_shareable_fail(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == -1

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == -1

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__two_hosts_exclusive_wrong_release_ahead(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == -1

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__two_hosts_exclusive_wrong_release_after(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == -1

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__convert_shareable_to_shareable(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_SHAREABLE)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__convert_shareable_to_exclusive(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__convert_exclusive_to_shareable(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_SHAREABLE)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__convert_exclusive_to_exclusive(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__two_hosts_convert_shareable_to_exclusive(ilm_daemon):
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
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_convert(s1, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -1

    ret = ilm.ilm_convert(s2, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -1

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__two_hosts_convert_shareable_to_exclusive_success(ilm_daemon):
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
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_convert(s1, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -1

    ret = ilm.ilm_convert(s2, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__timeout(ilm_daemon):
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
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    time.sleep(5)

    # Unlock will receive -ETIME
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -62

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__timeout_convert1(ilm_daemon):
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
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    time.sleep(5)

    # If the lock is timeout, cannot convert the mode;
    # will receive -ETIME
    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -62

    # Unlock will receive -EINVAL
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -22

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__timeout_convert2(ilm_daemon):
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
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    time.sleep(5)

    # If the lock is timeout, cannot convert the mode;
    # will receive -ETIME
    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_SHAREABLE)
    assert ret == -62

    # Unlock will receive -EINVAL
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -22

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__get_mode(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret, mode = ilm.ilm_get_mode(s, lock_id, lock_op)
    assert ret == 0
    assert mode == ilm.IDM_MODE_SHAREABLE

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret, mode = ilm.ilm_get_mode(s, lock_id, lock_op)
    assert ret == 0
    assert mode == ilm.IDM_MODE_EXCLUSIVE

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__get_mode_not_existed(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    lock_op = ilm.idm_lock_op()
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    # Return -ENOENT since the IDM has not been created
    ret, mode = ilm.ilm_get_mode(s, lock_id, lock_op)
    assert ret == -2

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__two_hosts_get_mode(ilm_daemon):
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
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret, mode = ilm.ilm_get_mode(s1, lock_id, lock_op)
    assert ret == 0
    assert mode == ilm.IDM_MODE_SHAREABLE

    # Before host2 acquires the lock, let's see if
    # can read out correct mode or not
    ret, mode = ilm.ilm_get_mode(s2, lock_id, lock_op)
    assert ret == 0
    assert mode == ilm.IDM_MODE_SHAREABLE

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    ret, mode = ilm.ilm_get_mode(s1, lock_id, lock_op)
    assert ret == 0
    assert mode == ilm.IDM_MODE_SHAREABLE

    ret, mode = ilm.ilm_get_mode(s2, lock_id, lock_op)
    assert ret == 0
    assert mode == ilm.IDM_MODE_SHAREABLE

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__get_host_count(ilm_daemon):
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
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s, lock_id, lock_op)
    assert ret == 0
    assert count == 1

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s, lock_id, lock_op)
    assert ret == 0
    assert count == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_lock__two_hosts_get_host_count(ilm_daemon):
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
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 2

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 1

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

def test_lock__three_hosts_get_host_count(ilm_daemon):
    ret, s1 = ilm.ilm_connect()
    assert ret == 0
    assert s1 > 0

    host_id = "00000000000000000000000000000000"
    ret = ilm.ilm_set_host_id(s1, host_id, 32)
    assert ret == 0

    ret, s2 = ilm.ilm_connect()
    assert ret == 0
    assert s2 > 0

    host_id = "11111111111111111111111111111111"
    ret = ilm.ilm_set_host_id(s2, host_id, 32)
    assert ret == 0

    ret, s3 = ilm.ilm_connect()
    assert ret == 0
    assert s3 > 0

    host_id = "22222222222222222222222222222222"
    ret = ilm.ilm_set_host_id(s3, host_id, 32)
    assert ret == 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid("0000000000000001")
    lock_id.set_lv_uuid("0123456789abcdef")

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, "/dev/sda1")
    lock_op.set_drive_names(1, "/dev/sda2")
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_lock(s1, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s2, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_lock(s3, lock_id, lock_op)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 2

    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 2

    # Duplicate unlocking, so received error
    ret = ilm.ilm_unlock(s2, lock_id)
    assert ret == -1

    # Check if the host count is 2 (s0 and s2 are granted)
    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 2

    ret = ilm.ilm_unlock(s3, lock_id)
    assert ret == 0

    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 1

    ret = ilm.ilm_unlock(s1, lock_id)
    assert ret == 0

    # All hosts have released the IDM, so return -ENOENT
    ret, count = ilm.ilm_get_host_count(s1, lock_id, lock_op)
    assert ret == 0
    assert count == 0

    ret = ilm.ilm_disconnect(s1)
    assert ret == 0

    ret = ilm.ilm_disconnect(s2)
    assert ret == 0

    ret = ilm.ilm_disconnect(s3)
    assert ret == 0
