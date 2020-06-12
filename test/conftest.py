from __future__ import absolute_import

import os

import pytest

from . import ilm_util

import idm_scsi

DRIVE1 = "/dev/sg5"
DRIVE2 = "/dev/sg7"

@pytest.yield_fixture
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

    cmd = 'sg_raw -v -r 512 -o test_data.bin ' + DRIVE1 + ' 88 00 01 00 00 00 00 20 FF 01 00 00 00 01 00 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i test_data.bin ' + DRIVE1 + ' 8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i test_data.bin ' + DRIVE2 + ' 8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00'
    os.system(cmd)

    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE2);

def pytest_addoption(parser):
    parser.addoption('--run-destroy', action='store_true', dest="destroy",
                     default=False, help="enable destroy test")

def pytest_configure(config):
    if not config.option.destroy:
        setattr(config.option, 'markexpr', 'not destroy')
