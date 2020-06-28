# Introduction

This document is to address raid lock related issues and related
discussion.


# Half brain issue

## Case 1: half brain issue for removing volume group (VG)

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

## Case 2: half brain issue for activaing volume group (VG)

After a logical volume has been created, if drives fail, this also might
lead to half brain issue.

We can use below commands to produce half brain issue:

```
# Map hardware devices
0 2097152 linear /dev/sdj2 0 | dmsetup create TESTPV1
0 2097152 linear /dev/sdj3 0 | dmsetup create TESTPV2

# Create VG
vgcreate --shared --lock-type idm TESTVG1 /dev/mapper/TESTPV1 /dev/mapper/TESTPV2

# Create LV
lvcreate -l100%FREE -n TESTVG1LV1 TESTVG1

# Deactivate VG
vgchange -a n TESTVG1

# Deactivate device
dmsetup remove -f /dev/mapper/TESTPV1

# Activate VG but cause half brain issue
vgchange -a y TESTVG1
```

As a side note, after we deactivate device 'TESTPV1', sometimes we still
can execute command 'vgchange -a y TESTVG1' to activate volume group; the
reason is the LVM configuration file has set global filter which allows
any block device to be used by LVM, so even the device 'TESTPV1' has been
unmapped, but LVM tool still can find its backend device '/dev/sdj2' and
pass to IDM lock manager for device path.

```
global_filter = [ "a|.*|" ]
```

Alternatively, if we can more strict global filter as below, which only
allows device 'TESTPV*' to be used, for this case, after TESTPV1 is
unmapped, the path string '[unknown]' will passed to lock manager and
the IDM lock manager has no chance to find corresponding SCSI device
node (e.g. /dev/sg2).  So finally this leads to half brain failure.

```
global_filter = [ "a|/dev/mapper/TESTPV.*|" ]
```

# Half brain issue mitigation

This section is to describe how to mitigate the half brain issue in the
IDM lock manager.

## Cached device mapping

IDM lock manager cannot directly send SCSI command by using block device
name, alternatively, IDM lock manager needs to find the mapping between a
block device and its associated SCSI generic node, a block device could
be a device with naming '/dev/mapper/XYZ' or directly to use the common
block device naming like '/dev/sdX'.

If IDM lock manager fails to find any mapping between block device and
SCSI generic node, it might cause half brain issue.  To mitigate half
brain issue, if find a new mapping, the IDM lock manager can cache the
mapping so this can be used for later searching.

If device name is '/dev/mapper/TESTPV1', let's see how to find its
corresponding SCSI generic node with below sequence:

- Firstly, we need to find the backend block device for the mapper; for
  example, '/dev/mapper/TESTVP1' is mapped on the top of hardware
  partition '/dev/sda2', so we need to firstly find out the device
  'sda2' is the backend devic for mapper 'TESTPV1'.

- Secondly, we need to find the mapping between the block device and
  its associated SCSI generic node (let's say '/dev/sg2').

With these two steps, IDM lock manager can know for the device
'/dev/mapper/TESTPV1', it should use SCSI generic node '/dev/sg2' to
send SCSI commands.  IDM lock manager will save the mapping between
'/dev/mapper/TESTPV1' and '/dev/sg2' in the cached list, this is
accomplished in the function ilm_add_cached_device_mapping().

Afterwards, IDM lock manager tries to parse the latest device mapping
rather than directly to fetch mapping from the cached list, but as a
last resort, if IDM lock manager fails to parse the device mapping,
it will try to retrieve mapping with ilm_find_cached_device_mapping().

Note, the cached device mapping might cause the side effect for the
*stale* mapping info, e.g. at the beginning if map '/dev/sda2' to
'/dev/mapper/TESTPV1', IDM lock manager will cache this mapping,
later user unmap '/dev/mapper/TESTPV1' and then map '/dev/sdb2' to
'/dev/mapper/TESTPV1', at this time point, if IDM lock manager fails
to find the mapping for '/dev/mapper/TESTPV1', it will try to find
the cached mapping so it will find the staled mapped device
'/dev/sda2' rather than '/dev/sdb2'.
