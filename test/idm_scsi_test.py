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

def test_idm_version():
    ret, version = idm_scsi.idm_drive_version(DRIVE1)
    assert ret == 0
    assert version == 0x100

def test_idm_lock_exclusive():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_lock_shareable():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_lock_exclusive_twice():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == -11       # -EAGAIN

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == -2        # -ENOENT

def test_idm_lock_shareable_twice():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == -11       # -EAGAIN

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == -2        # -ENOENT

def test_idm_lock_exclusive_two_hosts():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id1, DRIVE1, 10000);
    assert ret == -16       # -EBUSY

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == -2        # -ENOENT

def test_idm_lock_shareable_two_hosts():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 10000);
    assert ret == 0

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

def test_idm_break_lock_1():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id1, DRIVE1, 10000);
    assert ret == -16       # -EBUSY

    ret = idm_scsi.idm_drive_break_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id1, DRIVE1, 10000);
    assert ret == 0         # Break successfully

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == -2        # -ENOENT

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

def test_idm_break_lock_2():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 10000);
    assert ret == -16       # -EBUSY

    ret = idm_scsi.idm_drive_break_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id1, DRIVE1, 10000);
    assert ret == 0         # Break successfully

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == -2        # -ENOENT

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

def test_idm_break_lock_3():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id1, DRIVE1, 10000);
    assert ret == -16       # -EBUSY

    ret = idm_scsi.idm_drive_break_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id1, DRIVE1, 10000);
    assert ret == 0         # Break successfully

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == -2        # -ENOENT

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

def test_idm_convert_1():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_convert_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                          DRIVE1);
    assert ret == 0

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_convert_2():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_convert_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                          DRIVE1);
    assert ret == 0

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_convert_3():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id1, DRIVE1, 10000);
    assert ret == 0

    ret = idm_scsi.idm_drive_break_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id1, DRIVE1, 10000);
    assert ret == -1        # -EPERM

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

def test_idm_renew_1():

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

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_renew_2():

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

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_renew_fail_1():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    time.sleep(7)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                        host_id0, DRIVE1);
    assert ret == -62       # -ETIME or 0

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0         # -ETIME or 0

def test_idm_renew_fail_2():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    time.sleep(7)

    ret = idm_scsi.idm_drive_renew_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                        host_id0, DRIVE1);
    assert ret == -62       # -ETIME or 0

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0         # -ETIME or 0

def test_idm_read_lvb_1():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    a = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(lock_id0, host_id0, DRIVE1, a, 8);
    assert ret == 0
    assert ord(a[0]) == 0
    assert ord(a[1]) == 0
    assert ord(a[2]) == 0
    assert ord(a[3]) == 0
    assert ord(a[4]) == 0
    assert ord(a[5]) == 0
    assert ord(a[6]) == 0
    assert ord(a[7]) == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0         # -ETIME or 0

def test_idm_read_lvb_2():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    a = ilm.charArray(8)

    ret = ilm.ilm_read_lvb(lock_id0, host_id0, DRIVE1, a, 8);
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

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 5000);
    assert ret == 0

    ret = ilm.ilm_read_lvb(lock_id0, host_id0, DRIVE1, a, 8);
    assert ret == 0
    assert b[0] == 'a'
    assert b[1] == 'b'
    assert b[2] == 'c'
    assert b[3] == 'd'
    assert b[4] == 'e'
    assert b[5] == 'f'
    assert b[6] == 'g'
    assert b[7] == 'h'

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_get_host_count():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 0
    assert self == 1

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 0
    assert self == 0

def test_idm_two_hosts_get_host_count():

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

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8)
    assert ret == 0

    ret, count, self = idm_scsi.idm_drive_lock_count(lock_id0, host_id0, DRIVE1);
    assert ret == 0
    assert count == 0
    assert self == 0

def test_idm_three_hosts_get_host_count():

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

    a = ilm.charArray(8)

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

def test_idm_get_lock_mode():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret, mode = idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_UNLOCKED

def test_idm_get_lock_exclusive_mode():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_EXCLUSIVE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, mode = idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_EXCLUSIVE

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0

def test_idm_get_lock_shareable_mode():

    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    host_id0 = "00000000000000000000000000000000"

    ret = idm_scsi.idm_drive_lock(lock_id0, idm_scsi.IDM_MODE_SHAREABLE,
                                  host_id0, DRIVE1, 10000);
    assert ret == 0

    ret, mode = idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    assert ret == 0
    assert mode == idm_scsi.IDM_MODE_SHAREABLE

    a = ilm.charArray(8)

    ret = idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8)
    assert ret == 0
