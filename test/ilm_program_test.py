# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.


import errno
import io
import os
import time
import uuid
import subprocess

import pytest

def test_killsignal(ilm_daemon):
    process = subprocess.Popen("./killsignal_test", shell=True, stdout=subprocess.PIPE)
    process.wait()
    assert process.returncode == 0

def test_killpath(ilm_daemon):
    process = subprocess.Popen("./killpath_test", shell=True, stdout=subprocess.PIPE)
    process.wait()
    assert process.returncode == 0

def test_multi_threads(ilm_daemon):
    process = subprocess.Popen("./stress_test", shell=True, stdout=subprocess.PIPE)
    process.wait()
    assert process.returncode == 0
