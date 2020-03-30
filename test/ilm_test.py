from __future__ import absolute_import

import errno
import io
import os
import time

import pytest

import ilm

def test_lockspace(ilm_daemon):
    ret, s = ilm.ilm_connect()
    assert ret == 0
    assert s > 0

    ret = ilm.ilm_disconnect(s)
    assert ret == 0
