# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019 Red Hat, Inc.
# Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
# Derived from the sanlock file of the same name.


import os

import pytest

from . import ilm_util

import idm_scsi

DRIVE1 = "/dev/sg1"
DRIVE2 = "/dev/sg2"
DRIVE3 = "/dev/sg5"
DRIVE4 = "/dev/sg7"
DRIVE5 = "/dev/sg10"
DRIVE6 = "/dev/sg11"
DRIVE7 = "/dev/sg13"
DRIVE8 = "/dev/sg14"

@pytest.fixture(scope="session")
def ilm_daemon():
    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"

    cmd = 'dd if=/dev/zero of=zero.bin count=1 bs=512' 
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE1 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE2 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE3 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE4 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE5 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE6 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE7 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE8 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE2);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE3);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE4);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE5);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE6);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE7);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE8);

    """
    Run ILM daemon during a test.
    """
    p = ilm_util.start_daemon()
    try:
        ilm_util.wait_for_daemon(0.5)
        yield
    finally:
        # Killing lock manager allows terminating without reomving the lockspace.
        p.kill()
        p.wait()

@pytest.fixture(scope="session")
def idm_cleanup():
    lock_id0 = "0000000000000000000000000000000000000000000000000000000000000000"

    cmd = 'dd if=/dev/zero of=zero.bin count=1 bs=512' 
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE1 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE2 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE3 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE4 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE5 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE6 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE7 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    cmd = 'sg_raw -v -s 512 -i zero.bin ' + DRIVE8 + ' F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
    os.system(cmd)

    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE1);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE2);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE3);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE4);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE5);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE6);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE7);
    idm_scsi.idm_drive_lock_mode(lock_id0, DRIVE8);

def pytest_addoption(parser):
    parser.addoption('--run-destroy', action='store_true', dest="destroy",
                     default=False, help="enable destroy test")

def pytest_configure(config):
    if not config.option.destroy:
        setattr(config.option, 'markexpr', 'not destroy')
