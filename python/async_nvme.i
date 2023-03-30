%module async_nvme

%include "typemaps.i"
%include "stdint.i"
%include "carrays.i"

%array_class(char, charArray);

//%apply int *OUTPUT { int *result };
//%apply uint64_t *OUTPUT { uint64_t *handle };

%{

int thread_pool_destroy(void);
int thread_pool_init(void);

%}

int thread_pool_destroy(void);
int thread_pool_init(void);
