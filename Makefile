SUBDIRS = src

.PHONY: all $(SUBDIRS) clean install cscope

$(SUBDIRS):
	$(MAKE) -C $@

clean install:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
	rm -rf cscope.*

cscope:
	@echo "  CSCOPE"
	${Q}find ${CURDIR} -name "*.[ch]" > cscope.files
	${Q}cscope -b -q -k
