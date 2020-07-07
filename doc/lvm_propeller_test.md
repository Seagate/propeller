# Introduction

This document is to describe the detailed info for testing
Propeller/LVM on Centos7.

# Preparations

## Create disk partitions

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

## LVM test configurations

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

# Cleanup before run testing

Since the LVM testing is dependent on lock manager, it's needed to
prepare a clean enviornment for LVM testing.

```
  # If lvmlockd contains any existed lockspace, it can only be killed with '-9'.
  killall -9 lvmlockd
  killall seagate_ilm
  killall sanlock
```

# Run LVM test suites

## Test with single drive

Below is an example to use the partition /dev/sdj3 as backing device
for testing, in this case the test will create device mappers as PV.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_DEVICE=/dev/sdj3
```

## Test with multiple drives

As the multiple devices have been configured in the shell array 'BLK_DEVS',
it can support maximum to 16 devices.  If any case requires more than 16
devices, the test framework will fallback to use BLK_DEVS[1] as backing
device and create device mapping on it.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_MULTI_DEVICES=1
```

## Test with multiple drives

As the multiple devices have been configured in the shell array 'BLK_DEVS',
it can support maximum to 16 devices.  If any case requires more than 16
devices, the test framework will fallback to use BLK_DEVS[1] as backing
device and create device mapping on it.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_BACKING_MULTI_DEVICES=1
```

## Test for fault injection

There have three test cases which is used to test with fault injection:
- shell/idm_ilm_failure.sh: This test case is to verify when the IDM
  lock manager has failed, the drive firmware should remove the host
  from its whitelist and fence out the host.
  KNOWN ISSUE: the whitelist functionality is postponed to develop in
  next phase.
- shell/idm_ilm_recovery_back.sh: This test case is to verify if the
  IDM lock manager has lost the connection with drives, and then after
  a while the manager reconnects with drives, if the manager can
  operate properly the mutexs.
- shell/idm_fabric_failure.sh: This test case is to emulate the fabric
  issue, so the drives will disappear from system.  And after a while,
  if the drives reconnect with system, test the lock manager can work
  as expected or not.
  NOTE: when run the test case idm_fabric_failure.sh, please ensure the
  drive names are corrected appropriately for the system.

```
  # cd lvm2-stx-private/test
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 T=idm_ilm_failure.sh
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 T=idm_ilm_recovery_back.sh
  # make check_lvmlockd_idm LVM_TEST_FAILURE_INJECTION=1 T=idm_fabric_failure.sh
```

## Test for multi hosts

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
