/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 */

#ifndef __ASYNC_NVME_THREAD_POOL_H__
#define __ASYNC_NVME_THREAD_POOL_H__

#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


//TODO: Move these constants into pool context????
#define EXTRACT_WAIT_COUNT_MAX	10
#define EXTRACT_WAIT_SLEEP	2
#define NUM_THREADS		4
#define QUEUE_SIZE		8 //256
//TODO: Static queue size for rapid prototype.
//		Dynamic queue depth may be required for production.

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

// Context containing all related variables for a single thread pool.
// This thread pool is used to emulate asychronous command behavior using
// the NVMe command protocol.
struct async_nvme_thrd_pool_cntxt {

	char device[PATH_MAX];

	struct async_nvme_request *async_request_queue[QUEUE_SIZE];
	struct async_nvme_result  *async_result_queue[QUEUE_SIZE];

	int request_count;
	int result_count;
	int wait_count;
	int pool_kill;

	pthread_t              threads[NUM_THREADS];
	pthread_cond_t         cond_request_queue;
	pthread_mutex_t        mutex_request_queue;
	pthread_mutex_t        mutex_result_queue;
	pthread_mutex_t        mutex_pool_kill;
};

struct async_nvme_thrd_pool_cntxt thrd_pool_async_nvme;
//TODO: This single thread pool needs eventually be turned into map
//	Map will be a device string to malloc'd struct pointer


//////////////////////////////
// FUNCTIONS
//////////////////////////////

// int _result_queue_scan(Result *result_target);

void* thread_async_nvme(void *arg);

// void thread_result_extract(Result *result);
// void thread_result_submit(Result *result);
// void thread_task_extract(Task **task);
// void thread_task_submit(Task *task);

int thread_pool_destroy(void);
int thread_pool_init(void);

#endif /*__ASYNC_NVME_THREAD_POOL_H__*/
