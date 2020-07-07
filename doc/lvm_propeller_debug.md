# Introduction

This document is to describe how to debug Propeller/LVM on Centos7.

# Log

## Log level

lvmlockd and IDM lock manager follows the syslog's logging level
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

To simplify the logging system in IDM lock manager, now it supports
below three types:

        LOG_ERR         3       /* error conditions */
        LOG_WARNING     4       /* warning conditions */
        LOG_DEBUG       7       /* debug-level messages */

In the code, usually debug-level log is mainly used for debugging purpose;
for a running daemon, it's sugguested to only enable warning and error's
level log, this can avoid verbose messages.

## Log with syslog

Since lvmlockd and IDM lock manager both can output log through syslog,
so syslog will be a central place to gather whole info.

To enable the syslog for Seagate IDM lock manager, the option '-S'
needs to be specified, '-S 7' means to output verbose log to syslog.

```
  seagate_ilm -l 0 -L 7 -E 7 -S 7
```

To enable the syslog for lvmlockd, the option '-S' need to specified,
'-S 7' means to output verblose log (include DEBUG level) to syslog.

```
  lvmlockd -g idm -S 7
```

Finally, we can read the syslog info from /var/log/message:

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

IDM lock manager uses '/run/seagate_ilm' as default runtime directory.
By default, Seagate IDM lock manager saves its log in the file:
/run/seagate_ilm/seagate_ilm.log.

We can set the envoirnment variable 'ILM_RUN_DIR' to specify IDM lock
manager's runtime directory:
```
  # export ILM_RUN_DIR=/tmp/seagate_ilm/
```

If set the runtime directory as '/tmp/seagate_ilm', the log will be
stored into file: /tmp/seagate_ilm/seagate_ilm.log.

For lvmlockd, we can dump the log file by using command 'lvmlockdctl':
```
  # lvmlockctl --dump
```

## Log with stderr

IDM lock manager can output logs to stderr, so we need to enable its
'debug' mode and don't run as daemon mode:

```
  # seagate_ilm -D 1 -l 0 -L 7 -E 7 -S 7
```

lvmlockd can output logs to stderr, it also need to enable 'debug' mode
so can output logs to stderr:

```
  # lvmlockd -D -g idm -S 7
```

# Core dump

The core dump is not enabled by default on Centos7, we need to overwrite
the kernel 'core_pattern' setting:
```
  # echo /tmp/core-%e-sig%s-user%u-group%g-pid%p-time%t > /proc/sys/kernel/core_pattern
```

Or add below line into the file /etc/sysctl.conf:
```
  kernel.core_pattern=/tmp/core-%e-sig%s-user%u-group%g-pid%p-time%t
```

# File descriptor leak

If the system reports the file descriptor exceeds the limiation, can simply
use strace to check file descriptor leaking:
```
  # strace ./src/seagate_ilm -D 1 -l 0 -L 7 -E 7 -S 7
```

# Memory leak

TODO.
