# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019 Red Hat, Inc.
# Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
# Derived from the sanlock file of the same name.

import logging
import os

import pytest

from . import ilm_util

import idm_scsi
from test_conf import *     # Normally bad practice, but only importing 'constants' here

_logger = logging.getLogger(__name__)

BLK_DEVICES = [BLK_DEVICE1,
               BLK_DEVICE2,
               BLK_DEVICE3,
               BLK_DEVICE4,
               BLK_DEVICE5,
               BLK_DEVICE6,
               BLK_DEVICE7,
               BLK_DEVICE8]

@pytest.fixture(scope="session")
def ilm_daemon():
    _logger.info('ilm_daemon start')

    _init_devices()

    """
    Run ILM daemon during a test.
    """
    p = ilm_util.start_daemon()
    try:
        ilm_util.wait_for_daemon(0.5)
        yield
    finally:
        # Killing lock manager allows terminating without removing the lockspace.
        p.kill()
        p.wait()

@pytest.fixture(scope="session")
def idm_cleanup():
    _logger.info('idm_cleanup start')

    _init_devices()

def _init_devices():
    LOCK_ID0 ='0000000000000000000000000000000000000000000000000000000000000000'
    SG_CMD   = 'sg_raw -v -s 512 -i zero.bin'
    SG_DATA  = 'F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'

    os.system('dd if=/dev/zero of=zero.bin count=1 bs=512')

    for device in BLK_DEVICES:
        try:
            os.system(f'{SG_CMD} {device} {SG_DATA}')
        except Exception as e:
            _logger.error(f'{device} sg_raw failure')
            raise e from None

    for device in BLK_DEVICES:
        try:
            idm_scsi.idm_drive_lock_mode(LOCK_ID0, device)
        except Exception as e:
            _logger.error(f'{device} lock failure')
            raise e from None

def pytest_addoption(parser):
    parser.addoption('--run-destroy', action='store_true', dest="destroy",
                     default=False, help="enable destroy test")

def pytest_configure(config):
    if not config.option.destroy:
        setattr(config.option, 'markexpr', 'not destroy')
