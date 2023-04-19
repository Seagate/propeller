%module async_nvme

%include "typemaps.i"
%include "stdint.i"
%include "carrays.i"

%array_class(char, charArray);

//%apply int *OUTPUT { int *result };
//%apply uint64_t *OUTPUT { uint64_t *handle };

%{
struct async_nvme_result {
	//TODO: int uuid;   //Use libuuid?????
	int ret_status;
	//TODO: int age
};

struct async_nvme_request {
	//TODO: int uuid;   //Use libuuid?????
	int fd;	//TODO: Use this to further verify if result MATCH is accurate??
	struct async_nvme_result *async_result;
	//TODO: int age
};

int thread_pool_destroy(void);
int thread_pool_init(void);

%}
struct async_nvme_result {
	//TODO: int uuid;   //Use libuuid?????
	int ret_status;
	//TODO: int age
};

struct async_nvme_request {
	//TODO: int uuid;   //Use libuuid?????
	int fd;	//TODO: Use this to further verify if result MATCH is accurate??
	struct async_nvme_result *async_result;
	//TODO: int age
};

int thread_pool_destroy(void);
int thread_pool_init(void);
