## Mutex usages for LVM commands

| LVM command | Used mutex before command | Used mutex in command | Used mutex after command | Max count  |
| ----------- | ------------------------- | --------------------- | ------------------------ | ---------- |
| pvcreate    | n/a                       | GL/VG lock            | n/a                      | 2          |
| pvremove    | n/a                       | GL/VG lock            | n/a                      | 2          |
| vgcreate    | n/a                       | GL lock               | n/a                      | 1          |
| vgremove    | n/a                       | GL/VG/LV lock         | n/a                      | 3          |
| vgreduce    | n/a                       | GL/VG lock            | n/a                      | 2          |
| vgextend    | n/a                       | GL/VG lock            | n/a                      | 2          |
| vgchange    | n/a                       | GL lock               | n/a                      | 1          |
| lvcreate    | n/a                       | VG/LV lock            | LV lock                  | 2          |
| lvremove    | n/a                       | VG lock               | n/a                      | 1          |
| lvchange    | n/a                       | VG /LV lock           | LV lock                  | 2          |


## Mutex usages for LVM thin provisioning

Below commands are an example to create thin pool (TESTPOOL) and LV (TESTLVO0):
lvcreate -L4M -T TESTVG1/TESTPOOL
lvchange -an TESTVG1/TESTPOOL
lvcreate -V2G --name TESTLVO0 -T TESTVG1/TESTPOOL

Below commands are an example to create thin pool (TESTPOOL), LV (TESTVG1LV1)
and snapshot LV (TESTVG1LV2):
lvcreate -L10M -I4 -i2 -V10M -T TESTVG1/TESTVG1POOL --name TESTVG1LV1
lvcreate -s TESTVG1/TESTVG1LV1 --name TESTVG1LV2

The thin pool, LV and snapshot LV in the thin pool share the same mutex which
is created for the thin pool.

| LVM command        | Used mutex before command | Used mutex in command | Used mutex after command | Max count  |
| ------------------ | ------------------------- | --------------------- | ------------------------ | ---------- |
| Create thin pool   | n/a                       | VG/LV thin pool lock  | n/a                      | 2          |
| Remove thin pool   | n/a                       | VG/LV thin pool lock  | n/a                      | 2          |
| Create thin LV     | n/a                       | VG/LV thin pool lock  | n/a                      | 2          |
| Remove thin LV     | n/a                       | VG/LV thin pool lock  | n/a                      | 2          |
| Create snapshot LV | n/a                       | VG/LV thin pool lock  | n/a                      | 2          |
| Remove snapshot LV | n/a                       | VG/LV thin pool lock  | n/a                      | 2          |


## Mutex usages for nested device mapping

Nested device mapping is a common feature can be used by creating VG on top of
other existed VG/LVs.

Below is an example for the nested device mapping, in this example, it firstly
creates a thin pool (TESTVG1/TESTVG1POOL) with "ignore" discards, and a new LV
(TESTVG1LV1) is created within the thin pool.  Based on this, the second VG
(TESTVG2) is created and it is built on top of the first VG's thin LV
(/dev/TESTVG1/TESTVG1LV1).  So this creates the nested device mapping between the
two level's VG. 
vgcreate --shared TESTVG1 --lock-type idm  /dev/mapper/TESTPV1 /dev/mapper/TESTPV2
lvcreate -l 10 -T TESTVG1/TESTVG1POOL --discards ignore
lvcreate -V 9m -T TESTVG1/TESTVG1POOL -n TESTVG1LV1
lvchange -aey TESTVG1/TESTVG1LV1
vgcreate --shared --locktype idm -s 1m TESTVG2 /dev/TESTVG1/TESTVG1LV1 /dev/mapper/TESTPV3

| LVM command        | Used mutex before command | Used mutex in command | Used mutex after command | Max count  |
| ------------------ | ------------------------- | --------------------- | ------------------------ | ---------- |
| Create nested VG   | TESTVG1/TESTVG1LV1 lock   | GL/VG2 lock           | TESTVG1/TESTVG1LV1 lock  | 3          |
| Remove nested VG   | TESTVG1/TESTVG1LV1 lock   | GL/VG2 lock           | n/a                      | 3          |
