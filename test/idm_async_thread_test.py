# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.

import time

import pytest

import idm_api
from test_conf import *     # Normally bad practice, but only importing 'constants' here

def test_idm__async_threads_running1(nvme_async_threads):
    time.sleep(1)

def test_idm__async_threads_running2(nvme_async_threads):
    time.sleep(1)
