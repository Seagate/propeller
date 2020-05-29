from __future__ import absolute_import

import errno
import io
import os
import time
import uuid

import pytest

import idm_scsi

DRIVE1 = "/dev/sg5"
DRIVE2 = "/dev/sd7"

def test_idm_version(idm_cleanup):
    ret, version = idm_scsi.idm_drive_version(DRIVE1)
    assert ret == 0
    assert version == 0x100

def test_idm__sync_lock_exclusive(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_lock_shareable(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_lock_exclusive_twice(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == -11       # -EAGAIN

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == -2        # -ENOENT

def test_idm__sync_lock_shareable_twice(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == -11       # -EAGAIN

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == -2        # -ENOENT

def test_idm__sync_lock_exclusive_two_hosts(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id1, DRIVE1, 10000);
    assert ret == -16       # -EBUSY

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE1)
    assert ret == -2        # -ENOENT

def test_idm__sync_lock_shareable_two_hosts(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 10000);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_break_lock_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id1, DRIVE1, 5000);
    assert ret == -16       # -EBUSY

    time.sleep(10)

    ret = idm_scsi.idm_drive_break_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id1, DRIVE1, 5000);
    assert ret == 0         # Break successfully

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == -2        # -ENOENT

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_break_lock_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 5000);
    assert ret == -16       # -EBUSY

    time.sleep(7)

    ret = idm_scsi.idm_drive_break_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id1, DRIVE1, 5000);
    assert ret == 0         # Break successfully

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == -2        # -ENOENT

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_break_lock_3(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id1, DRIVE1, 5000);
    assert ret == -16       # -EBUSY

    time.sleep(7)

    ret = idm_scsi.idm_drive_break_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id1, DRIVE1, 10000);
    assert ret == 0         # Break successfully

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == -2        # -ENOENT

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_convert_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_convert_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                          host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_convert_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_convert_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                          host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_convert_3(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_convert_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                          host_id1, DRIVE1);
    assert ret == -1        # -EPERM

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_renew_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    time.sleep(5)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id0, DRIVE1);
    assert ret == 0

    time.sleep(5)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id0, DRIVE1);
    assert ret == 0

    time.sleep(5)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_renew_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    time.sleep(5)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id0, DRIVE1);
    assert ret == 0

    time.sleep(5)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id0, DRIVE1);
    assert ret == 0

    time.sleep(5)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_renew_timeout_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    time.sleep(7)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0         # -ETIME or 0

def test_idm__sync_renew_timeout_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    time.sleep(7)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0         # -ETIME or 0

def test_idm__sync_renew_timeout_3(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    time.sleep(7)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0         # -ETIME or 0

def test_idm__sync_renew_timeout_4(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    time.sleep(7)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id0, DRIVE1);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0         # -ETIME or 0

def test_idm__sync_read_lvb_1(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_read_lvb(lock_id0, host_id0, a, 8, DRIVE1);
    assert ret == 0
    assert ord(a[0]) == 0
    assert ord(a[1]) == 0
    assert ord(a[2]) == 0
    assert ord(a[3]) == 0
    assert ord(a[4]) == 0
    assert ord(a[5]) == 0
    assert ord(a[6]) == 0
    assert ord(a[7]) == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0         # -ETIME or 0

def test_idm__sync_read_lvb_2(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    a = idm_scsi.charArray(8)

    a[0] = 'a'
    a[1] = 'b'
    a[2] = 'c'
    a[3] = 'd'
    a[4] = 'e'
    a[5] = 'f'
    a[6] = 'g'
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    ret = idm_scsi.idm_drive_read_lvb(lock_id0, host_id0, a, 8, DRIVE1);
    assert ret == 0
    assert a[0] == 'a'
    assert a[1] == 'b'
    assert a[2] == 'c'
    assert a[3] == 'd'
    assert a[4] == 'e'
    assert a[5] == 'f'
    assert a[6] == 'g'
    assert ord(a[7]) == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_read_lvb_3(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    a = idm_scsi.charArray(8)
    a[0] = 1
    a[1] = 2
    a[2] = 3
    a[3] = 4
    a[4] = 1
    a[5] = 1
    a[6] = 1
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    ret = idm_scsi.idm_drive_read_lvb(lock_id0, host_id0, a, 8, DRIVE1);
    assert ret == 0
    assert ord(a[0]) == 1
    assert ord(a[1]) == 2
    assert ord(a[2]) == 3
    assert ord(a[3]) == 4
    assert ord(a[4]) == 1
    assert ord(a[5]) == 1
    assert ord(a[6]) == 1
    assert ord(a[7]) == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_get_host_count(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 0
    assert self == 1

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 0
    assert self == 0

def test_idm__sync_two_hosts_get_host_count(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"
    host_id2 = "00000000000000000000000000000002"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 10000);
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 1
    assert self == 1

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id1, DRIVE1);
    assert ret == 0
    assert count == 1
    assert self == 1

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id2, DRIVE1);
    assert ret == 0
    assert count == 2
    assert self == 0

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 0
    assert self == 0

def test_idm__sync_three_hosts_get_host_count(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"
    host_id2 = "00000000000000000000000000000002"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id2, DRIVE1, 10000);
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 2
    assert self == 1

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id1, DRIVE1);
    assert ret == 0
    assert count == 2
    assert self == 1

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id2, DRIVE1);
    assert ret == 0
    assert count == 2
    assert self == 1

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id2, a, 8)
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 0
    assert self == 0

def test_idm__sync_get_lock_mode(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, mode = idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_UNLOCK

def test_idm__sync_get_lock_exclusive_mode(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, mode = idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_EXCLUSIVE

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0

def test_idm__sync_get_lock_shareable_mode(idm_cleanup):

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, mode = idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_SHAREABLE

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    assert ret == 0
