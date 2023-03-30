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

#include <unistd.h>

#include "async_nvme_thread_pool.h"


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
