from __future__ import absolute_import

import os

import pytest

from . import ilm_util

import idm_scsi

DRIVE1 = "/dev/sg5"
DRIVE2 = "/dev/sg7"

@pytest.fixture
def ilm_daemon():
    """
    Run ILM daemon during a test.
    """
    p = ilm_util.start_daemon()
    try:
        ilm_util.wait_for_daemon(0.5)
        yield
    finally:
        # Killing sanlock allows terminating without reomving the lockspace,
        # which takes about 3 seconds, slowing down the tests.
        p.kill()
        p.wait()

@pytest.fixture
def idm_cleanup():
    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"
    lock_id1 = "0000000000000000000000000000000000000000000000000000000000000001"
    host_id0 = "00000000000000000000000000000000"
    host_id1 = "00000000000000000000000000000001"
    host_id2 = "00000000000000000000000000000002"

    a = idm_scsi.charArray(8)
    a[0] = 0
    a[1] = 0
    a[2] = 0
    a[3] = 0
    a[4] = 0
    a[5] = 0
    a[6] = 0
    a[7] = 0

    idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE1)
    idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE1)
    idm_scsi.idm_drive_unlock(lock_id0, host_id2, a, 8, DRIVE1)
    idm_scsi.idm_drive_unlock(lock_id0, host_id0, a, 8, DRIVE2)
    idm_scsi.idm_drive_unlock(lock_id0, host_id1, a, 8, DRIVE2)
    idm_scsi.idm_drive_unlock(lock_id0, host_id2, a, 8, DRIVE2)
    idm_scsi.idm_drive_unlock(lock_id1, host_id0, a, 8, DRIVE1)
    idm_scsi.idm_drive_unlock(lock_id1, host_id1, a, 8, DRIVE1)
    idm_scsi.idm_drive_unlock(lock_id1, host_id2, a, 8, DRIVE1)
    idm_scsi.idm_drive_unlock(lock_id1, host_id0, a, 8, DRIVE2)
    idm_scsi.idm_drive_unlock(lock_id1, host_id1, a, 8, DRIVE2)
    idm_scsi.idm_drive_unlock(lock_id1, host_id2, a, 8, DRIVE2)

def pytest_addoption(parser):
    parser.addoption('--run-destroy', action='store_true', dest="destroy",
                     default=False, help="enable destroy test")

def pytest_configure(config):
    if not config.option.destroy:
        setattr(config.option, 'markexpr', 'not destroy')
