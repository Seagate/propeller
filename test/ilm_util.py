from __future__ import absolute_import

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
           # Log level is LOG_DEBUG
           "-L", "7"]
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
