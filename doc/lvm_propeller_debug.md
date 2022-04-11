# Introduction

This document describes how to debug Propeller and LVM on Centos7.

# Log

## Log level

lvmlockd and the IDM lock manager follow syslog's logging level
definition:

```
        LOG_EMERG       0       /* system is unusable */
        LOG_ALERT       1       /* action must be taken immediately */
        LOG_CRIT        2       /* critical conditions */
        LOG_ERR         3       /* error conditions */
        LOG_WARNING     4       /* warning conditions */
        LOG_NOTICE      5       /* normal but significant condition */
        LOG_INFO        6       /* informational */
        LOG_DEBUG       7       /* debug-level messages */
```

To simplify the logging system in the IDM lock manager, it supports
below three types:

        LOG_ERR         3       /* error conditions */
        LOG_WARNING     4       /* warning conditions */
        LOG_DEBUG       7       /* debug-level messages */

In the code, debug-level logging is used only for debugging purposes;
for a running daemon, it's suggested to enable warning or error
level logs only to avoid excessively verbose messages in the log.

## Log with syslog

Since lvmlockd and IDM lock manager can both output logging through syslog,
syslog is the central place to gather whole info for the system.

To enable the syslog for the IDM lock manager, the option '-S'
needs to be specified. '-S 7' means to output verbose log to syslog.

```
  seagate_ilm -l 0 -L 7 -E 7 -S 7
```

To enable the syslog for lvmlockd, the option '-S' need to specified,
'-S 7' means to output verblose log (include DEBUG level) to syslog.

```
  lvmlockd -g idm -S 7
```

After setting these options, we can read the syslog info from /var/log/message:

```
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 worker_thread_main: run_sanlock=0 run_dlm=0 run_idm=1
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 lvmlockd[2039]: 1593484286 partition name='propeller'
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sda1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sdb1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sdc1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sdd1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sde1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sdf1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sdg1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: ilm_find_deepest_device_mapping: Fail to read command buffer dmsetup deps -o devname /dev/sdh1
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: Two drives have same UUID?
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive path=/dev/sg3
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive UUID: c94b885a-fc7c-4254-b093-02e56b0ea1ee
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive path=/dev/sg3
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive UUID: c94b885a-fc7c-4254-b093-02e56b0ea1ee
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: Two drives have same UUID?
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive path=/dev/sg2
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive UUID: a0d7e838-18cf-4b92-a631-b67b125da218
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive path=/dev/sg2
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive UUID: a0d7e838-18cf-4b92-a631-b67b125da218
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: Two drives have same UUID?
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive path=/dev/sg4
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive UUID: cd1275ac-0b85-47e6-a1f9-98c223164491
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive path=/dev/sg4
  Jun 29 22:31:26 node2 seagate_ilm[2033]: 2020-06-29 22:31:26 710 [2045]: drive UUID: cd1275ac-0b85-47e6-a1f9-98c223164491
  [...]
```

## Log with file

The IDM lock manager uses '/run/seagate_ilm' as the default runtime directory.
By default, the IDM lock manager saves its log in the file:
/run/seagate_ilm/seagate_ilm.log. 

We can set the environment variable 'ILM_RUN_DIR' to specify IDM lock
manager's runtime directory:
```
  # export ILM_RUN_DIR=/tmp/seagate_ilm/
```

If set the runtime directory as '/tmp/seagate_ilm', the log will be
stored into file: /tmp/seagate_ilm/seagate_ilm.log. This is a good way
to separate the debugging work from standard operations.

For lvmlockd, we can dump the log file by using command 'lvmlockdctl':
```
  # lvmlockctl --dump
```

## Log with stderr

The IDM lock manager can output logs to stderr, so we need to enable its
'debug' mode and not run it in daemon mode:

```
  # seagate_ilm -D 1 -l 0 -L 7 -E 7 -S 7
```

lvmlockd can output logs to stderr, but it also needs to have 'debug' mode enabled
so it can output logs correctly:

```
  # lvmlockd -D -g idm -S 7
```

# Core dump

The core dump is not enabled by default on Centos7, we need to overwrite
the kernel 'core_pattern' setting:
```
  # echo /tmp/core-%e-sig%s-user%u-group%g-pid%p-time%t > /proc/sys/kernel/core_pattern
```

Or add the below line into the file /etc/sysctl.conf:
```
  kernel.core_pattern=/tmp/core-%e-sig%s-user%u-group%g-pid%p-time%t
```

# File descriptor leak

If the system reports the file descriptor exceeds the limiation, we can
use strace to check file descriptor leaking:
```
  # strace ./src/seagate_ilm -D 1 -l 0 -L 7 -E 7 -S 7
```

# Memory leak

TODO.
