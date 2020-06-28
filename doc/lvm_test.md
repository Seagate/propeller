## LVM test result with IDM lock manager

- 8 cases are caused by cannot find drive path cannot achieve
  majority:

  activate-missing-segment.sh
  large-physical-sector-size.sh
  lvchange-partial-raid10.sh
  lvchange-partial.sh
  mirror-vgreduce-removemissing.sh
  profiles-thin.sh
  topology-support.sh
  vgchange-partial.sh

- 4 cases are dependent on big disk.

  lvconvert-cache-chunks.sh
  lvcreate-large-raid.sh
  profiles-cache.sh
  thin-large.sh

## Comparision with sanlock lock manager

There have 8 cases in LVM which cannot pass with IDM lock manager,
for more confidence, below is the checking for these 8 cases with
sanlock lock manager, and all of them fail with sanlock as well.
This can give us more confidence for the testing result.

- Test case: activate-missing-segment.sh failed
  [ 0:58]   VG @PREFIX@vg lock skipped: error -221
  [ 0:58]   Couldn't find device with uuid yhFyko-LPGx-ausB-vJX6-q3g1-JapR-9N3nwP.
  [ 0:58]   WARNING: Couldn't find all devices for LV @PREFIX@vg/lvmlock while checking used and assumed devices.
  [ 0:58]   Cannot access VG @PREFIX@vg due to failed lock.

- Test case: large-physical-sector-size.sh failed
  scsi_debug backend cannot be supported for sanlock

- Test case: lvchange-partial-raid10.sh failed
  [ 0:59] #lvchange-partial-raid10.sh:29+ lvchange -ay --partial @PREFIX@vg/LV1
  [ 0:59]   PARTIAL MODE. Incomplete logical volumes will be processed.
  [ 0:59]   VG @PREFIX@vg lock skipped: error -221
  [ 0:59]   Couldn't find device with uuid orbWR2-znUH-b9Ef-iguB-7uLl-JDlf-mFWRL6.
  [ 0:59]   Couldn't find device with uuid ZOWpJp-L2eM-9vov-xD93-iVdT-5c5k-RWi2w3.
  [ 0:59]   WARNING: Couldn't find all devices for LV @PREFIX@vg/lvmlock while checking used and assumed devices.
  [ 0:59]   Cannot access VG @PREFIX@vg due to failed lock.

- Test case: lvchange-partial.sh failed
  [ 0:59] lvchange -C y $vg/$lv1
  [ 0:59] #lvchange-partial.sh:31+ lvchange -C y @PREFIX@vg/LV1
  [ 0:59]   VG @PREFIX@vg lock failed: error -221

- Test case: mirror-vgreduce-removemissing.sh
  [ 0:58] #mirror-vgreduce-removemissing.sh:135+ prepare_lvs_
  [ 0:58] #mirror-vgreduce-removemissing.sh:115+ lvremove -ff @PREFIX@vg
  [ 0:58] #mirror-vgreduce-removemissing.sh:116+ dm_table
  [ 0:58] #utils:236+ should dmsetup table
  [ 0:58] #mirror-vgreduce-removemissing.sh:116+ not grep @PREFIX@vg
  [ 0:58] @PREFIX@vg-lvmlock: 0 524288 linear 253:46 2048
  [ 0:58] #mirror-vgreduce-removemissing.sh:117+ die 'ERROR: lvremove did leave some some mappings in DM behind!'

- Test case: profiles-thin.sh
  scsi_debug backend cannot be supported for sanlock

- Test case: topology-support.sh
  scsi_debug backend cannot be supported for sanlock

- Test case: vgchange-partial.sh
  [ 0:58] #vgchange-partial.sh:20+ aux disable_dev /dev/mapper/@PREFIX@pv1
  [ 0:58] Disabling device /dev/mapper/@PREFIX@pv1 (253:47)
  [ 0:58]
  [ 0:58] #
  [ 0:58] # Test for allowable metadata changes
  [ 0:58] # addtag_ARG
  [ 0:58] # deltag_ARG
  [ 0:58] vgchange --addtag foo $vg
  [ 0:58] #vgchange-partial.sh:26+ vgchange --addtag foo @PREFIX@vg
  [ 0:58]   VG @PREFIX@vg lock failed: error -221
