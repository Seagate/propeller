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

////////////////////////////////////////////////////////////////////////////////
// COMPILE SWITCHES
////////////////////////////////////////////////////////////////////////////////
/* For using internal main() for stand-alone debug compilation.
Setup to be gcc-defined (-D) in make file */
#ifdef DBG__NVME_ANI_MAIN_ENABLE
#define DBG__NVME_ANI_MAIN_ENABLE 1
#else
#define DBG__NVME_ANI_MAIN_ENABLE 0
#endif

/* Define for logging a function's name each time it is entered. */
#define DBG__LOG_FUNC_ENTRY

/* Define for extra logging on drive-to-threadpool table interactions */
#define DBG__THRD_POOL_TABLE

/* Defines for logging struct field data for important data structs */
#define DBG__DUMP_STRUCTS

////////////////////////////////////////////////////////////////////////////////
// INCLUDES
////////////////////////////////////////////////////////////////////////////////
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

#include "ani_api.h"
#include "idm_nvme_utils.h"
#include "log.h"
#include "thpool.h"

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////
#define MAX_TABLE_ENTRIES		8
#define TABLE_ENTRY_DRIVE_BUFFER_SIZE	32
#define NUM_POOL_THREADS		4

////////////////////////////////////////////////////////////////////////////////
// STRUCTURES
////////////////////////////////////////////////////////////////////////////////
struct table_entry {
	char              drive[PATH_MAX];
	struct threadpool *thpool;
};

////////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////////
//Primary drive-to-threadpool table
struct table_entry *table_thpool[MAX_TABLE_ENTRIES];

////////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// ASYNC NVME INTERFACE(ANI)
////////////////////////////////////////////////////////////////////////////////
int ani_init(void)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	return table_init();
}

//The request_idm already points to the memory locations where the desired data will be stored
//If the result is found, and shows success, then the data has already been written to those memory locations.
//Just return and let the calling code retrieve what it needs.
// Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
int ani_data_rcv(struct idm_nvme_request *request_idm, int *result)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	char *drive = request_idm->drive;
	struct table_entry *entry;
	int ret = FAILURE;

	entry = table_entry_find(drive);
	if (!entry){
		ilm_log_err("%s: find fail: %s", __func__, drive);
		return FAILURE;
	}

	ret = thpool_find_result(entry->thpool, request_idm->uuid_async_job,
	                         10000, 10000, result); //TODO: Fix hard-coded values
	if (ret){
		ilm_log_err("%s: add work fail: %s", __func__, drive);
		ret = FAILURE;
	}

	return ret;
}

// Returns zero or a negative error (ie. EINVAL, ENOMEM, EBUSY, etc).
int ani_send_cmd(struct idm_nvme_request *request_idm, unsigned long ctrl)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

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
			ilm_log_err("%s: add entry fail: %s", __func__, drive);
			ret = FAILURE;
			goto EXIT;
		}
		entry = table_entry_find(drive);
		if (!entry){
			ilm_log_err("%s: find fail: %s", __func__, drive);
			ret = FAILURE;
			goto EXIT;
		}
	}

	//This is kludgy, but needed to simplify how memory was freed for
	//ANI's use of 'struct arg_ioctl' and 'struct nvme_passthru_cmd'.
	//This way, this memory will be freed when request_idm is freed.
	arg = (struct arg_ioctl*)calloc(1, sizeof(*arg));
	if (!arg){
		ilm_log_err("%s: arg calloc fail: drive %s", __func__, drive);
		ret = -ENOMEM;
		goto EXIT;
	}

	cmd = (struct nvme_passthru_cmd *)calloc(1, sizeof(*cmd));
	if (!cmd){
		ilm_log_err("%s: cmd calloc fai: drive %s", __func__, drive);
		ret = -ENOMEM;
		goto EXIT;
	}

	fill_nvme_cmd(request_idm, cmd);
	arg->fd   = request_idm->fd_nvme;
	arg->ctrl = ctrl;
	arg->cmd  = cmd;

	uuid_generate(request_idm->uuid_async_job);

	#ifdef DBG__DUMP_STRUCTS
	dumpNvmePassthruCmd(cmd);
	#endif

	ret = thpool_add_work(entry->thpool, request_idm->uuid_async_job,
	                      (th_func_p)ani_ioctl, (void*)arg);
	if (ret){
		ilm_log_err("%s: add work fail: %s", __func__, drive);
		ret = FAILURE;
	}
EXIT:
	return ret;
}

void ani_destroy(void)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	table_destroy();
}

static int ani_ioctl(void* arg)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct arg_ioctl *arg_ = (struct arg_ioctl *)arg;

	return ioctl(arg_->fd, arg_->ctrl, arg_->cmd);
}


////////////////////////////////////////////////////////////////////////////////
// LOOKUP TABLE
////////////////////////////////////////////////////////////////////////////////
//return: 1 when empty, 0 when NOT empty (so behaves like logical bool)
static int _table_entry_is_empty(struct table_entry *entry)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	if (!entry) return 1; //TODO: This is actually an error.  Should never happen

	if ((!entry->drive) || (!entry->thpool)) return 1;

	return 0;
}

//Finds the index of the first table entry that is emtpy
//return: (int >= 0) on success, -1 on failure(table full)
static int _table_entry_find_empty(void)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

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

	#ifdef DBG__THRD_POOL_TABLE
	if (ret >= 0)
		ilm_log_dbg("%s: empty entry found at %d", __func__, ret);
	else
		ilm_log_dbg("%s: empty entry NOT found", __func__);
	#endif

	return ret;
}

//Finds the index of the first table entry with the matching drive string.
//return: (int >= 0) on success, -1 on failure
static int _table_entry_find_index(char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY w\\ %s", __func__, drive);
	#endif

	int i;
	struct table_entry *entry;
	int ret = FAILURE;

	if (!drive){
		ilm_log_err("%s: invalid param", __func__);
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

	#ifdef DBG__THRD_POOL_TABLE
	if (ret >= 0)
		ilm_log_dbg("%s: %s found at %d", __func__, entry->drive, ret);
	else
		ilm_log_dbg("%s: %s NOT found", __func__, drive);
	#endif

	return ret;
}

//returns: 0 on success, -1 on failure.
static int table_init(void)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int i;
	struct table_entry *entry;

	//If calloc() fails, known state for ALL "entry" when table_destroy() runs
	for (i = 0; i < MAX_TABLE_ENTRIES; i++){
		table_thpool[i] = NULL;		//use memset?
	}

	for (i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = (struct table_entry *)calloc(MAX_TABLE_ENTRIES, sizeof(*entry));
		if (!entry) {
			table_destroy();
			ilm_log_err("%s: calloc failure", __func__);
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
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY w\\ %s", __func__, drive);
	#endif

	int i;
	struct table_entry *entry = NULL;

	if (!drive){
		ilm_log_err("%s: invalid param", __func__);
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

	#ifdef DBG__THRD_POOL_TABLE
	if (entry)
		ilm_log_dbg("%s: %s found", __func__, entry->drive);
	else
		ilm_log_dbg("%s: %s NOT found", __func__, drive);
	#endif

	return entry;
}

//Adds to the table a NEW table entry, ONLY.
//returns: 0 on success, -1 on failure.
static int table_entry_add(char *drive, int n_pool_thrds)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY w\\ %s", __func__, drive);
	#endif

	int index;
	struct table_entry *entry;

	index = _table_entry_find_empty();
	if (index < 0) {
		ilm_log_err("%s: %s{%d} NOT added", __func__, drive, n_pool_thrds);
		return FAILURE;
	}

	entry = table_thpool[index];

	strcpy(entry->drive, drive);

	entry->thpool = thpool_init(n_pool_thrds);
	if (!entry->thpool){
		return FAILURE;
	}

	ilm_log_err("%s: %s{%d} added at %d",
			__func__, entry->drive,
			thpool_num_threads_alive(entry->thpool), index);

	return SUCCESS;
}

//Replaces in the table an EXISTING table entry, ONLY.
//returns: 0 on success, -1 on failure.
static int table_entry_replace(char *drive, int n_pool_thrds)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY w\\ %s", __func__, drive);
	#endif

	int index;
	struct table_entry *entry;

	index = _table_entry_find_index(drive);  //find existing entry and update
	if (index < 0) {
		ilm_log_err("%s: %s{%d} NOT replaced", __func__, drive, n_pool_thrds);
		return FAILURE;
	}

	entry = table_thpool[index];

	thpool_destroy(entry->thpool);
	entry->thpool = thpool_init(n_pool_thrds);
	if (!entry->thpool){
		return FAILURE;
	}

	#ifdef DBG__THRD_POOL_TABLE
	ilm_log_dbg("%s: %s{%d} replaced at %d",
	            __func__, entry->drive,
	            thpool_num_threads_alive(entry->thpool), index);
	#endif

	return SUCCESS;
}

//Updates the table with an existing entry, or, adds a new entry if not found.
//returns: 0 on success, -1 on failure.
__attribute__ ((unused)) static int table_entry_update(char *drive, int n_pool_thrds)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY w\\ %s", __func__, drive);
	#endif

	int ret = FAILURE;	//assume table is full

	ret = table_entry_replace(drive, n_pool_thrds);  //find existing entry and update
	if (ret) {
		ret = table_entry_add(drive, n_pool_thrds);  //find empty entry
		if (ret) {
			ilm_log_err("%s: %s NOT updated", __func__, drive);
			return ret;
		}
	}

	#ifdef DBG__THRD_POOL_TABLE
	ilm_log_dbg("%s: %s updated", __func__, drive);
	#endif

	return SUCCESS;
}

//Removes from the table an existing entry.
__attribute__ ((unused)) static void table_entry_remove(char *drive)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY w\\ %s", __func__, drive);
	#endif

	int index;
	struct table_entry *entry;

	index = _table_entry_find_index(drive);  //find existing entry
	if (index >= 0) {
		entry = table_thpool[index];

		if (entry->drive)
			entry->drive[0]  = '\0';

		if (entry->thpool){
			thpool_destroy(entry->thpool);
			entry->thpool = NULL;
		}

		#ifdef DBG__THRD_POOL_TABLE
		ilm_log_dbg("%s: %s removed", __func__, drive);
		#endif
	}
}

static void table_destroy(void)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	int i;
	struct table_entry *entry;

	for (i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = table_thpool[i];
		if (entry) {
			entry->drive[0]  = '\0';
			if (entry->thpool){
				thpool_destroy(entry->thpool);
			}
			free(entry);
			table_thpool[i] = NULL;
		}
	}
}

__attribute__ ((unused)) static void table_show(void)
{
	#ifdef DBG__LOG_FUNC_ENTRY
	ilm_log_dbg("%s: ENTRY", __func__);
	#endif

	struct table_entry *entry;
	int i;

	for(i = 0; i < MAX_TABLE_ENTRIES; i++){
		entry = table_thpool[i];
		if (entry)
			ilm_log_dbg("%s:     entry(%p): drive:'%s', pool(%p)",
			             __func__, entry, entry->drive, entry->thpool);
		else
			ilm_log_dbg("%s:     entry(%p)", __func__, entry);
	}
}

////////////////////////////////////////////////////////////////////////////////
// DEBUG MAIN
////////////////////////////////////////////////////////////////////////////////
#if DBG__NVME_ANI_MAIN_ENABLE
#include <assert.h>

#define DRIVE1	"/dev/nvme1n1"
#define DRIVE2	"/dev/nvme2n1"
#define DRIVE3	"/dev/nvme3n1"

int main(void){
	printf("main - ani_api\n");

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
	table_show();
	assert(ret == 0);
	entry = table_entry_find((char*)DRIVE3);
	assert(strcmp(entry->drive, (char*)DRIVE3) == 0);
	assert(thpool_num_threads_alive(entry->thpool) == 10);

	ret = _table_entry_find_empty();
	assert(ret == 2);
	printf("\n");

	// Just add another entry
	ret = table_entry_update((char*)DRIVE2, 15);
	table_show();
	assert(ret == 0);

	ret = _table_entry_find_empty();
	assert(ret == 3);
	printf("\n");

	// Test REMOVE
	printf("REMOVE test\n");
	table_entry_remove((char*)DRIVE3);
	table_show();

	ret = _table_entry_find_empty();
	assert(ret == 1);


	table_destroy();

	return 0;
}
#endif //DBG__NVME_ANI_MAIN_ENABLE
