# Copyright 2020-2021 Seagate
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.
#
include ../Makefile.config
include ../Makefile.common

TARGET = seagate_ilm
HEADER_TARGET = ilm.h
LIB_TARGET = libseagate_ilm

SOMAJOR=0
SOMINOR=1

LIBSO_TARGET = $(LIB_TARGET).so.$(SOMAJOR).$(SOMINOR)
LIBPC_TARGET = libseagate_ilm.pc

SOURCE = main.c \
	 log.c \
	 client.c \
	 cmd.c \
	 lockspace.c \
	 lock.c \
	 util.c \
	 raid_lock.c \
	 inject_fault.c \
	 failure.c \
	 drive.c

LIB_SOURCE = lib_client.c

CFLAGS += -fPIE -DPIE -D_GNU_SOURCE

ifdef IDM_PTHREAD_EMULATION
CFLAGS += -DIDM_PTHREAD_EMULATION
SOURCE += idm_pthread_backend.c
else
SOURCE += idm_scsi.c
endif

LIB_CFLAGS = -fPIC

LDFLAGS = -Wl,-z,relro -pie

LDLIBS = -lpthread -luuid -lblkid -ludev

all: $(LIBSO_TARGET) $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCE) $(LDLIBS) -o $@ -L.

$(LIBSO_TARGET): $(LIB_SOURCE)
	$(CC) $(LIB_CFLAGS) $(LDFLAGS) -shared -o $@ -Wl,-soname=$(LIB_TARGET).so.$(SOMAJOR) $^
	ln -sf $(LIBSO_TARGET) $(LIB_TARGET).so
	ln -sf $(LIBSO_TARGET) $(LIB_TARGET).so.$(SOMAJOR)

clean:
	rm -f *.o *.so *.so.* $(TARGET)

INSTALL=$(shell which install)

DESTDIR=
BINDIR=/usr/sbin
LIBDIR=/usr/lib64
HEADIR=/usr/include

.PHONY: install
install: all
	$(INSTALL) -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -d $(DESTDIR)/$(LIBDIR)
	$(INSTALL) -d $(DESTDIR)/$(HEADIR)
	$(INSTALL) -c -m 755 $(TARGET) $(DESTDIR)/$(BINDIR)
	$(INSTALL) -c -m 755 $(LIBSO_TARGET) $(DESTDIR)/$(LIBDIR)
	$(INSTALL) -c -m 644 $(LIBPC_TARGET) $(DESTDIR)/$(LIBDIR)/pkgconfig
	cp -a $(LIB_TARGET).so $(DESTDIR)/$(LIBDIR)
	cp -a $(LIB_TARGET).so.$(SOMAJOR) $(DESTDIR)/$(LIBDIR)
	$(INSTALL) -c -m 644 $(HEADER_TARGET) $(DESTDIR)/$(HEADIR)
	$(INSTALL) -c -m 644 seagate_ilm.service /usr/lib/systemd/system/
	$(INSTALL) -c -m 644 logrotate.ilm /etc/logrotate.d/seagate_ilm
