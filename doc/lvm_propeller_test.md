# Introduction

This document is to describe the detailed info for testing Propeller/LVM
on Centos7.

# Test for IDM SCSI wrapper and IDM lock manager

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


# Test for LVM tool

## Preparation

### Create disk partitions

Let's take the an example that a system have four block devices:
/dev/sdi, /dev/sdj, /dev/sdk, /dev/sdm.

The partitions can be created with below script:

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

On every disk, the first partition is labelled with 'propeller', this
gives the indication for IDM locking that these disks will be used for
global lock.  But there have no any specific requirement for other
partitions, the label and partition size both are flexible.

### LVM test configurations

- After setup disk partitions, we need to change LVM's test
  configuration file to specify multiple device in the
  LVM's [test/lib/aux.sh file](https://github.com/Seagate/lvm2-stx-private/blob/centos7_lvm2/test/lib/aux.sh#L875)

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

  As we have allocated paritions in the first configuration, we can
  use the partitions 2 to 5 on every disk (partition 1 is reserved
  as an indicator for global locking).

- Specify sg nodes to clean up drive firmwares in the LVM's
  [test/lib/aux.sh file](https://github.com/Seagate/lvm2-stx-private/blob/centos7_lvm2/test/lib/aux.sh#L954)

```
  sg_raw -v -r 512 -o test_data.bin /dev/sg2  88 00 01 00 00 00 00 20 FF 01 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg2  8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg4  8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg5  8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg7  8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg10 8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg11 8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg12 8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
  sg_raw -v -s 512 -i test_data.bin /dev/sg14 8E 00 FF 00 00 00 00 00 00 00 00 00 00 01 00 00
```

## Check and Cleanup before run testing

Please check the drive firmware has been upgraded to the expected
version number, so far the latest firmware version is 1759:

```
  [root@target ~]# lsscsi -g
  [0:0:8:0]    disk    SEAGATE  XS3840SE70014    1759  /dev/sdi   /dev/sg10
  [0:0:9:0]    disk    SEAGATE  XS3840SE70014    1759  /dev/sdj   /dev/sg11
  [0:0:10:0]   disk    SEAGATE  XS3840SE70014    1759  /dev/sdk   /dev/sg12
  [0:0:12:0]   disk    SEAGATE  XS3840SE70014    1759  /dev/sdm   /dev/sg14
```

Since the LVM testing is dependent on lock manager, it's needed to
prepare a clean enviornment for LVM testing.

```
  # If lvmlockd contains any existed lockspace, it can only be killed with '-9'.
  killall -9 lvmlockd
  killall seagate_ilm
  killall sanlock
```

## Run LVM test suites

### Test with single drive

Below is an example to use the partition /dev/sdj3 as backing device
for testing, in this case the test will create device mappers as PV.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_DEVICE=/dev/sdj3
```

If see many failures for the single drive testing, it's good to test
with a simple test case and verify if the envoirnment has been prepared
properly, e.g. we can run the test case 'activate-minor.sh' and it can
give us the result in short time, so we can use this way to quickly
check if the test machine is ready for mass testing or not.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_DEVICE=/dev/sdj3 T=activate-minor.sh
```

After finish run the testing, the folder lvm2-stx-private/test/results
contains the detailed info for testing result, so it's good to package
it for offline analysis:

```
  # cd lvm2-stx-private/test
  # tar czvf results.tgz result
```

### Test with multiple drives

As the multiple devices have been configured in the shell array 'BLK_DEVS',
it can support maximum to 16 devices.  If any case requires more than 16
devices, the test framework will fallback to use BLK_DEVS[1] as backing
device and create device mapping on it.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_MULTI_DEVICES=1
```

### Test with multiple drives

As the multiple devices have been configured in the shell array 'BLK_DEVS',
it can support maximum to 16 devices.  If any case requires more than 16
devices, the test framework will fallback to use BLK_DEVS[1] as backing
device and create device mapping on it.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_MULTI_DEVICES=1
```

### Test for fault injection

There have three test cases which is used to test with fault injection:
- shell/idm_lvmlockd_failure.sh: This test case is to verify if lvmlockd
  exits abnormally, it can relaunch and talk to IDM lock manager again;
  it also can activate again for VG and LV by acquiring the VG/LV lock.
- shell/idm_ilm_abnormal_exit.sh: This test case is to verify when the
  IDM lock manager has failed, the drive firmware should remove the host
  from its whitelist and fence out the host.
  KNOWN ISSUE: the whitelist functionality is postponed to develop in
  next phase.
- shell/idm_ilm_recovery_back.sh: This test case is to inject drive
  failure and drives recovery back without timeout, so the lvmlockd and
  IDM lock manager can continue mutex operations and without errors.
- shell/idm_ilm_fabric_failure_half_brain.sh: This test case is to
  emulate fabric failure and introduce half brain, but since the renewal
  can keep majority by making success half of drives, so IDM lock
  manager still can renew the lock successfully.
  KNOWN ISSUE: After run this case, the drive might fail to recovery back
  to system and the drive names will be altered, this might impact later's
  testing, in some situation, need to restart the machine so can continue
  other testing.
- shell/idm_ilm_fabric_failure_timeout.sh: This test case is to emulate
  fabric failure and fail to renew the ownership, leads to timeout issue;
  IDM lock manager will invoke kill path when detect timeout.
  KNOWN ISSUE: After run this case, the drive might fail to recovery back
  to system and the drive names will be altered, this might impact later's
  testing, in some situation, need to restart the machine so can continue
  other testing.
- shell/idm_ilm_fabric_failure.sh: This test case is to emulate the
  fabric issue, so the drives will disappear from system.  And after a
  while, if the drives reconnect with system, even the drive device node
  name and SG node name have been altered, the lock manager is expected
  to work with these altered names.
  NOTE: when run the test case idm_fabric_failure.sh, please ensure the
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

There have four cases for multi hosts testing, the test case name
contains 'hosta' and 'hostb', which means these two test script should
run on two different hosts, usually the execution sequence is firstly
to run 'hosta' script and then on another host to run 'hostb' script.
These scripts we need to manually to run one by one.

- idm_multi_hosts_vg_hosta.sh/idm_multi_hosts_vg_hostb.sh:
  This test case is to verify VG operations on two hosts.
- idm_multi_hosts_lv_sh_ex_hosta.sh/idm_multi_hosts_lv_sh_ex_hostb.sh:
  This test case is to verify LV operations on two hosts, every host
  tries to lock LV with shareable mode and exclusive mode.
- idm_multi_hosts_lv_ex_timeout_hosta.sh/idm_multi_hosts_lv_ex_timeout_hostb.sh:
  This test case is to verify LV operations on two hosts, hosta acquires
  mutex with exclusive mode and stop the lock manager, so it will fail
  to renew the LV lock; on hostb, it will wait for 60s for the LV lock
  timeout which was owned by hosta, then it check if it can be granted
  with exclusive lock or not.
- idm_multi_hosts_lv_sh_timeout_hosta.sh/idm_multi_hosts_lv_sh_timeout_hostb.sh:
  This test case is to verify LV operations on two hosts, hosta acquires
  mutex with shareable mode and stop the lock manager, so it will fail
  to renew the LV lock; on hostb, it will wait for 60s for the LV lock
  timeout which was owned by hosta, then it check if it can be granted
  with exclusive lock or not.

NOTE: when run the test case idm_fabric_failure.sh, please ensure the
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
