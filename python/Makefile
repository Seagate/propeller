PYTHON_MOD=_ilm.so

SWIG = swig
SWIGFLAGS = -python
CFLAGS = -c -fpic -I/usr/include/python2.7 -I../src
LDFLAGS = -shared

all: $(PYTHON_MOD)

$(PYTHON_MOD): ../src/lib_client.o lib_ilm_wrap.o
	$(LD) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $? -o $@

lib_ilm_wrap.c: lib_ilm.i
	$(SWIG) $(SWIGFLAGS) $<

clean:
	rm -f *.o  *.py *.pyc *_wrap.c $(PYTHON_MOD)
