PYTHON_ILM_MOD=_ilm.so
PYTHON_IDM_API_MOD=_idm_api.so
PYTHON_ASYNC_NVME_MOD=_async_nvme.so

SWIG = /usr/local/bin/swig
SWIGFLAGS = -python
CFLAGS = -DTEST -c -fpic -I/usr/include/python3.6m -I../src
LDFLAGS = -shared

IDM_OBJECT_FILES = \
	 ../src/async_nvme_thread_pool.o \
	 ../src/idm_cmd_common.o \
	 ../src/idm_nvme_api.o \
	 ../src/idm_nvme_io.o \
	 ../src/idm_nvme_utils.o \
	 ../src/idm_scsi.o \
	 ../src/idm_api.o

all: $(PYTHON_ILM_MOD) $(PYTHON_IDM_API_MOD) $(PYTHON_ASYNC_NVME_MOD)

$(PYTHON_ILM_MOD): ../src/lib_client.o lib_ilm_wrap.o
	$(LD) $(LDFLAGS) $^ -o $@

$(PYTHON_IDM_API_MOD): $(IDM_OBJECT_FILES) idm_api_wrap.o
	$(LD) $(LDFLAGS) $^ -o $@

$(PYTHON_ASYNC_NVME_MOD): ../src/async_nvme_thread_pool.o async_nvme_wrap.o
	$(LD) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $? -o $@

lib_ilm_wrap.c: lib_ilm.i
	$(SWIG) $(SWIGFLAGS) $<

idm_api_wrap.c: idm_api.i
	$(SWIG) $(SWIGFLAGS) $<

async_nvme_wrap.c: async_nvme.i
	$(SWIG) $(SWIGFLAGS) $<

clean:
	rm -f *.o  *.py *.pyc *_wrap.c *.so
