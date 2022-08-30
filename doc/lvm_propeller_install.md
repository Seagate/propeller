# Introduction

This document describes the installation process for Propeller and LVM on Centos7.

# Build and Install IDM lock manager

## Prepare dependencies

### Install the dependency libs

```
    $ sudo yum groupinstall 'Development Tools'
    $ sudo yum install libuuid-devel
    $ sudo yum install libblkid libblkid-devel
    $ sudo yum install python3 python3-devel
    $ sudo pip3 install pytest
    $ sudo yum install pcre-devel
```
The SWIG library must be installed from source to bypass CentOS 7 repository:
```
    $ sudo git clone https://github.com/swig/swig.git
    $ cd swig-4.0.x
    $ ./autogen.sh
    $ ./configure
    $ make
    $ make install
```

If any problems are encountered during swig installation, please refer to the
swig website for troublesooting details.
```
    https://www.swig.org
```
### Load the dependency modules

The Propeller IDM lock manager is dependent on the SCSI generic module,
loading the kernel module 'sg' is a prerequisite for using IDM lock
manager:

```
    $ sudo modprobe sg
```

If want to load the module automatically at boot time,
we can use the SCSI configuration file for this:

```
    $ sudo echo sg >> /etc/modules-load.d/scsi.conf
```


## Build Propeller IDM lock manager

The Propeller repository has three main parts to build:

- The IDM lock manager and the lib which is in 'src/' folder;
- The Python wrapper for pytest test harness in the 'python/' folder;
- The test harness in the 'test/' folder, which has C code that must be compiled before running pytest.

Running 'make' from the root folder of Propeller will build all three of these parts.

```
    $ cd /work/path
    $ git clone https://github.com/Seagate/propeller
    $ cd /work/path/propeller
    $ make
```

## Install Propeller IDM lock manager

On Centos7, the 'make install' command can be used to install libs,
header files and systemd service file:

```
    $ sudo make install
```

## Launch Propeller IDM lock manager

Systemctl can be used to launch Propeller IDM lock manager:

```
    $ sudo systemctl start seagate_ilm
```

The 'systemctl status' command can be used to check the IDM lock manager's status:

```
    $ sudo systemctl status seagate_ilm
● seagate_ilm.service - Seagate Propeller IDM Lock Manager
   Loaded: loaded (/usr/lib/systemd/system/seagate_ilm.service; disabled; vendor preset: enabled)
   Active: active (running) since Mon 2020-06-29 03:28:12 EDT; 13s ago
  Process: 2337 ExecStart=/usr/sbin/seagate_ilm -l 0 -L 7 -E 7 -S 7 (code=exited, status=0/SUCCESS)
 Main PID: 2338 (seagate_ilm)
    Tasks: 4 (limit: 4915)
   CGroup: /system.slice/seagate_ilm.service
           └─2338 /usr/sbin/seagate_ilm -l 0 -L 7 -E 7 -S 7

Jun 29 03:28:12 target.test.com systemd[1]: Starting Seagate Propeller IDM Lock Manager...
Jun 29 03:28:12 target.test.com systemd[1]: Started Seagate Propeller IDM Lock Manager.

```

# Build and Install LVM toolkit (with lvmlockd)

## Prepare dependencies

Install dependency libs on Centos7:

```
    $ sudo yum install sanlock sanlock-devel sanlock-lib
    $ sudo yum install dlm dlm-devel dlm-lib
    $ sudo yum install lvm2-lockd
    $ sudo yum install libaio-devel dbus-devel libudev-devel
    $ sudo yum install readline readline-devel
    $ sudo yum install corosynclib corosynclib-devel
    $ sudo pip3 install pyudev
```

## Build LVM toolkit

Historically, the installation procedure below uses the LVM code located at
https://github.com/Seagate/lvm2-idm to build the modified lvmlockd.  This was
the Seagate development repo for LVM-specific changes related to the Propeller
projects modifications of lvmlockd.

However, the above-mentioned code has since been upstreamed to the
primary LVM Linux repository, https://github.com/lvmteam/lvm2.
So, pulling and building LVM from this repo should work.  However, any subsequent
changes on this repo's master branch may introduce additional dependencies.

```
    $ cd /work/path
    $ git clone https://github.com/Seagate/lvm2-idm
    $ cd /work/path/lvm2-idm
    $ git checkout -b centos7_lvm2 origin/centos7_lvm2
    $ ./configure --build=x86_64-redhat-linux-gnu --host=x86_64-redhat-linux-gnu \
      --program-prefix= \
      --disable-dependency-tracking \
      --prefix=/usr \
      --exec-prefix=/usr \
      --bindir=/usr/bin \
      --sbindir=/usr/sbin \
      --sysconfdir=/etc \
      --datadir=/usr/share \
      --includedir=/usr/include \
      --libdir=/usr/lib64 \
      --libexecdir=/usr/libexec \
      --localstatedir=/var \
      --sharedstatedir=/var/lib \
      --mandir=/usr/share/man \
      --infodir=/usr/share/info \
      --with-default-dm-run-dir=/run \
      --with-default-run-dir=/run/lvm \
      --with-default-pid-dir=/run \
      --with-default-locking-dir=/run/lock/lvm \
      --with-usrlibdir=/usr/lib64 \
      --enable-fsadm --enable-write_install \
      --with-user= --with-group= --with-device-uid=0 --with-device-gid=6 \
      --with-device-mode=0660 --enable-pkgconfig --enable-applib \
      --enable-cmdlib --enable-dmeventd --enable-blkid_wiping \
      --enable-python2-bindings --with-cluster=internal \
      --with-clvmd=corosync --enable-cmirrord --with-udevdir=/usr/lib/udev/rules.d \
      --enable-udev_sync --with-thin=internal --enable-lvmetad --with-cache=internal \
      --enable-lvmpolld --enable-lvmlockd-dlm --enable-lvmlockd-sanlock \
      --enable-lvmlockd-idm --enable-dmfilemapd
    $ make
```

## Install LVM toolkit

On the Centos7, it's simple to use 'make install' to install libs,
header files and systemd service file with single one command:

```
    $ sudo make install
```

*Please note*, if rootFS is the logical volume, the system
boot process requires LVM commands.  Upgrading the
LVM toolkit might cause a serious booting failure because of
incompatibility between LVM tools and libs, and recovering your system will be difficult.
So at this stage, please ensure the experimental PC doesn't contain any useful
data.

## Launch lvmlockd daemon

Change lvmlockd configuration to use IDM as default for global locking:
```
    $ sudo vi /usr/lib/systemd/system/lvm2-lvmlockd.service

    Change the below line to specify global lock as IDM:
    ExecStart=/usr/sbin/lvmlockd -f -g idm
```

Systemctl can be used to launch lvmlockd:

```
    $ sudo systemctl restart lvm2-lvmlockd
```

The below command can be used to check the lvmlockd status:

```
    $ sudo systemctl status lvm2-lvmlockd
● lvm2-lvmlockd.service - LVM2 lock daemon
   Loaded: loaded (/usr/lib/systemd/system/lvm2-lvmlockd.service; enabled; vendor preset: enabled)
   Active: active (running) since Mon 2020-06-29 04:06:29 EDT; 9s ago
     Docs: man:lvmlockd(8)
 Main PID: 1788 (lvmlockd)
    Tasks: 3 (limit: 4915)
   CGroup: /system.slice/lvm2-lvmlockd.service
           └─1788 /usr/sbin/lvmlockd -f

Jun 29 04:06:29 node1.test.com systemd[1]: Started LVM2 lock daemon.
Jun 29 04:06:29 node1.test.com lvmlockd[1788]: [D] creating /run/lvm/lvmlockd.socket
Jun 29 04:06:29 node1.test.com lvmlockd[1788]: 1593417989 lvmlockd started
```
