from __future__ import absolute_import

import errno
import io
import os
import time
import uuid

import pytest

import idm_scsi

DRIVE1 = "/dev/sda1"
DRIVE2 = "/dev/sda2"

def wait_for_scsi_response(handle):
    sg_fd = idm_scsi.idm_drive_get_fd(handle)
    assert sg_fd >= 0

    poll.register(sg_fd)

    fd_event_list = poll.poll(2)

    for fd, event in fd_event_list:
        assert sg_fd == fd

    poll.unregister(sg_fd)

def wait_for_two_drive_scsi_response(handle1, handle2):
    sg_fd1 = idm_scsi.idm_drive_get_fd(handle1)
    assert sg_fd1 >= 0

    sg_fd2 = idm_scsi.idm_drive_get_fd(handle2)
    assert sg_fd2 >= 0

    poll.register(sg_fd1)
    poll.register(sg_fd2)

    while(True):
        fd_event_list = poll.poll(2)

        if len(my_list) == 0:
            break

        for fd, event in fd_event_list:
            assert (sg_fd1 == fd) or (sg_fd2 == fd)

    poll.unregister(sg_fd1)
    poll.unregister(sg_fd2)

def test_idm_lock_exclusive_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_lock_shareable_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_lock_exclusive_two_drives_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle1 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                 host_id0, DRIVE2, 10000);
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle1 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

def test_idm_lock_shareable_two_drives_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle1 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id0, DRIVE2, 10000);
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle1 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

def test_idm_lock_shareable_two_locks_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000001"
    host_id0 = "00000000000000000000000000000000"

    ret, handle1 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_lock_async(lock_id1, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle1 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

def test_idm_break_lock_1_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                 host_id1, DRIVE1, 10000);
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == -16    # -EBUSY

    ret, handle = idm_scsi.idm_drive_break_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                      host_id1, DRIVE1, 10000);
    assert ret == 0         # Break successfully

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle1 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert ret == -2        # -ENOENT

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

def test_idm_break_lock_2_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id1, DRIVE1, 10000);
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == -16    # -EBUSY

    ret, handle = idm_scsi.idm_drive_break_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                      host_id1, DRIVE1, 10000);
    assert ret == 0         # Break successfully

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle1 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert ret == -2        # -ENOENT

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

def test_idm_break_lock_3_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                 host_id1, DRIVE1, 10000);
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == -16    # -EBUSY

    ret, handle = idm_scsi.idm_drive_break_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                      host_id1, DRIVE1, 10000);
    assert ret == 0         # Break successfully

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle1 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert ret == -2        # -ENOENT

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

def test_idm_convert_1_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_convert_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_EXCLUSIVE, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_convert_2_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_convert_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_SHAREABLE, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_convert_3_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret, handle1 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                 host_id1, DRIVE1, 10000);
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_convert_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                  host_id1, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == -1        # -EPERM

    a = ilm.charArray(8)

    ret, handle1 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, handle2 = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_two_drive_scsi_response(handle1, handle2)

    ret, result = idm_scsi.idm_drive_async_result(handle1)
    assert ret == 0
    assert result == 0

    ret, result = idm_scsi.idm_drive_async_result(handle2)
    assert ret == 0
    assert result == 0

def test_idm_renew_1_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_SHAREABLE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_SHAREABLE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_SHAREABLE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_renew_2_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_EXCLUSIVE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_EXCLUSIVE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(5)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_EXCLUSIVE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_renew_fail_1_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(7)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_SHAREABLE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == -62        # -ETIME or 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0          # -ETIME or 0

def test_idm_renew_fail_2_async():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    time.sleep(7)

    ret, handle = idm_scsi.idm_drive_renew_lock_async(lock_id0,
                        idm_scsi.IDM_MODE_EXCLUSIVE, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == -62        # -ETIME or 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0          # -ETIME or 0

def test_idm_read_lvb_1():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_read_lvb_async(lock_id0, host_id0, DRIVE1, a, 8);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_read_lvb_async_result(handle, a, 8)
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

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_read_lvb_2():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_read_lvb_async(lock_id0, host_id0, DRIVE1, a, 8);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_read_lvb_async_result(handle, a, 8)
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

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_read_lvb_async(lock_id0, host_id0, DRIVE1, a, 8);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_read_lvb_async_result(handle, a, 8)
    assert ret == 0
    assert result == 0

    assert b[0] == 'a'
    assert b[1] == 'b'
    assert b[2] == 'c'
    assert b[3] == 'd'
    assert b[4] == 'e'
    assert b[5] == 'f'
    assert b[6] == 'g'
    assert b[7] == 'h'

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_get_host_count():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 1

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 0

def test_idm_two_hosts_get_host_count():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"
    host_id2 = "00000000000000000000000000000002"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id1, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 1
    assert self == 1

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id1, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 1
    assert self == 1

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id2, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 0

def test_idm_three_hosts_get_host_count():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"
    host_id2 = "00000000000000000000000000000002"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id1, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id2, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 1

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id1, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 1

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id2, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 2
    assert self == 1

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id1, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id2, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_count_async(lock_id0, host_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, count, self, result = idm_scsi.idm_drive_lock_count_async_result(handle);
    assert ret == 0
    assert result == 0
    assert count == 0
    assert self == 0

def test_idm_get_lock_mode():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_mode_async(lock_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, mode, result = idm_scsi.idm_drive_lock_mode_async_result(handle)
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_UNLOCKED
    assert result == 0

def test_idm_get_lock_exclusive_mode():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_mode_async(lock_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, mode, result = idm_scsi.idm_drive_lock_mode_async_result(handle)
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_EXCLUSIVE
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

def test_idm_get_lock_shareable_mode():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, handle = idm_scsi.idm_drive_lock_async(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                                host_id0, DRIVE1, 10000);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0

    ret, handle = idm_scsi.idm_drive_lock_mode_async(lock_id0, DRIVE1);
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, mode, result = idm_scsi.idm_drive_lock_mode_async_result(handle)
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_SHAREABLE
    assert result == 0

    a = ilm.charArray(8)

    ret, handle = idm_scsi.idm_drive_unlock_async(lock_id0, host_id0, a, 8)
    assert ret == 0

    wait_for_scsi_response(handle)

    ret, result = idm_scsi.idm_drive_async_result(handle)
    assert ret == 0
    assert result == 0
