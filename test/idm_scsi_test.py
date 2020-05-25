from __future__ import absolute_import

import errno
import io
import os
import time
import uuid

import pytest

import idm_scsi

def test_idm_version():
    ret, version = idm_scsi.idm_drive_version("/dev/sda1")
    assert ret == 0
    assert version == 0x100
