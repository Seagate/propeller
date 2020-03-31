# Copyright 2020-2021 Seagate
# Copyright 2020-2021 Linaro Ltd
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.

TARGET = seagate_ilm
LIB_TARGET = libseagate_ilm

SOMAJOR=0
SOMINOR=1

LIBSO_TARGET = $(LIB_TARGET).so.$(SOMAJOR).$(SOMINOR)

SOURCE = main.c \
	 log.c \
	 client.c \
	 cmd.c \
	 lockspace.c \
	 lock.c

LIB_SOURCE = lib_client.c

CFLAGS = -fPIE -DPIE
LIB_CFLAGS = -fPIC

LDFLAGS = -Wl,-z,relro -pie

LDLIBS = -lpthread -luuid

all: $(LIBSO_TARGET) $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCE) $(LDLIBS) -o $@ -L.

$(LIBSO_TARGET): $(LIB_SOURCE)
	$(CC) $(LIB_CFLAGS) $(LDFLAGS) -shared -o $@ -Wl,-soname=$(LIB_TARGET).so.$(SOMAJOR) $^
	ln -sf $(LIBSO_TARGET) $(LIB_TARGET).so
	ln -sf $(LIBSO_TARGET) $(LIB_TARGET).so.$(SOMAJOR)

clean:
	rm -f *.o *.so *.so.* $(TARGET)
