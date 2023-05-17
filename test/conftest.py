# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019 Red Hat, Inc.
# Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates.
# Derived from the sanlock file of the same name.

import logging
import os
import sys

import pytest

from . import ilm_util

import idm_api
from test_conf import *     # Normally bad practice, but only importing 'constants' here

# Logger
_logger = logging.getLogger(__name__)
_logger.setLevel(logging.DEBUG)
handler = logging.StreamHandler(sys.stdout)
handler.setLevel(logging.DEBUG)
_logger.addHandler(handler)

# Globals
RAW_DEVICES = [SG_DEVICE1,
               SG_DEVICE2,
               SG_DEVICE3,
               SG_DEVICE4,
               SG_DEVICE5,
               SG_DEVICE6,
               SG_DEVICE7,
               SG_DEVICE8]
# List of active devices to be put under test.  Filled each time tests are run.
test_devices = []

# Command-related strings for hard resetting drives
SG_CMD    = 'sg_raw -v -s 512 -i zero.bin'
SG_DATA   = 'F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00'
NVME_CMD  = 'nvme admin-passthru'
NVME_DATA = '--opcode=0xC1 --data-len=512 --write --cdw12=0x0000FF00 --cdw10=0x00000080 --input-file=zero.bin'


###############################################################################
# PYTEST TEST FIXTURES
###############################################################################
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

"""
Fixture for manually creating, yielding and then destroying the
async nvme interface (ANI), which is the custom NVMe IDM async code.
"""
@pytest.fixture(scope="module")
def idm_async_daemon():

    _logger.info("fixture start: idm_async_daemon")

    _init_devices()

    try:
        idm_api.idm_environ_init()
    except Exception as e:
        _logger.error(f'idm_environ_init failure:{e}')
        raise e from None

    yield

    try:
        idm_api.idm_environ_destroy()
    except Exception as e:
        _logger.error(f'idm_environ_destroy failure:{e}')
        raise e from None

@pytest.fixture(scope="module")
def idm_sync_daemon():

    _logger.info("fixture start: idm_sync_daemon")

    _init_devices()

@pytest.fixture(scope="function")
def reset_devices():

    _logger.info("fixture start: reset_devices")

    _reset_devices()

###############################################################################
# HELPER FUNCTIONS
###############################################################################
def _init_devices():

    _logger.info("_init_devices start")

    # Find test devices
    test_devices.clear()
    for device in RAW_DEVICES:
        try:
            with open(device, "r") as _:
                _logger.info(f'test device found: {device}')
                test_devices.append(device)
        except IOError:
            continue

     # Needed by _reset_drives() but only need to run once.
    os.system('dd if=/dev/zero of=zero.bin count=1 bs=512')

    _reset_devices()

    LOCK_ID0 ='0000000000000000000000000000000000000000000000000000000000000000'
    for device in test_devices:
        try:
            idm_api.idm_drive_lock_mode(LOCK_ID0, device)
        except Exception as e:
            _logger.error(f'{device} read lock mode failure')
            raise e from None

def _reset_devices():

    _logger.info("_reset_devices start")

    for device in test_devices:
        try:
            if device.find('nvme') >= 0:
                os.system(f'{NVME_CMD} {device} {NVME_DATA}')
            else:
                os.system(f'{SG_CMD} {device} {SG_DATA}')
        except Exception as e:
            _logger.error(f'reset failure: {device}')
            raise e from None

###############################################################################
# PYTEST HOOKS
###############################################################################
def pytest_addoption(parser):
    parser.addoption('--run-destroy', action='store_true', dest="destroy",
                     default=False, help="enable destroy test")

def pytest_configure(config):
    if not config.option.destroy:
        setattr(config.option, 'markexpr', 'not destroy')
