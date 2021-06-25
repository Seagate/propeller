# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.

version := $(shell cat VERSION)

ifeq ($(shell git describe --exact-match 2>/dev/null),)
release := $(shell git describe --tags | awk -F- '{print $$(NF-1) "." $$(NF)}')
else
release := 0
endif

SUBDIRS = src python test

.PHONY: all $(SUBDIRS) clean install cscope

all:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
	rm -rf cscope.*

install:
	$(MAKE) -C src $@

cscope:
	@echo "  CSCOPE"
	${Q}find ${CURDIR} -name "*.[ch]" > cscope.files
	${Q}cscope -b -q -k

spec:
	sed -e 's/@VERSION@/$(version)/g' \
		-e 's/@RELEASE@/$(release)/g' \
		seagate_ilm.spec.in > seagate_ilm.spec
