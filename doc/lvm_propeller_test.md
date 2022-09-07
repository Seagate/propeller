# Introduction

This document describes testing Propeller and LVM
on Centos7.

# Test for IDM SCSI wrapper and IDM lock manager

Testing is divided into two different modes: manual mode and
automatic mode using **pytest** (or **py.test**).

## Test setup

### Environment
Before running any test cases, you will need to configure the environment.
Whether you are using manua; or automatic testing,
this will configure the environment variables:

    $ cd /path/to/propeller/test
    $ source init_env.sh

The script 'init_env.sh' will configure shell variables for the lib and python path, but not the run directory.

Please be aware, if you want to launch two different shell windows to
execute testing, you will need to run 'source init_env.sh' separately in both shell windows.
Two shell windows are useful for debugging the IDM lock
manager daemon with one for the lock manager and another for the client.

### Daemon
For manual testing, we can use the below command to launch the daemon:

    $ ./src/seagate_ilm -D 1 -l 0 -L 7 -E 7 -S 7

The IDM locking manager has several options that can be modified:

  '-D': This enables debugging mode and doesn't invoke the daemon() function,
        allowing the program to run as a background process;

  '-l': If set to 1, this will lockdown the process's virtual address space into RAM;

  '-L': This sets the log file priority (written into seagate_ilm.log)
        7 outputs all log levels (debug, warning, err);
        4 outputs warning and error level logs;
        3 outputs error log level only;

  '-E': Stderr log priority, which accepts the same arguments as '-L'.

  '-S': Syslog log priority, which accepts the same arguments as '-L'.

The above command manually launches IDM lock manager, specifying the following
arguments: 'enabling debugging mode, without lockdown virtual addressing, log file
priority is 7, stderr log priority is 7, and syslog log priority is 7'.

### Unit test settings
Currently, there are 2 files that have variable settings used throughout the unit test code.
    - test/test_conf.h
    - test/test_conf.py

These 2 files are configuration settings used throughout the C code and the Python
code, respectively.  (Note: These settings need to be refactored into a single JSON file)

The most important settings to configure are the block device (BLK_DEVICEX) and
scsi generic device (SG_DEVICEX) system paths.
Make sure that these constant configuration settings are correct before running any
unit tests.  Other drives on the system can be damaged if these setting are not configured
correctly.

## Test examples

When manually testing the lock manager, it's useful to run the smoke test
before running more strenuous tests:

    $ cd test
    $ ./smoke_test

When automatically testing, pytest is used.
The command below will run all tests, the'-v' flag specifying verbose log output:

    $ cd test
    $ python3 -m pytest -v

For testing a single unit test python module (a single .py file) by
itself, you can use:

    $ python3 -m pytest -v ilm_inject_fault_test.py

The option '-k' specifies testing cases, in this example it will only
execute test cases with the prefix 'test_lock'.

    $ python3 -m pytest -v -k test_lock'

The option -k can also be used to ignore certain test cases.  For example, if
you wanted to only run tests that involved 2 or less drive, we'd have to ignore
all the tests that interact with 3+ drives.

    $ python3 -m pytest -v -k "not 3_drive and not 4_drive"

Further, if you only have 1 host, you'd have to also disable the 2 and 3 host unit tests

    $ python3 -m pytest -v -k "not 3_drive and not 4_drive and not two_host and not three_host"

The option '--run-destroy' will enable an extra case for testing
IDM destroy.

    $ python3 -m pytest -v --run-destroy

For testing IDM SCSI wrapper APIs, you can use:

    $ python3 -m pytest -v -k test_idm

For testing IDM SCSI wrapper APIs in sync mode, you can use:

    $ python3 -m pytest -v -k test_idm__sync

For testing IDM SCSI wrapper APIs in async mode, you can use:

    $ python3 -m pytest -v -k test_idm__async

For testing without suppressing verbose console log:

    $ python3 -m pytest -v -k test_idm__async -s
## Test debug tracing

The section is about adding trace debug messages within the unit test methods themselves.

Using print() does not appear to work with this unit test setup.  The location
of the print() output could not be found.
So, alternatively, the python logging module was used.

At the top of the module, create the logger object.
```
  from logging import Logger
  _logger = Logger.getLogger(__name__)
```

Then add the trace messages where desired.  Below are several example tracing options.
```
def test_idm_version(idm_cleanup):
    _logger.info('test_idm_version info')
    _logger.debug('test_idm_version debug')
    _logger.warning('test_idm_version warning')
    _logger.error('test_idm_version error')
    _logger.critical('test_idm_version critical')
    ret, version = idm_scsi.idm_drive_version(DRIVE1)
    assert ret == 0
    assert version == 0x100
```

However, to activate this output, the user must use the pytest --log_cli* options during
 cli invocation.

    $ python3 -m pytest -v -k test_idm_version --log-cli-level=0

See https://docs.pytest.org/ for more details.


# Test for LVM tool

## Preparation

### Create disk partitions

Let's use an example system that has four block devices with IDM firmware:
/dev/sdi, /dev/sdj, /dev/sdk, /dev/sdm.

The partitions can be created with the below script:

```
  parted -s /dev/sdi mklabel gpt mkpart propeller ext4 2048s 1048576s
  parted -s /dev/sdi mkpart primary 1050624s 420481023s
  parted -s /dev/sdi mkpart primary 420481024s 839911423s
  parted -s /dev/sdi mkpart primary 839911424s 1259341823s
  parted -s /dev/sdi mkpart logical 1259341824s 1678772223s

  parted -s /dev/sdj mklabel gpt mkpart propeller ext4 2048s 1048576s
  parted -s /dev/sdj mkpart primary 1050624s 420481023s
  parted -s /dev/sdj mkpart primary 420481024s 839911423s
  parted -s /dev/sdj mkpart primary 839911424s 1259341823s
  parted -s /dev/sdj mkpart logical 1259341824s 1678772223s

  parted -s /dev/sdk mklabel gpt mkpart propeller ext4 2048s 1048576s
  parted -s /dev/sdk mkpart primary 1050624s 420481023s
  parted -s /dev/sdk mkpart primary 420481024s 839911423s
  parted -s /dev/sdk mkpart primary 839911424s 1259341823s
  parted -s /dev/sdk mkpart logical 1259341824s 1678772223s

  parted -s /dev/sdm mklabel gpt mkpart propeller ext4 2048s 1048576s
  parted -s /dev/sdm mkpart primary 1050624s 420481023s
  parted -s /dev/sdm mkpart primary 420481024s 839911423s
  parted -s /dev/sdm mkpart primary 839911424s 1259341823s
  parted -s /dev/sdm mkpart logical 1259341824s 1678772223s
```

After running this script, the partitions will be created:

```
  /dev/sdi1 /dev/sdi2 /dev/sdi3 /dev/sdi4 /dev/sdi5
  /dev/sdj1 /dev/sdj2 /dev/sdj3 /dev/sdj4 /dev/sdj5
  /dev/sdk1 /dev/sdk2 /dev/sdk3 /dev/sdk4 /dev/sdk5
  /dev/sdm1 /dev/sdm2 /dev/sdm3 /dev/sdm4 /dev/sdm5
```

On every disk, the first partition must be labelled with 'propeller'. This
shows the IDM lock manager that these disks will be used for
global locking.  But there is no specific requirement for other
partitions, the label and partition size are both flexible.

### LVM test configurations

- After the disk partitions have been created, we need to change LVM's test
  configuration file to specify the partitions we created. (NOTE: Only use the partitions not labelled 'propeller', as it is reserved for global locking)
  [test/lib/aux.sh file](https://github.com/Seagate/lvm2-stx-private/blob/centos7_lvm2/test/lib/aux.sh#L875)

```
  BLK_DEVS[1]="/dev/sdi2"
  BLK_DEVS[2]="/dev/sdj2"
  BLK_DEVS[3]="/dev/sdk2"
  BLK_DEVS[4]="/dev/sdm2"
  BLK_DEVS[5]="/dev/sdi3"
  BLK_DEVS[6]="/dev/sdj3"
  BLK_DEVS[7]="/dev/sdk3"
  BLK_DEVS[8]="/dev/sdm3"
  BLK_DEVS[9]="/dev/sdi4"
  BLK_DEVS[10]="/dev/sdj4"
  BLK_DEVS[11]="/dev/sdk4"
  BLK_DEVS[12]="/dev/sdm4"
  BLK_DEVS[13]="/dev/sdi5"
  BLK_DEVS[14]="/dev/sdj5"
  BLK_DEVS[15]="/dev/sdk5"
  BLK_DEVS[16]="/dev/sdm5"
```

- Specify sg nodes in LVM's [test/shell/aa-lvmlockd-idm-prepare.sh file](https://github.com/Seagate/lvm2-stx-private/blob/centos7_lvm2/test/shell/aa-lvmlockd-idm-prepare.sh#L23) file, as this cleans the In-Drive Mutex after testing:

```
  dd if=/dev/zero of=zero.bin count=1 bs=512
  sg_raw -v -s 512 -i zero.bin /dev/sg2  F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
  sg_raw -v -s 512 -i zero.bin /dev/sg4  F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
  sg_raw -v -s 512 -i zero.bin /dev/sg5  F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
  sg_raw -v -s 512 -i zero.bin /dev/sg7  F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
  sg_raw -v -s 512 -i zero.bin /dev/sg10 F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
  sg_raw -v -s 512 -i zero.bin /dev/sg11 F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
  sg_raw -v -s 512 -i zero.bin /dev/sg12 F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
  sg_raw -v -s 512 -i zero.bin /dev/sg14 F0 00 00 00 00 00 00 00 00 00 00 00 02 00 FF 00
```
If you do not know the sg nodes for the partitions you created, you can check using the below command:
```
    $ lsscsi -g
```

## Check and Cleanup before run testing

Check if the drive firmware has been upgraded to the latest
version number (currently 529):

```
  $ lsscsi -g
  [0:0:8:0]    disk    SEAGATE  XS3840SE70084    B529  /dev/sdi   /dev/sg1
  [0:0:9:0]    disk    SEAGATE  XS3840SE70084    B529  /dev/sdj   /dev/sg2
  ....
```

Since the LVM testing is dependent on the IDM locking manager, you will need to
prepare a clean enviornment for LVM testing.

```
  # If lvmlockd contains any existing lockspace, it can only be killed with '-9'.
  killall -9 lvmlockd
  killall seagate_ilm
  killall sanlock
```

## Run LVM test suites

### Test with a single drive

Below is an example of using the partition /dev/sdj3 as a backing device for testing.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_DEVICE=/dev/sdj3
```

If you see a lot of failures when testing, it's good to use
a simple test case and verify if the environment has been prepared
properly. In this case, we can run the test case 'activate-minor.sh', which provides results
quickly and will indicate if any mistakes were made during the preparation process.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_DEVICE=/dev/sdj3 T=activate-minor.sh
```

After testing has finished, the folder lvm2-stx-private/test/results
will contain detailed info of the testing results for each individual test.
This directory can be packaged for easy offline analysis:

```
  # cd lvm2-stx-private/test
  # tar czvf results.tgz result
```

### Test with multiple drives

If multiple devices have been configured in the test/shell/aux.sh file (see beginning of LVM Test Configurations section),
you can test multiple IDM-enabled devices at once. The 'BLK_DEVS' array can support a maximum of 16 devices. If any case requires more than 16
devices, the test framework will fall back to using BLK_DEVS[1] as backing device and create device mapping on it.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_MULTI_DEVICES=1
```

### Test for fault injection

There are three test cases that can be used to test fault injection:
- shell/idm_lvmlockd_failure.sh: This test case is to verify if lvmlockd
  exits abnormally, that it can relaunch and talk to IDM lock manager again,
  and activate the VG and LV again by acquiring the VG/LV lock.
- shell/idm_ilm_abnormal_exit.sh: This test case is to verify that when the
  IDM lock manager has failed, the drive firmware removes the host
  from its whitelist and fences out the removed host.
  KNOWN ISSUE: the whitelist functionality is postponed for later development.
- shell/idm_ilm_recovery_back.sh: This test case is used to inject drive
  failure and if drives can recover without timeout, so that lvmlockd and the
  IDM lock manager can continue mutex operations without errors.
- shell/idm_ilm_fabric_failure_half_brain.sh: This test case is to
  emulate a fabric failure and introduce half brain, but since the renewal
  can keep majority by using half of the drives, the IDM lock
  manager can still renew the lock successfully.
  KNOWN ISSUE: After running this case, the drive might fail to recover
  and the drive names will be altered. This might impact later
  testing, and in some situations, you may need to restart the machine to continue testing.
- shell/idm_ilm_fabric_failure_timeout.sh: This test case is to emulate
  fabric failure and a failure to renew the ownership, which leads to a timeout issue;
  IDM lock manager will invoke the killpath when detecting a timeout.
  KNOWN ISSUE: After run this case, the drive might fail to recover
  and the drive names will be altered. This might impact later
  testing, and in some situations, you may need to restart the machine to continue testing.
- shell/idm_ilm_fabric_failure.sh: This test case is to emulate a
  fabric issue, making the drives disappear from the system. If the drives reconnect to the system,
  even with an altered drive device node name and SG node name, the lock manager is expected
  to work.
  NOTE: when running the test case idm_fabric_failure.sh, please ensure the
  drive names are corrected appropriately for the system.
  KNOWN ISSUE: IDM lock manager so far cannot handle properly for the
  altered device node name and SG node name, so this case fails.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 T=idm_lvmlockd_failure.sh
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 LVM_TEST_BACKING_MULTI_DEVICES=1 T=idm_ilm_abnormal_exit.sh
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 LVM_TEST_BACKING_MULTI_DEVICES=1 T=idm_ilm_recovery_back.sh
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 T=idm_ilm_fabric_failure_half_brain.sh
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 T=idm_ilm_fabric_failure_timeout.sh
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 T=idm_ilm_fabric_failure.sh
```

### Test for multi hosts

There are four cases for multi hosts testing. These LVM test cases
contain 'hosta' and 'hostb' in their name. These are used by running
the 'hosta' script on one host, and the 'hostb' script on another.
These will need to be manually run one by one.

- idm_multi_hosts_vg_hosta.sh/idm_multi_hosts_vg_hostb.sh:
  This test case is to verify VG operations on two hosts.
- idm_multi_hosts_lv_sh_ex_hosta.sh/idm_multi_hosts_lv_sh_ex_hostb.sh:
  This test case is to verify LV operations on two hosts, every host
  tries to lock LV with shareable mode and exclusive mode.
- idm_multi_hosts_lv_ex_timeout_hosta.sh/idm_multi_hosts_lv_ex_timeout_hostb.sh:
  This test case is to verify LV operations on two hosts, Host A acquires
  mutex with exclusive mode and stops the lock manager, causing it to fail
  renewal of the LV lock; on Host B, it will wait 60s for the LV lock
  timeout which was owned by Host A, then it checks if it can be granted
  with exclusive lock or not.
- idm_multi_hosts_lv_sh_timeout_hosta.sh/idm_multi_hosts_lv_sh_timeout_hostb.sh:
  This test case is to verify LV operations on two hosts, Host A acquires
  mutex with shareable mode and stop the lock manager, so it will fail
  to renew the LV lock; on Host B, it will wait 60s for the LV lock
  timeout which was owned by Host A, then it checks if it can be granted
  with exclusive lock or not.

NOTE: when running the test case idm_fabric_failure.sh, please ensure the
drive names are corrected appropriately for the system.

The cases need to be verified on two hosts:

Host A:
```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_MULTI_HOST_IDM=1 T=idm_multi_hosts_xxx_hosta.sh
```

Host B:
```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_MULTI_HOST_IDM=1 T=idm_multi_hosts_xxx_hostb.sh
```
