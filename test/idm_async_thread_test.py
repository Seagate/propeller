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

def test_async_nvme__result_struct():
    res = async_nvme.async_nvme_result()
    res.ret_status = 5
    assert res.ret_status == 5

def test_async_nvme__request_struct():
    req = async_nvme.async_nvme_request()
    req.fd = 9
    assert req.fd == 9

def test_async_nvme__request_struct_result():
    req = async_nvme.async_nvme_request()
    req.async_result.ret_status = 10
    assert req.fd == 10
