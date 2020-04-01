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

def test_lock(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.vg_uuid = "1111222233334444"
    lock_id.lv_uuid = "0123456789abcdef"

    lock_op = ilm.idm_lock_op()
    lock_op.mode = 0
    lock_op.drive_name = 2
    drive = lock_op.get_array_element(0)
    drive = "/dev/sda1"
    drive = lock_op.get_array_element(1)
    drive = "/dev/sda2"
    lock_op.timeout = 0
    lock_op.quiescent = 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0
