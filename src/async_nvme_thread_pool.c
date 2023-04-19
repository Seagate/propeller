/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 *
 *
 * On Service Starup
 * =================
 * 1.) Create\Malloc Async NVMe Thread Pool
 *
 * Normal IDM Async Thread-Safe Operations
 * =======================================
 * 1.) Submit to request_queue		(main thread - phase 1 async)
 * 2.) Extract from request_queue	(pool thread)
 * 3.) Call IOCTL() 			(pool thread)
 * 4.) Submit to result_queue		(pool thread)
 * 5.) Extract from result_queue	(main thread - phase 2 async)
 *
 * On Service Stop
 * ===============
 * 1.) Free\Destroy Async NVMe Thread Pool
 *
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "async_nvme_thread_pool.h"

/*

//Used on MAIN thread				Replacement for main thread "read()"
void thread_result_extract(Result *result_target)
{
	printf("%s: start\n", __func__);

	int res;
	int found;



//TODO: timeout control how long stay in while loop??????
	while(1) {
//TODO: If something gets "stuck\forgotten about" in the result queue, this is NEVER true after that
		if (result_count == 0) {
			//Sleep\Wait -OR- fail if waiting too long
			//If sleep, thn continue after sleep
		}

		res = pthread_mutex_trylock(&mutex_result_queue);
		printf("%s: trylock result: %d\n", __func__, res);

		switch(res) {
		case 0:
			//lock acquired.
			//scan result_queue for desired result				// HOW IDENTIFY RESULT????
				//if FOUND
					//extract result from queue.
					//unlock mutex.
					//leave while loop
				//If NOT_FOUND
					//unlock mutex.
					//Sleep\Wait -OR- fail if waiting too long
			if (_result_queue_scan(result_target) == 0) {
				pthread_mutex_unlock(&mutex_result_queue);  //TODO: Move into func
				goto EXIT;
			}
			pthread_mutex_unlock(&mutex_result_queue);
			sleep(1);
			break;
		case EBUSY:
			//lock NOT acquired.
			//Sleep\Wait -OR- fail if waiting too long
			printf("%s: lock busy!\n", __func__);
			sleep(1);
			//TODO: 1ms per queue_depth
			break;
		default:
			printf("%s: Result extraction failed!\n", __func__);
			goto EXIT;
		}

		sleep(1);
	}
EXIT:
	printf("%s: exit\n", __func__);
}

//Mutex is assumed acquired
int _result_queue_scan(Result *result_target)
{
	printf("%s: start\n", __func__);

	int i, j;
	int ret = -1;
	Result *result_candidate;

	printf("Searchig for: result_target addr: 0x%p\n", result_target);
	for(i = 0; i < result_count; i++) {
		result_candidate = result_queue[i];
		printf("found: result_candidate addr: 0x%p\n", result_candidate);
//TODO: Is this ok, for finding the unique result matched to the specific task??
		if (result_target == result_candidate) {
			printf("found result\n");
			for (j = i; j < result_count - 1; j++) {
				result_queue[j] = result_queue[j + 1];
			}
			result_count--;
			ret = 0;
			goto EXIT;
		}
	}

EXIT:
	printf("%s: exit\n", __func__);
	return ret;
}

//Used on POOL thread
void thread_result_submit(Result *result)
{
	printf("%s: start\n", __func__);

	pthread_mutex_lock(&mutex_result_queue);

	result_queue[result_count] = result;
	result_count++;

	pthread_mutex_unlock(&mutex_result_queue);

	//no result signal
	printf("%s: result submitted\n", __func__);
}

//Used on POOL thread
void thread_task_extract(Task **task)
{
	printf("%s: start\n", __func__);

	int i;
	pthread_mutex_lock(&mutex_task_queue);

	while (task_count == 0) {
		printf("%s: enter wait\n", __func__);
		pthread_cond_wait(&cond_task_queue, &mutex_task_queue);
		printf("%s: leave wait\n", __func__);

		if(pool_kill)
			goto EXIT;
	}

	*task = task_queue[0];
	for (i = 0; i < task_count - 1; i++) {
		task_queue[i] = task_queue[i + 1];
	}
	task_count--;
//TODO: Need better fifo, just move head and tail pointers??????
//TODO:  C lib for thread-safe fifo?????
//TODO: Instead of queue, use a map-style object??  hash-map??


EXIT:
	pthread_mutex_unlock(&mutex_task_queue);
	if (pool_kill)
		printf("%s: time to die\n", __func__);
	else
		printf("%s: task extracted\n", __func__);
}

//Used on MAIN thread				Replacement for main thread "write()"
void thread_request_submit(struct async_nvme_request *requent_async)
{
	printf("%s: start\n", __func__);

	pthread_mutex_lock(&mutex_task_queue);

	task_queue[task_count] = requent_async;
	task_count++;

	pthread_mutex_unlock(&mutex_task_queue);

	//TODO: Don't want these debug printf's within the mutex lock??
	printf("%s: request addr: 0x%p\n", __func__, requent_async);
	printf("%s: result addr: 0x%p\n", __func__, requent_async->async_result);

	pthread_cond_signal(&cond_task_queue);  // signal thread pool

	printf("%s: task submitted\n", __func__);
}

*/





//Worker thread
void* thread_async_nvme(void *arg) {

	int id = *(int*)arg;
	free(arg);
	printf("%s(%d): start\n", __func__, id);

// 	Task *task = NULL;

	pthread_mutex_lock(&(thrd_pool_async_nvme.mutex_pool_kill));
	while(thrd_pool_async_nvme.pool_kill == 0) {
		pthread_mutex_unlock(&(thrd_pool_async_nvme.mutex_pool_kill));

// 		printf("%s(%d) while loop start\n", __func__, id);
// 		thread_task_extract(&task);
		sleep(1);

		pthread_mutex_lock(&(thrd_pool_async_nvme.mutex_pool_kill));
		if (thrd_pool_async_nvme.pool_kill) {
			pthread_mutex_unlock(&(thrd_pool_async_nvme.mutex_pool_kill));
			printf("%s(%d): while exit\n", __func__, id);
			goto EXIT;
		}
		pthread_mutex_unlock(&(thrd_pool_async_nvme.mutex_pool_kill));

// 		printf("%s(%d): fake ioctl(): %d, %d\n", __func__, id,
// 		                                        task->fd, task->ctl);
// 		sleep(1);
// 		task->result->ret_ioctl = 9;//dummy result

// 		thread_result_submit(task->result);

	}
EXIT:
	printf("%s(%d): dieing\n", __func__, id);
	return 0;
}

//TODO: Need to pass in device name (when using device to pool map)
int thread_pool_init(void)
{
	printf("%s\n", __func__);

	struct async_nvme_thrd_pool_cntxt *thrd_pool = &thrd_pool_async_nvme;

	pthread_cond_init(&(thrd_pool->cond_request_queue), NULL);
	pthread_mutex_init(&(thrd_pool->mutex_request_queue), NULL);
	pthread_mutex_init(&(thrd_pool->mutex_result_queue), NULL);
	pthread_mutex_init(&(thrd_pool->mutex_pool_kill), NULL);

	thrd_pool->request_count = 0;
	thrd_pool->result_count  = 0;
	thrd_pool->pool_kill     = 0;

	int i;
	for (i = 0; i < NUM_THREADS; i++) {
		int *id = malloc(sizeof(int));	//memory freed in thread
		*id = i;
		if (pthread_create(&(thrd_pool->threads[i]),
		                   NULL,
		                   &thread_async_nvme,
		                   (void*)id) != 0) {
			printf("thread creation failed\n");
		}
	}

	return 0;
}

int thread_pool_destroy(void)
{
	printf("%s\n", __func__);

	int i;
	int ret;
	struct async_nvme_thrd_pool_cntxt *thrd_pool = &thrd_pool_async_nvme;

	pthread_mutex_lock(&(thrd_pool->mutex_pool_kill));
	thrd_pool->pool_kill = 1;
	pthread_mutex_unlock(&(thrd_pool->mutex_pool_kill));

	// signal entire thread pool to exit wait
	pthread_cond_broadcast(&(thrd_pool->cond_request_queue));

	for (i = 0; i < NUM_THREADS; i++) {
		ret = pthread_join(thrd_pool->threads[i], NULL);
		if(ret) {
			printf("Thread join failed: %d\n", ret);
		}
	}

	pthread_cond_destroy(&(thrd_pool->cond_request_queue));
	pthread_mutex_destroy(&(thrd_pool->mutex_request_queue));
	pthread_mutex_destroy(&(thrd_pool->mutex_result_queue));
	pthread_mutex_destroy(&(thrd_pool->mutex_pool_kill));

	return 0;
}

