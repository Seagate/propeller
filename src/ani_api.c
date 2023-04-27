/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * ani_api.c - Source file for the IDM aync nvme interface (ANI).
 * This interface is for simulating NVMe asynchronous command behavior.
 * This is in-lieu of using Linux kernel-supplied NVMe asynchronous capability.
 * Currently, bypassing the kernel-supplied async functionality due to th e fact
 * that the Linux kernel containing this functionality is too "new" and, therefore,
 * not easily adoptable by customers.
 *
 * There are 2 main sections:
 * 	The public "ASYNC NVME Interface"
 * 		Exposes API's used by the idm_nvme_io.c commands.
 * 		Asyncnronous behavior achieved via thread pools.
 * 	The private "LOOKUP TABLE" interface
 *		Links the device string name ("/dev/nvme2") to its'
 		corresponding thread pool.

		TODO: This table needs to be redone.
			Implemented with a simple linear lookup algo.
			Will be too slow for large populations of drives (30+).
			Alternatives:
				Use Linux device# (ie: the 2 in /dev/nvme2n1) as the table index?
				Fully implemented hash table?
				Use existing double-linked list from .../src/list.h?
				Other?
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

#include "ani_api.h"
#include "idm_nvme_utils.h"
#include "thpool.h"


/* ======================= COMPILE SWITCHES========================== */

//TODO: DELETE THESE 2 (AND ALL CORRESPONDING CODE) AFTER ASYNC NVME INTEGRATION
#define COMPILE_STANDALONE
#ifdef MAIN_ACTIVATE_ANI_API
#define MAIN_ACTIVATE_ANI_API 1
#else
#define MAIN_ACTIVATE_ANI_API 0
#endif

#define FUNCTION_ENTRY_DEBUG    //TODO: Remove this entirely???

#define DEBUG_TABLE__DRIVE_TO_POOL


/* ========================== DEFINES ============================ */

#define MAX_TABLE_ENTRIES		8
#define TABLE_ENTRY_DRIVE_BUFFER_SIZE	32
#define NUM_POOL_THREADS		4


/* ========================== STRUCTURES ============================ */

struct table_entry {
	char *drive;
	struct threadpool *thpool;
};


/* =========================== GLOBLALS ============================= */

//Primary drive-to-threadpool table
struct table_entry *table_thpool[MAX_TABLE_ENTRIES];


/* ========================== PROTOTYPES ============================ */

static int ani_ioctl(void* arg);

static int  _table_entry_is_empty(struct table_entry *entry);
static int  _table_entry_find_empty(void);
static int  _table_entry_find_index(char *drive);
static int  table_init(void);
static struct table_entry* table_entry_find(char *drive);
static int  table_entry_add(char *drive, int n_pool_thrds);
static int  table_entry_replace(char *drive, int n_pool_thrds);
static int  table_entry_update(char *drive, int n_pool_thrds);
static void table_entry_remove(char *drive);
static void table_destroy(void);
static void table_show(void);


/* ================== ASYNC NVME INTERFACE(ANI) ===================== */

int ani_init(void)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	return table_init();
}

//The request_idm already points to the memory locations where the desired data will be stored
//If the result is found, and shows success, then the data has already been written to those memory locations.
//Just return and let the calling code retrieve what it needs.
// Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
int ani_data_rcv(struct idm_nvme_request *request_idm, int *result)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	char *drive = request_idm->drive;
	struct table_entry *entry;
	int ret = FAILURE;

	entry = table_entry_find(drive);
	if (!entry){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: find fail: %s", __func__, drive);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: find fail: %s\n", __func__, drive);
		#endif
		return FAILURE;
	}

	ret = thpool_find_result(entry->thpool, request_idm->uuid_async_job,
	                         10000, 10000, result); //TODO: Fix hard-coded values
	if (ret){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: add work fail: %s", __func__, drive);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: add work fail: %s\n", __func__, drive);
		#endif
		ret = FAILURE;
	}

	return ret;
}

// Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
int ani_send_cmd(struct idm_nvme_request *request_idm, unsigned long ctrl)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	char *drive                   = request_idm->drive;
	struct arg_ioctl *arg         = request_idm->arg_async_nvme;
	struct nvme_passthru_cmd *cmd = request_idm->cmd_nvme_passthru;
	struct table_entry *entry;
	int ret = FAILURE;

	entry = table_entry_find(drive);
	if (!entry){
		//Auto-create new thpool for drive
		ret = table_entry_add(drive, NUM_POOL_THREADS);
		if (ret){
			#if !defined(COMPILE_STANDALONE)
			ilm_log_err("%s: add entry fail: %s", __func__, drive);
			#elif defined(COMPILE_STANDALONE)
			printf("%s: add entry fail: %s\n", __func__, drive);
			#endif
			ret = FAILURE;
			goto EXIT;
		}
		entry = table_entry_find(drive);
		if (!entry){
			#if !defined(COMPILE_STANDALONE)
			ilm_log_err("%s: find fail: %s", __func__, drive);
			#elif defined(COMPILE_STANDALONE)
			printf("%s: find fail: %s\n", __func__, drive);
			#endif
			ret = FAILURE;
			goto EXIT;
		}
	}

	//This is kludgy, but needed to simplify how memory was freed for
	//ANI's use of 'struct arg_ioctl' and 'struct nvme_passthru_cmd'.
	//This way, this memory will be freed when request_idm is freed.
	arg = (struct arg_ioctl*)calloc(1, sizeof(*arg));
	if (!arg){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: arg calloc fail: drive %s", __func__, drive);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: arg calloc fail: drive %s\n", __func__, drive);
		#endif
		ret = -ENOMEM;
		goto EXIT;
	}

	cmd = (struct nvme_passthru_cmd *)calloc(1, sizeof(*cmd));
	if (!cmd){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: cmd calloc fai: drive %s", __func__, drive);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: cmd calloc fai: drive %s\n", __func__, drive);
		#endif
		ret = -ENOMEM;
		goto EXIT;
	}

	fill_nvme_cmd(request_idm, cmd);
	arg->fd   = request_idm->fd_nvme;
	arg->ctrl = ctrl;
	arg->cmd  = cmd;

	uuid_generate(request_idm->uuid_async_job);

	//TODO: Keep?  Add debug flag?
	dumpNvmePassthruCmd(cmd);

	ret = thpool_add_work(entry->thpool, request_idm->uuid_async_job,
	                      (th_func_p)ani_ioctl, (void*)arg);
	if (ret){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: add work fail: %s", __func__, drive);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: add work fail: %s\n", __func__, drive);
		#endif
		ret = FAILURE;
	}
EXIT:
	return ret;
}

void ani_destroy(void)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	table_destroy();
}

static int ani_ioctl(void* arg)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	struct arg_ioctl *arg_ = (struct arg_ioctl *)arg;

	return ioctl(arg_->fd, arg_->ctrl, arg_->cmd);
}


/* ========================== LOOKUP TABLE ============================ */

//return: 1 when empty, 0 when NOT empty (so behaves like logical bool)
static int _table_entry_is_empty(struct table_entry *entry)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	if (!entry) return 1;

	if ((!entry->drive) || (!entry->thpool)) return 1;

	return 0;
}

//Finds the index of the first table entry that is emtpy
//return: (int >= 0) on success, -1 on failure(table full)
static int _table_entry_find_empty(void)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int i;
	struct table_entry *entry;
	int ret = FAILURE;

	for(i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = table_thpool[i];
		if (_table_entry_is_empty(entry)){
			ret = i;
			break;
		}
	}

	if (ret >= 0){
		#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		ilm_log_dbg("%s: empty entry found at %d", __func__, ret);
		#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		printf("%s: empty entry found at %d\n", __func__, ret);
		#endif
	}
	else{
		#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		ilm_log_dbg("%s: empty entry NOT found", __func__);
		#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		printf("%s: empty entry NOT found\n", __func__);
		#endif
	}
	return ret;
}

//Finds the index of the first table entry with the matching drive string.
//return: (int >= 0) on success, -1 on failure
static int _table_entry_find_index(char *drive)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START: %s\n", __func__, drive);
	#endif //FUNCTION_ENTRY_DEBUG

	int i;
	struct table_entry *entry;
	int ret = FAILURE;

	if (!drive){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: invalid param", __func__);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: invalid param\n", __func__);
		#endif
		return FAILURE;
	}

	for(i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = table_thpool[i];
		if (!_table_entry_is_empty(entry)){
			if (strcmp(entry->drive, drive) == 0){
				ret = i;
				break;
			}
		}
	}

	if (ret >= 0){
		#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		ilm_log_dbg("%s: %s found at %d", __func__, entry->drive, ret);
		#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		printf("%s: %s found at %d\n", __func__, entry->drive, ret);
		#endif
	}
	else{
		#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		ilm_log_dbg("%s: %s NOT found", __func__, drive);
		#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		printf("%s: %s NOT found\n", __func__, drive);
		#endif
	}
	return ret;
}

//returns: 0 on success, -1 on failure.
static int table_init(void)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int i;
	struct table_entry *entry;

	//If calloc() fails, known state for ALL "entry" when table_destroy() runs
	memset(&table_thpool, 0, MAX_TABLE_ENTRIES * sizeof(*table_thpool));

	for (i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = (struct table_entry *)calloc(MAX_TABLE_ENTRIES,
		                                     sizeof(*table_thpool));
		if (!entry) {
			table_destroy();
			#if !defined(COMPILE_STANDALONE)
			ilm_log_err("%s: calloc failure", __func__);
			#elif defined(COMPILE_STANDALONE)
			printf("%s: calloc failure\n", __func__);
			#endif
			return FAILURE;
		}
		table_thpool[i] = entry;
	}

	return 0;
}

//Finds the table entry corresponding to the drive string.
//return: valid entry ptr on success, NULL on failure
static struct table_entry* table_entry_find(char *drive)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START: %s\n", __func__, drive);
	#endif //FUNCTION_ENTRY_DEBUG

	int i;
	struct table_entry *entry = NULL;

	if (!drive){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: invalid param", __func__);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: invalid param\n", __func__);
		#endif
		return NULL;
	}

	for (i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = table_thpool[i];
		if (!_table_entry_is_empty(entry)){
			if (strcmp(entry->drive, drive) == 0){
				break;
			}
		}
		entry = NULL;
	}

	if (entry){
		#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		ilm_log_dbg("%s: %s found", __func__, entry->drive);
		#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		printf("%s: %s found\n", __func__, entry->drive);
		#endif
	}
	else{
		#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		ilm_log_dbg("%s: %s NOT found", __func__, drive);
		#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		printf("%s: %s NOT found\n", __func__, drive);
		#endif
	}

	return entry;
}

//Adds to the table a NEW table entry, ONLY.
//returns: 0 on success, -1 on failure.
static int table_entry_add(char *drive, int n_pool_thrds)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START: %s\n", __func__, drive);
	#endif //FUNCTION_ENTRY_DEBUG

	int index;
	struct table_entry *entry;

	index = _table_entry_find_empty();
	if (index <= 0) {
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: %s{%d} NOT added", __func__, drive, n_pool_thrds);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: %s{%d} NOT added\n", __func__, drive, n_pool_thrds);
		#endif
		return FAILURE;
	}

	entry = table_thpool[index];

	entry->drive = (char *)calloc(TABLE_ENTRY_DRIVE_BUFFER_SIZE,
					sizeof(char));
	if (!entry->drive){
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: drive calloc failure", __func__);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: drive calloc failure\n", __func__);
		#endif
		return FAILURE;
	}
	strcpy(entry->drive, drive);

	entry->thpool = thpool_init(n_pool_thrds);
	if (!entry->thpool){
		return FAILURE;
	}

	#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
	ilm_log_err("%s: %s{%d} added at %d",
			__func__, entry->drive,
			thpool_num_threads_alive(entry->thpool), index);
	#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
	printf("%s: %s{%d} added at %d\n",
		__func__, entry->drive,
		thpool_num_threads_alive(entry->thpool), index);
	#endif

	return SUCCESS;
}

//Replaces in the table an EXISTING table entry, ONLY.
//returns: 0 on success, -1 on failure.
static int table_entry_replace(char *drive, int n_pool_thrds)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START: %s\n", __func__, drive);
	#endif //FUNCTION_ENTRY_DEBUG

	int index;
	struct table_entry *entry;

	index = _table_entry_find_index(drive);  //find existing entry and update
	if (index <= 0) {
		#if !defined(COMPILE_STANDALONE)
		ilm_log_err("%s: %s{%d} NOT replaced", __func__, drive, n_pool_thrds);
		#elif defined(COMPILE_STANDALONE)
		printf("%s: %s{%d} NOT replaced\n", __func__, drive, n_pool_thrds);
		#endif
		return FAILURE;
	}

	entry = table_thpool[index];

	thpool_destroy(entry->thpool);
	entry->thpool = thpool_init(n_pool_thrds);
	if (!entry->thpool){
		return FAILURE;
	}

	#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
	ilm_log_dbg("%s: %s{%d} replaced at %d\n",
	            __func__, entry->drive,
	            thpool_num_threads_alive(entry->thpool), index);
	#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
	printf("%s: %s{%d} replaced at %d\n",
	       __func__, entry->drive,
	       thpool_num_threads_alive(entry->thpool), index);
	#endif

	return SUCCESS;
}

//Updates the table with an existing entry, or, adds a new entry if not found.
//returns: 0 on success, -1 on failure.
static int table_entry_update(char *drive, int n_pool_thrds)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START: %s\n", __func__, drive);
	#endif //FUNCTION_ENTRY_DEBUG

	int ret = FAILURE;	//assume table is full

	ret = table_entry_replace(drive, n_pool_thrds);  //find existing entry and update
	if (ret) {
		ret = table_entry_add(drive, n_pool_thrds);  //find empty entry
		if (ret) {
			#if !defined(COMPILE_STANDALONE)
			ilm_log_err("%s: %s NOT updated", __func__, drive);
			#elif defined(COMPILE_STANDALONE)
			printf("%s: %s NOT updated\n", __func__, drive);
			#endif
			return ret;
		}
	}

	#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
	ilm_log_dbg("%s: %s updated", __func__, drive);
	#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
	printf("%s: %s updated\n", __func__, drive);
	#endif
	return SUCCESS;
}

//Removes from the table an existing entry.
static void table_entry_remove(char *drive)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START: %s\n", __func__, drive);
	#endif //FUNCTION_ENTRY_DEBUG

	int index;
	struct table_entry *entry;

	index = _table_entry_find_index(drive);  //find existing entry
	if (index >= 0) {
		entry = table_thpool[index];

		free(entry->drive);
		entry->drive  = NULL;

		thpool_destroy(entry->thpool);
		entry->thpool = NULL;

		#if !defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		ilm_log_dbg("%s: %s removed", __func__, drive);
		#elif defined(COMPILE_STANDALONE) && defined(DEBUG_TABLE__DRIVE_TO_POOL)
		printf("%s: %s removed\n", __func__, drive);
		#endif
	}
}

static void table_destroy(void)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	int i;
	struct table_entry *entry;

	for (i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = table_thpool[i];
		if (entry) {
			if (entry->thpool)
				thpool_destroy(entry->thpool);
			if (entry->drive)
				free(entry->drive);
			free(entry);
			table_thpool[i] = NULL;
		}
	}
}

static void table_show(void)
{
	#ifdef FUNCTION_ENTRY_DEBUG
	printf("%s: START\n", __func__);
	#endif //FUNCTION_ENTRY_DEBUG

	struct table_entry *entry;
	int i;

	for(i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = table_thpool[i];
		if (entry)
			printf("%s:     entry(%p): drive:'%s', pool(%p)\n", __func__, entry, entry->drive, entry->thpool);
		else
			printf("%s:     entry(%p)\n", __func__, entry);
	}
}


/* ============================= MAIN =============================== */

#if MAIN_ACTIVATE_ANI_API
#include <assert.h>

#define DRIVE1	"/dev/nvme1n1"
#define DRIVE2	"/dev/nvme2n1"
#define DRIVE3	"/dev/nvme3n1"

int main(void){
	printf("main\n");

	int ret;
	struct table_entry* entry;

	table_init();

	// Test private FIND - basic
	printf("FIND test - private\n");
	ret = _table_entry_find_empty();
	assert(ret == 0);

	ret = _table_entry_find_index(NULL);//invalid
	assert(ret == -1);
	ret = _table_entry_find_index((char*)DRIVE1);
	assert(ret == -1);
	printf("\n");

	// Test public FIND - basic
	printf("FIND test - public\n");
	entry = table_entry_find(NULL);//invalid
	assert(entry == NULL);
	entry = table_entry_find((char*)DRIVE1);
	assert(entry == NULL);
	printf("\n");

	// Test ADD & public FIND
	printf("ADD test\n");
	ret = table_entry_add((char*)DRIVE1, 4);
	assert(ret == 0);
	entry = table_entry_find((char*)DRIVE1);
	assert(strcmp(entry->drive, (char*)DRIVE1) == 0);
	assert(thpool_num_threads_alive(entry->thpool) == 4);

	ret = _table_entry_find_empty();
	assert(ret == 1);
	table_show();
	printf("\n");

	// Test REPLACE & public FIND
	printf("REPLACE test\n");
	ret = table_entry_replace((char*)DRIVE1, 6);
	table_show();
	assert(ret == 0);
	entry = table_entry_find((char*)DRIVE1);
	assert(strcmp(entry->drive, (char*)DRIVE1) == 0);
	assert(thpool_num_threads_alive(entry->thpool) == 6);

	ret = _table_entry_find_empty();
	assert(ret == 1);
	printf("\n");

	// Test UPDATE & public FIND
	printf("UPDATE test\n");
	ret = table_entry_update((char*)DRIVE1, 8);
	table_show();
	assert(ret == 0);
	entry = table_entry_find((char*)DRIVE1);
	assert(strcmp(entry->drive, DRIVE1) == 0);
	assert(thpool_num_threads_alive(entry->thpool) == 8);

	ret = table_entry_update((char*)DRIVE3, 10);
	assert(ret == 0);
	entry = table_entry_find((char*)DRIVE3);
	assert(strcmp(entry->drive, (char*)DRIVE3) == 0);
	assert(thpool_num_threads_alive(entry->thpool) == 10);

	ret = _table_entry_find_empty();
	assert(ret == 2);
	printf("\n");

	// Just add another entry
	ret = table_entry_update((char*)DRIVE2, 15);
	assert(ret == 0);

	ret = _table_entry_find_empty();
	assert(ret == 3);
	printf("\n");

	// Test REMOVE
	printf("REMOVE test\n");
	table_entry_remove((char*)DRIVE3);

	ret = _table_entry_find_empty();
	assert(ret == 1);


	table_destroy();

	return 0;
}
#endif //MAIN_ACTIVATE_ANI_API
