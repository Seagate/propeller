Propeller - LVM Locking scheme with Seagate IDM
===============================================

This repository provides the customized locking scheme based on Seagate
In-Drive-Mutex (IDM) and integration into lvmlockd.

This repository contains the IDM lock manager under 'src' folder; later
it can be extended to add IDM wrapper APIs lib and integrate with LVM2
for full stack releasing.

The library and APIs is implemented in C.

Dependency
----------

It depends below libs and utilities:

Install dependencies on Debian 18.04:

    $ sudo apt-get install build-essential
    $ sudo apt-get install libblkid-dev libblkid1
    $ sudo apt-get install uuid-dev
    $ sudo apt-get install python-pytest

Install dependencies on Centos 7:

    $ sudo yum groupinstall 'Development Tools'
    $ sudo yum install libuuid-devel
    $ sudo yum install libblkid libblkid-devel
    $ sudo yum install python-pytest

Building
--------

In this repository, there have three main parts to build:

- IDM lock manager and the lib which is in 'src' folder;
- Python wrapper for pytest in 'python' folder;
- Test cases in 'test' folder, especially, there have some C program
  code needs to build before run the test cases.

For simplify building steps, change to the root folder of Propeller and
execute 'make' commands, it will build all up three parts.

    $ cd /path/to/propeller
    $ make

Installation
------------

TODO

Enviornment Variables
---------------------

So far, only provides one variable 'ILM_RUN_DIR' which can be used to
set the runtime directory.  If without this variable, IDM lock manager
will use '/run/seagate_ilm' as default runtime directory.  Please note,
usually '/run/' and its child directories only can be accessed with
root, so other users have no permission to access it.  Thus, the normal
user without root permission can set variable with below command:

    $ export ILM_RUN_DIR=/tmp/seagate_ilm/

Logging
-------

IDM lock manager follows the syslog's logging level definition:

        LOG_EMERG       0       /* system is unusable */
        LOG_ALERT       1       /* action must be taken immediately */
        LOG_CRIT        2       /* critical conditions */
        LOG_ERR         3       /* error conditions */
        LOG_WARNING     4       /* warning conditions */
        LOG_NOTICE      5       /* normal but significant condition */
        LOG_INFO        6       /* informational */
        LOG_DEBUG       7       /* debug-level messages */

To simplify the logging system in IDM lock manager, now it supports
below three types:

        LOG_ERR         3       /* error conditions */
        LOG_WARNING     4       /* warning conditions */
        LOG_DEBUG       7       /* debug-level messages */

In the code, usually debug-level log is mainly used for debugging purpose;
for a running daemon, it's sugguested to only enable warning and error's
level log, this can avoid verbose messages.

The log will be written into three targets:

- stderr;
- log file $ILM_RUN_DIR/seagate_ilm.log;
- syslog.

Testing
-------

Let's divide testing into two different modes: manual mode and
automatic mode with py.test.

Before running test case, it's good to configure the environment so can
allow to access log files even without root permission.  Either for
manual mode or automatic mode, both suggest to configure the
environment variables before run any testing:

    $ cd /path/to/propeller/test
    $ . init_env.sh (or execute 'source init_env.sh')

Except to set run directory variable, the script 'init_env.sh' will
configure shell variables for lib path, python path.

Please be aware, if you want to launch two different shell windows to
execute testing, two shell windows are useful for debugging IDM lock
manager daemon with one shell window and another is dedicated for
client.  These two shell windows should run command
'. init_env.sh' to configure environment variables separately, the
script can only take effect for its own shell.

For manual testing mode, we can use below command to launch daemon:

    $ ./src/seagate_ilm -D 1 -l 0 -L 7 -E 7 -S 7

In this command, the options have below meanings:

  '-D': Enable debugging mode and don't invoke daemon() function, this
        allows the program to run as background process;

  '-l': If set, lockdown process's virtual address space into RAM;

  '-L': Log file priority (which is written into seagate_ilm.log)
        7 means to output all level's log (debug, warning, err);
        4 means to output warning and error level's log;
        3 means to output error level's log;

  '-E': Stderr log priority, the argument value is the same with '-L'.

  '-S': Syslog log priority, the argument value is the same with '-L'.

Up command manually launches IDM lock manager, it specifies the
arguments with 'enabling debugging mode, without lockdown VA, log file
priority is 7, stderr log priority is 7, syslog log priority is 7'.

With manual testing mode, it's useful for us to run some C program,
e.g. for the smoke testing:

    $ cd test
    $ ./smoke_test

For automatic testing mode, the command is straightforward:

    $ cd test

Run all cases, '-v' will output verbose logs

    $ py.test -v

The option '-t' specifies testing cases, in this example it only
executes cases with prefix 'test_lock'.

    $ py.test -v -t test_lock'

The option '--run-destroy' will enable an extra case for testing
IDM destroy.

    $ py.test -v --run-destroy

For test IDM SCSI wrapper APIs, it can use the command:

    $ py.test -v -k test_idm

For test IDM SCSI wrapper APIs with sync mode, use the command:

    $ py.test -v -k test_idm__sync

For test IDM SCSI wrapper APIs with async mode, use the command:

    $ py.test -v -k test_idm__async

For test without suppressing verbose log onn the console:

    $ py.test -v -k test_idm__async -s
