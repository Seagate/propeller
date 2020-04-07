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
