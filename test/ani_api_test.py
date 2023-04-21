# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.

import time

import pytest

import ani_api
from test_conf import *     # Normally bad practice, but only importing 'constants' here


def test_ani__cycle_startup_manual():
    ret = ani_api.ani_init()
    assert ret == 0

    ani_api.ani_destroy()

def test_ani__cycle_startup_fixture(ani_api_startup):
    assert ani_api_startup == 0



#TODO: For now, save as examples of how to handle c-structs SWIG'd to python classes
# def test_async_nvme__result_struct():
#     res = async_nvme.async_nvme_result()
#     res.ret_status = 5
#     assert res.ret_status == 5

# def test_async_nvme__request_struct():
#     req = async_nvme.async_nvme_request()
#     req.fd = 9
#     assert req.fd == 9

# def test_async_nvme__request_struct_result():
#     req = async_nvme.async_nvme_request()
#     req.async_result.ret_status = 10
#     assert req.fd == 10
