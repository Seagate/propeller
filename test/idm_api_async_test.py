# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.


import errno
import io
import os
import time
import uuid
import select

import pytest

import idm_api
from test_conf import *     # Normally bad practice, but only importing 'constants' here

def wait_for_scsi_response(device, handle):
    sg_fd = idm_api.idm_drive_get_fd(device, handle)
    assert sg_fd >= 0

    poll = select.poll()

    poll.register(sg_fd, select.POLLIN)

    fd_event_list = poll.poll(2)

    for fd, event in fd_event_list:
        assert sg_fd == fd

    poll.unregister(sg_fd)

def test_idm__async_lock_exclusive(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0
    assert handle != 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_lock_shareable(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0
    assert handle != 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_lock_exclusive_two_drives(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle1 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                 host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                 host_id0, SG_DEVICE2, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE2, handle2)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle1 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                   host_id0, a, 8, SG_DEVICE2)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE2, handle2)
    assert ret == 0
    assert result == 0

def test_idm__async_lock_shareable_two_drives(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle1 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id0, SG_DEVICE2, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE2, handle2)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle1 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id0, a, 8, SG_DEVICE2)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE2, handle2)
    assert ret == 0
    assert result == 0

def test_idm__async_lock_shareable_two_locks(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    lock_id1 = "0000000000000000000000000000000000000000000000000000000000000001"
    host_id0 = "00000000000000000000000000000000"

    ret, handle1 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_lock_async(lock_id1, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle1 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_unlock_async(lock_id1, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == 0

def test_idm__async_break_lock_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                 host_id0, SG_DEVICE1, 5000)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                 host_id1, SG_DEVICE1, 5000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == -16    # -EBUSY

    time.sleep(7)

    ret, handle = idm_api.idm_drive_break_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                      host_id1, SG_DEVICE1, 5000)
    assert ret == 0         # Break successfully

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle1 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                   host_id1, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == -2	     # -ENOENT

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == 0

def test_idm__async_break_lock_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                 host_id0, SG_DEVICE1, 5000)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id1, SG_DEVICE1, 5000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == -16    # -EBUSY

    time.sleep(7)

    ret, handle = idm_api.idm_drive_break_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                      host_id1, SG_DEVICE1, 5000)
    assert ret == 0         # Break successfully

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle1 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id1, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == -2

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == 0

def test_idm__async_break_lock_3(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id0, SG_DEVICE1, 5000)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                 host_id1, SG_DEVICE1, 5000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == -16    # -EBUSY

    time.sleep(7)

    ret, handle = idm_api.idm_drive_break_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                      host_id1, SG_DEVICE1, 5000)
    assert ret == 0         # Break successfully

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle1 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id1, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == -2        # -ENOENT

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == 0

def test_idm__async_convert_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_convert_lock_async(lock_id0,
                        idm_api.IDM_MODE_EXCLUSIVE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_convert_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_convert_lock_async(lock_id0,
                        idm_api.IDM_MODE_SHAREABLE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_convert_3(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                 host_id1, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_convert_lock_async(lock_id0,
		    idm_api.IDM_MODE_EXCLUSIVE, host_id1, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == -1        # -EPERM

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle1 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    ret, handle2 = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                   host_id1, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle1)
    wait_for_scsi_response(SG_DEVICE2, handle2)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle2)
    assert ret == 0
    assert result == 0

def test_idm__async_renew_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_SHAREABLE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_SHAREABLE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_SHAREABLE, host_id0, SG_DEVICE1, 10000);
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_renew_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_EXCLUSIVE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_EXCLUSIVE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_EXCLUSIVE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_renew_timout_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(7)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_SHAREABLE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0        # -ETIME or 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0          # -ETIME or 0

def test_idm__async_renew_timeout_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    time.sleep(7)

    ret, handle = idm_api.idm_drive_renew_lock_async(lock_id0,
                        idm_api.IDM_MODE_EXCLUSIVE, host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0          # -ETIME or 0

def test_idm__async_read_lvb_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)

    ret, handle = idm_api.idm_drive_read_lvb_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_read_lvb_async_result(SG_DEVICE1, handle, a, 8)
    assert ret == 0
    assert result == 0

    assert ord(a[0]) == 0
    assert ord(a[1]) == 0
    assert ord(a[2]) == 0
    assert ord(a[3]) == 0
    assert ord(a[4]) == 0
    assert ord(a[5]) == 0
    assert ord(a[6]) == 0
    assert ord(a[7]) == 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_read_lvb_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    a = idm_api.charArray(8)

    ret, handle = idm_api.idm_drive_read_lvb_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_read_lvb_async_result(SG_DEVICE1, handle, a, 8)
    assert ret == 0
    assert result == 0

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

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_read_lvb_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_read_lvb_async_result(SG_DEVICE1, handle, a, 8)
    assert ret == 0
    assert result == 0

    assert a[0] == 'a'
    assert a[1] == 'b'
    assert a[2] == 'c'
    assert a[3] == 'd'
    assert a[4] == 'e'
    assert a[5] == 'f'
    assert a[6] == 'g'
    assert ord(a[7]) == 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_get_host_count(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 1

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 0

def test_idm__async_two_hosts_get_host_count(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"
    host_id2 = "00000000000000000000000000000002"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id1, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 1
    assert self == 1

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id1, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 1
    assert self == 1

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id2, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id1, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 0

def test_idm__async_three_hosts_get_host_count(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"
    host_id2 = "00000000000000000000000000000002"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id1, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id2, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 1

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id1, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 1

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id2, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 1

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id1, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id2, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_count_async(lock_id0, host_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, count, self, result = idm_api.idm_drive_lock_count_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 0

def test_idm__async_get_lock_mode(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_mode_async(lock_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, mode, result = idm_api.idm_drive_lock_mode_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert mode == idm_api.IDM_MODE_UNLOCK
    assert result == 0

def test_idm__async_get_lock_exclusive_mode(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_mode_async(lock_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, mode, result = idm_api.idm_drive_lock_mode_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert mode == idm_api.IDM_MODE_EXCLUSIVE
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_EXCLUSIVE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

def test_idm__async_get_lock_shareable_mode(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_api.idm_drive_lock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                host_id0, SG_DEVICE1, 10000)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_api.idm_drive_lock_mode_async(lock_id0, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, mode, result = idm_api.idm_drive_lock_mode_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert mode == idm_api.IDM_MODE_SHAREABLE
    assert result == 0

    a = idm_api.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret, handle = idm_api.idm_drive_unlock_async(lock_id0, idm_api.IDM_MODE_SHAREABLE,
                                                  host_id0, a, 8, SG_DEVICE1)
    assert ret == 0

    wait_for_scsi_response(SG_DEVICE1, handle)

    ret, result = idm_api.idm_drive_async_result(SG_DEVICE1, handle)
    assert ret == 0
    assert result == 0
