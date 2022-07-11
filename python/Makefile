PYTHON_ILM_MOD=_ilm.so
PYTHON_IDM_SCSI_MOD=_idm_scsi.so

SWIG = /usr/local/bin/swig
SWIGFLAGS = -python
CFLAGS = -DTEST -c -fpic -I/usr/include/python3.6m -I../src
LDFLAGS = -shared

all: $(PYTHON_ILM_MOD) $(PYTHON_IDM_SCSI_MOD)

$(PYTHON_ILM_MOD): ../src/lib_client.o lib_ilm_wrap.o
	$(LD) $(LDFLAGS) $^ -o $@

$(PYTHON_IDM_SCSI_MOD): ../src/idm_scsi.o idm_scsi_wrap.o
	$(LD) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $? -o $@

lib_ilm_wrap.c: lib_ilm.i
	$(SWIG) $(SWIGFLAGS) $<

idm_scsi_wrap.c: idm_scsi.i
	$(SWIG) $(SWIGFLAGS) $<

clean:
	rm -f *.o  *.py *.pyc *_wrap.c $(PYTHON_MOD) $(PYTHON_IDM_SCSI_MOD)
