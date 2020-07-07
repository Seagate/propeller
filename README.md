Propeller - LVM Locking scheme with Seagate IDM
===============================================

This repository provides the customized locking scheme based on Seagate
In-Drive-Mutex (IDM) and integration into lvmlockd.

This repository contains the IDM lock manager under 'src' folder; later
it can be extended to add IDM wrapper APIs lib and integrate with LVM2
for full stack releasing.

The library and APIs is implemented in C.

Building and Installation
-------------------------

See [LVM and IDM lock manager installation](doc/lvm_propeller_install.md) for details.

Enviornment Variables and Logging
---------------------------------

See [LVM and IDM lock manager debugging](doc/lvm_propeller_debug.md) for details.

Testing
-------

See [LVM and IDM lock manager testing](doc/lvm_propeller_test.md) for details.
