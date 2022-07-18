# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2019 Red Hat, Inc.
# Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
# Derived from the sanlock file test/util.py

import errno
import io
import os
import socket
import struct
import subprocess
import time

TESTDIR = os.path.dirname(__file__)
ILM = os.path.join(TESTDIR, os.pardir, "src", "seagate_ilm")

class TimeoutExpired(Exception):
    """ Exception raised for timeout expired """

def start_daemon():
    cmd = [ILM,
           # don't set daemon so print log to stderr
           "-D", "1",
           # don't use mlockall
           "-l", "0",
           # Log level is LOG_WARNING for log file
           "-L", "4",
           # stderr level is disabled
           "-E", "0",]
    return subprocess.Popen(cmd)

def wait_for_daemon(timeout):
    """
    Wait until deamon is accepting connections
    """
    deadline = time.time() + timeout
    path = os.path.join(os.environ["ILM_RUN_DIR"], "main.sock")
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        while True:
            try:
                s.connect(path)
                return
            except EnvironmentError as e:
                if e.errno not in (errno.ECONNREFUSED, errno.ENOENT):
                    raise  # Unexpected error
            if time.time() > deadline:
                raise TimeoutExpired
            time.sleep(0.05)
    finally:
        s.close()
