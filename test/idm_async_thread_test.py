# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.

import time

import pytest

import async_nvme
from test_conf import *     # Normally bad practice, but only importing 'constants' here

def test_idm__async_nvme_cycle_pool():
    ret = async_nvme.thread_pool_init()
    time.sleep(1)
    assert ret == 0

    ret = async_nvme.thread_pool_destroy()
    time.sleep(1)
    assert ret == 0

def test_idm__async_nvme_pool_fixture(async_nvme_threads):
    time.sleep(1)
    assert async_nvme_threads == 0

