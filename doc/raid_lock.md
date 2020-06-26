## Introduction

This document is to address raid lock related issues and related
discussion.


## Half brain issue

When a volume group (VG) is created with multiple PVs, afterwards if PV
is unmapped or the associated drives fail, since the IDM lock manager
cannot operate mutex for the failed PV, this causes the half brain issue
for locking majority, thus vgremove cannot remove VG.

We can use below commands to produce this issue:

```
echo 0 163840 linear /dev/sdj2 0 | dmsetup create TESTPV1
echo 0 163840 linear /dev/sdj3 0 | dmsetup create TESTPV2
vgcreate --shared TESTVG1 --lock-type idm  /dev/mapper/TESTPV1 /dev/mapper/TESTPV2
dmsetup remove /dev/mapper/TESTPV2
vgremove TESTVG1
VG TESTVG1 lock failed: storage errors for sanlock leases [1]
```

Since IDM lock manager fails to find device '/dev/mapper/TESTPV2',
thus this causes the half brain issue for locking majority, this result
in the vgremove command to fail acquiring VG lock and returns back.

To fix this issue, we can use two methods.

Method 1: recovery the failed block device, in up case we can use
command to remap the device

```
echo 0 163840 linear /dev/sdj3 0 | dmsetup create TESTPV2
```
For the hardware device, we can unplug and plug the drive so the failed
drives can appear in the system again.

After the recovering the PVs, then vgremove command can work well.

Method 2: change to local lock type

Alternatively, as a last resort, we can force to change VG lock type
from any cluster locking scheme to local type, and vgremove command
can execute successfully:

```
vgchange --lock-type none --lockopt force  TESTVG1
vgremove TESTVG1
```

[1] vgremove command compliants the storage errors, here need to note
that it reports the error for 'sanlock leases', this is hard-coded in
LVM locking lib which assumes all IO related failures are only caused
by sanlock.
