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

def test_inject_fault__2_drives_lock(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__3_drives_lock(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 3
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 70)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 60)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 30)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__4_drives_lock(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 4
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.set_drive_names(3, BLK_DEVICE4)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 70)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 10)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__2_drives_unlock(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    # Return -EIO
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__3_drives_unlock(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 3
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    # Return -EIO
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -5

    # Return -EIO
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    # Return -EIO
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__4_drives_unlock(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 4
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.set_drive_names(3, BLK_DEVICE4)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    # Return -EIO
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -5

    # Return -EIO
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    # Return -EIO
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__2_drives_convert(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__3_drives_convert(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 3
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__4_drives_convert(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 4
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.set_drive_names(3, BLK_DEVICE4)
    lock_op.timeout = 60000     # Timeout: 60s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == -5

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_convert(s, lock_id, ilm.IDM_MODE_EXCLUSIVE)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_100_percent__1_drive_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 1
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_50_percent__1_drive_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 1
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_10_percent__1_drive_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 1
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__1_drive_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 1
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ## Test for 100 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 50 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    # Inject fault for renewal in lockspace
    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 0 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_100_percent__2_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_50_percent__2_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_10_percent__2_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__2_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 2
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ## Test for 100 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 50 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    # Inject fault for renewal in lockspace
    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 0 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_100_percent__3_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 3
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_50_percent__3_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 3
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_10_percent__3_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 3
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__3_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 3
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ## Test for 100 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 50 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    # Inject fault for renewal in lockspace
    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 0 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_100_percent__4_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 4
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.set_drive_names(3, BLK_DEVICE4)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_50_percent__4_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 4
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.set_drive_names(3, BLK_DEVICE4)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault_10_percent__4_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 4
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.set_drive_names(3, BLK_DEVICE4)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0

def test_inject_fault__4_drives_renew(ilm_daemon, reset_devices):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    lock_id = ilm.idm_lock_id()
    lock_id.set_vg_uuid(LOCK1_VG_UUID)
    lock_id.set_lv_uuid(LOCK1_LV_UUID)

    lock_op = ilm.idm_lock_op()
    lock_op.mode = ilm.IDM_MODE_SHAREABLE
    lock_op.drive_num = 4
    lock_op.set_drive_names(0, BLK_DEVICE1)
    lock_op.set_drive_names(1, BLK_DEVICE2)
    lock_op.set_drive_names(2, BLK_DEVICE3)
    lock_op.set_drive_names(3, BLK_DEVICE4)
    lock_op.timeout = 3000     # Timeout: 3s

    ret = ilm.ilm_inject_fault(s, 0) # Reset initial fault to 0
    assert ret == 0

    ## Test for 100 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 100)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 50 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    # Inject fault for renewal in lockspace
    ret = ilm.ilm_inject_fault(s, 50)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    # Return success after timeout
    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_start_renew(s)
    assert ret == 0


    ## Test for 0 percentage fault
    ret = ilm.ilm_lock(s, lock_id, lock_op)
    assert ret == 0

    ret = ilm.ilm_inject_fault(s, 0)
    assert ret == 0

    time.sleep(5)

    ret = ilm.ilm_stop_renew(s)
    assert ret == 0

    ret = ilm.ilm_unlock(s, lock_id)
    assert ret == 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0
