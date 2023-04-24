/* ********************************
 * Author:       Johan Hanssen Seferidis
 * License:	     MIT
 * Description:  Library providing a threading pool where you can add
 *               work. For usage, check the thpool.h file or README.md
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Johan Hanssen Seferidis
 * Copyright (C) 2023 Seagate Technology LLC and/or its Affiliates.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ********************************/

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#include <uuid/uuid.h>

#include "thpool.h"

#ifdef THPOOL_DEBUG
#define THPOOL_DEBUG 1
#else
#define THPOOL_DEBUG 0
#endif

#if !defined(DISABLE_PRINT) || defined(THPOOL_DEBUG)
#define err(str) fprintf(stderr, str)
#else
#define err(str)
#endif



//TODO DUMPING GROUND
//===================
//TODO:(captured) add queue metrics
//TODO: AFTER INTEGRATION: all printf() and err() calls must be replaced will appropriate logging function calls






/* ========================== STRUCTURES ============================ */


/* Binary semaphore */
typedef struct bsem {
	pthread_mutex_t mutex;
	pthread_cond_t   cond;
	int v;
} bsem;

// typedef struct job_metrics {
// 	int    age_queue_in;
// 	int    age_queue_out;
// }job_metrics;

/* Job */
typedef struct job{
	struct job   *prev;          /* pointer to previous job   */

	th_func_p    function;       /* function pointer          */
	void         *arg;            /* function's argument       */

	uuid_t       uuid;           /* job identifier            */
	int          result;         /* job result code           */
//	int          age_queue;      /* generic age for either queue?  Later put in metrics struct? */

// 	struct job_metrics     metrics;
} job;

/* Job queue */
typedef struct jobqueue{
	pthread_mutex_t rwmutex;             /* used for queue r/w access */
	job  *front;                         /* pointer to front of queue */
	job  *rear;                          /* pointer to rear  of queue */
	bsem *has_jobs;                      /* flag as binary semaphore  */
	volatile int len;                    /* number of jobs in queue   */
} jobqueue;


/* Thread */
//TODO: Add a flushing state to the thread (for when a task requestor goes away unexpectedly)
typedef struct thread{
	int       id;                        /* friendly id               */
	pthread_t pthread;                   /* pointer to actual thread  */
	struct threadpool *thpool_p;         /* access to thpool          */
} thread;

/* Threadpool */
typedef struct threadpool{
	thread   **threads;                  /* pointer to threads        */

	volatile int num_threads_alive;      /* threads currently alive   */
	volatile int num_threads_working;    /* threads currently working */
	pthread_mutex_t  thcount_lock;       /* used for thread count etc */
	pthread_cond_t  threads_all_idle;    /* signal to thpool_wait     */

	volatile int threads_keepalive;      /* live\die status flag      */
	volatile int threads_on_hold;        /* run\pause status flag     */
	pthread_mutex_t  alive_lock;         /* used for thpool run state */

	jobqueue  queue_in;                  /* queue for pending jobs    */
	jobqueue  queue_out;                 /* queue for completed jobs  */
} threadpool;


#define MAX_QUEUE_SIZE_WITHOUT_WARNING      100


/* ========================== PROTOTYPES ============================ */


static int   thread_init(struct threadpool *thpool_p, struct thread **thread_p, int id);
static void* thread_do(struct thread *thread_p);
static void  thread_hold(int sig_id);
static void  thread_destroy(struct thread *thread_p);

static int   jobqueue_init(jobqueue *jobqueue_p);
static void  jobqueue_clear(jobqueue *jobqueue_p);
static void  jobqueue_push(jobqueue *jobqueue_p, struct job *newjob_p);
static struct job* jobqueue_pull_front(jobqueue *jobqueue_p);
static struct job* jobqueue_pull_by_uuid(jobqueue *jobqueue_p, uuid_t uuid_job);
static int   jobqueue_length(jobqueue *jobqueue_p);
static void  jobqueue_destroy(jobqueue *jobqueue_p);

static int   bsem_init(struct bsem *bsem_p, int value);
static void  bsem_reset(struct bsem *bsem_p);
static void  bsem_post(struct bsem *bsem_p);
static void  bsem_post_all(struct bsem *bsem_p);
static void  bsem_wait(struct bsem *bsem_p);
static void  bsem_destroy(struct bsem *bsem_p);





/* ========================== THREADPOOL ============================ */


/* Initialise thread pool */
struct threadpool* thpool_init(int num_threads){

	if (num_threads < 0){
		num_threads = 0;
	}

	/* Make new thread pool */
	struct threadpool *thpool_p;
	thpool_p = (struct threadpool*)malloc(sizeof(struct threadpool));
	if (thpool_p == NULL){
		err("thpool_init(): Could not allocate memory for thread pool\n");
		return NULL;
	}
	thpool_p->num_threads_alive   = 0;
	thpool_p->num_threads_working = 0;
	thpool_p->threads_on_hold     = 0;
	thpool_p->threads_keepalive   = 1;

	/* Initialise the job queue */
	if (jobqueue_init(&thpool_p->queue_in) == -1){
		err("thpool_init(): Could not allocate memory for input job queue\n");
		free(thpool_p);
		return NULL;
	}

	if (jobqueue_init(&thpool_p->queue_out) == -1){
		err("thpool_init(): Could not allocate memory for output job queue\n");
		jobqueue_destroy(&thpool_p->queue_in);
		free(thpool_p);
		return NULL;
	}

	/* Make threads in pool */
	thpool_p->threads = (struct thread**)malloc(num_threads * sizeof(struct thread *));
	if (thpool_p->threads == NULL){
		err("thpool_init(): Could not allocate memory for threads\n");
		jobqueue_destroy(&thpool_p->queue_out);
		jobqueue_destroy(&thpool_p->queue_in);
		free(thpool_p);
		return NULL;
	}

	pthread_mutex_init(&(thpool_p->thcount_lock), NULL);
	pthread_mutex_init(&(thpool_p->alive_lock), NULL);
	pthread_cond_init(&thpool_p->threads_all_idle, NULL);

	/* Thread init */
	int ret;
	int n;
	for (n=0; n<num_threads; n++){
		ret = thread_init(thpool_p, &thpool_p->threads[n], n);
		if (ret) {
			thpool_destroy(thpool_p);
			return NULL;
		}
	}

	/* Wait for threads to initialize */
	struct timespec ts;
	int wait_count = 0;

	ts.tv_sec  = 0;
	ts.tv_nsec = 100;
	while (thpool_p->num_threads_alive != num_threads){
		nanosleep(&ts, &ts);
		wait_count++;
		if (wait_count > 100000000){//Kludge to give 10 sec max wait
#if THPOOL_DEBUG
			printf("THPOOL_DEBUG: %s: Timeout waiting for all threads\n",
			       __func__);
#endif
			thpool_destroy(thpool_p);
			return NULL;
		}
	}

	return thpool_p;
}


/* Add work to the thread pool */
int thpool_add_work(struct threadpool *thpool_p, uuid_t uuid_job,
                    th_func_p func_p, void *arg_p){
	job *newjob;

	newjob=(struct job*)malloc(sizeof(struct job));
	if (newjob==NULL){
		err("thpool_add_work(): Could not allocate memory for new job\n");
		return -1;
	}

	/* add function and argument */
	newjob->function=func_p;
	newjob->arg=arg_p;

	newjob->prev=NULL;
	uuid_copy(newjob->uuid, uuid_job);

	/* add job to queue */
	jobqueue_push(&thpool_p->queue_in, newjob);

	return 0;
}

/* Extract result from thread pool */
int thpool_find_result(struct threadpool *thpool_p, uuid_t uuid_job,
                       int retry_count_max, int retry_interval_ns, int *result_p){

	struct timespec ts;
	job *completed_job;
	int retry_count = 0;
	int result_found = 0;

	ts.tv_sec  = 0;
	ts.tv_nsec = retry_interval_ns;

	while(retry_count < retry_count_max){

		completed_job = jobqueue_pull_by_uuid(&thpool_p->queue_out, uuid_job);
		if (completed_job){
			*result_p = completed_job->result;
			free(completed_job);
			result_found = 1;
			break;
		}
		else{
			nanosleep(&ts, &ts);
		}
		retry_count++;
	}

#if THPOOL_DEBUG
	unsigned char uuid_str[37];
	uuid_unparse(uuid_job, uuid_str);
#endif
	if (result_found){
#if THPOOL_DEBUG
		printf("THPOOL_DEBUG: %s: job(%p) found: uuid %s\n",
		       __func__, completed_job, uuid_str);
#endif
		return 0;
	}
	else{
#if THPOOL_DEBUG
		printf("THPOOL_DEBUG: %s: job NOT found: uuid %s\n",
		       __func__, uuid_str);
#endif
		return -1;
	}
}


/* Wait until all jobs have finished */
//TODO: Hardcoded for "thpool_p->queue_in".
//		Can "thpool_p->queue_out" even use this concept?
//				(queue_out NOT GUARENTEED to be emptied out via thpool_find_results())
//			If NOT, rename function?
void thpool_wait(struct threadpool *thpool_p){
	pthread_mutex_lock(&thpool_p->thcount_lock);
	while (jobqueue_length(&thpool_p->queue_in) || thpool_p->num_threads_working) {
		pthread_cond_wait(&thpool_p->threads_all_idle, &thpool_p->thcount_lock);
	}
	pthread_mutex_unlock(&thpool_p->thcount_lock);
}


/* Destroy the threadpool */
/* Retrieve any desired output before calling this destroy */
void thpool_destroy(struct threadpool *thpool_p){
	/* No need to destroy if it's NULL */
	if (thpool_p == NULL) return ;

	volatile int threads_total = thpool_num_threads_alive(thpool_p);

	/* End each thread 's infinite loop */
	pthread_mutex_lock(&thpool_p->alive_lock);
	thpool_p->threads_keepalive = 0;
	pthread_mutex_unlock(&thpool_p->alive_lock);

	/* Give one second to kill idle threads */
	double TIMEOUT = 1.0;
	time_t start, end;
	double tpassed = 0.0;
	time (&start);
	while (tpassed < TIMEOUT && thpool_num_threads_alive(thpool_p)){
		bsem_post_all(thpool_p->queue_in.has_jobs);
		time (&end);
		tpassed = difftime(end,start);
	}

	/* Poll remaining threads */
	while (thpool_num_threads_alive(thpool_p)){
		bsem_post_all(thpool_p->queue_in.has_jobs);
		sleep(1);
	}

	/* Job queue cleanup */
	jobqueue_destroy(&thpool_p->queue_out);
	jobqueue_destroy(&thpool_p->queue_in);
	/* Deallocs */
	int n;
	for (n=0; n < threads_total; n++){
		thread_destroy(thpool_p->threads[n]);
	}
	free(thpool_p->threads);
	pthread_mutex_destroy(&thpool_p->thcount_lock);
	pthread_mutex_destroy(&thpool_p->alive_lock);
	pthread_cond_destroy(&thpool_p->threads_all_idle);
	free(thpool_p);
}


/* Pause all threads in threadpool */
void thpool_pause(struct threadpool *thpool_p) {
	int n;
	for (n=0; n < thpool_num_threads_alive(thpool_p); n++){
		pthread_kill(thpool_p->threads[n]->pthread, SIGUSR1);
	}
	thpool_p->threads_on_hold = 1;
}


/* Resume all threads in threadpool */
void thpool_resume(struct threadpool *thpool_p) {
	int n;
	for (n=0; n < thpool_num_threads_alive(thpool_p); n++){
		pthread_kill(thpool_p->threads[n]->pthread, SIGUSR2);
	}
	thpool_p->threads_on_hold = 0;
}


int thpool_num_threads_alive(struct threadpool *thpool_p){
	int num;
	pthread_mutex_lock(&thpool_p->thcount_lock);
	num = thpool_p->num_threads_alive;
	pthread_mutex_unlock(&thpool_p->thcount_lock);
	return num;
}


int thpool_num_threads_working(struct threadpool *thpool_p){
	int num;
	pthread_mutex_lock(&thpool_p->thcount_lock);
	num = thpool_p->num_threads_working;
	pthread_mutex_unlock(&thpool_p->thcount_lock);
	return num;
}


int thpool_queue_out_len(struct threadpool *thpool_p){
	return jobqueue_length(&thpool_p->queue_out);
}


int thpool_alive_state(struct threadpool *thpool_p){
	int state;
	pthread_mutex_lock(&thpool_p->alive_lock);
	state = thpool_p->threads_keepalive;
	pthread_mutex_unlock(&thpool_p->alive_lock);
	return state;
}



/* ============================ THREAD ============================== */


/* Initialize a thread in the thread pool
 *
 * @param thread        address to the pointer of the thread to be created
 * @param id            id to be given to the thread
 * @return 0 on success, -1 otherwise.
 */
//TODO: Change thread id to set internally???
static int thread_init (struct threadpool *thpool_p, struct thread** thread_p, int id){

	*thread_p = (struct thread*)malloc(sizeof(struct thread));
	if (*thread_p == NULL){
		err("thread_init(): Could not allocate memory for thread\n");
		return -1;
	}

	(*thread_p)->thpool_p = thpool_p;
	(*thread_p)->id       = id;

	pthread_create(&(*thread_p)->pthread, NULL, (void * (*)(void *)) thread_do, (*thread_p));
	pthread_detach((*thread_p)->pthread);
#if THPOOL_DEBUG
	printf("THPOOL_DEBUG: %s: Thread created (id:%d)\n", __func__, id);
#endif
	return 0;
}


/* Sets the calling thread on hold */
static void thread_hold(int sig_id) {
	switch(sig_id) {
		case SIGUSR1:
			pause();
			break;
		case SIGUSR2:
			break;
		default:
			err("thread_hold(): Invalid sig_id");
			break;
	}
}


/* What each thread is doing
*
* In principle this is an endless loop. The only time this loop gets interuppted is once
* thpool_destroy() is invoked or the program exits.
*
* @param  thread        thread that will run this function
* @return nothing
*/
static void* thread_do(struct thread* thread_p){

	/* Set thread name for profiling and debugging */
	char thread_name[16] = {0};
	snprintf(thread_name, 16, "thpool-%d", thread_p->id);
//TODO: Set thread "id" here instead (using pthread_self())????  Use both??
#if THPOOL_DEBUG
	printf("THPOOL_DEBUG: %s: Thread started (id:%d, pthread:%u)\n",
	       __func__, thread_p->id, (unsigned int)pthread_self());
#endif

#if defined(__linux__)
	/* Use prctl instead to prevent using _GNU_SOURCE flag and implicit declaration */
	prctl(PR_SET_NAME, thread_name);
#elif defined(__APPLE__) && defined(__MACH__)
	pthread_setname_np(thread_name);
#else
	err("thread_do(): pthread_setname_np is not supported on this system");
#endif

	/* Assure all threads have been created before starting serving */
	struct threadpool *thpool_p = thread_p->thpool_p;

	/* Register signal handler */
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = thread_hold;
	if (sigaction(SIGUSR1, &act, NULL) == -1) {
		err("thread_do(): cannot handle SIGUSR1");
	}
	if (sigaction(SIGUSR2, &act, NULL) == -1) {
		err("thread_do(): cannot handle SIGUSR2");
	}

	/* Mark thread as alive (initialized) */
	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive++;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	struct timespec ts;
	ts.tv_sec  = 0;
	ts.tv_nsec = 1;

	while(thpool_alive_state(thpool_p)){

		bsem_wait(thpool_p->queue_in.has_jobs);

		if (thpool_alive_state(thpool_p)){

			pthread_mutex_lock(&thpool_p->thcount_lock);
			thpool_p->num_threads_working++;
			pthread_mutex_unlock(&thpool_p->thcount_lock);

			/* Read job from queue and execute it */
			th_func_p func_buff;
			void*  arg_buff;
			job* job_p = jobqueue_pull_front(&thpool_p->queue_in);
			if (job_p) {
				func_buff     = job_p->function;
				arg_buff      = job_p->arg;
				job_p->result = func_buff(arg_buff);
				jobqueue_push(&thpool_p->queue_out, job_p);
			}

			pthread_mutex_lock(&thpool_p->thcount_lock);
			thpool_p->num_threads_working--;
			if (!thpool_p->num_threads_working) {
				pthread_cond_signal(&thpool_p->threads_all_idle);
			}
			pthread_mutex_unlock(&thpool_p->thcount_lock);

			nanosleep(&ts, &ts);     /* Allow other threads CPU time */
		}
	}
	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive--;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	return NULL;
}


/* Frees a thread  */
static void thread_destroy (thread* thread_p){
	free(thread_p);
}





/* ============================ JOB QUEUE =========================== */


/* Initialize queue */
static int jobqueue_init(jobqueue* jobqueue_p){
	int ret = -1;

	jobqueue_p->len = 0;
	jobqueue_p->front = NULL;
	jobqueue_p->rear  = NULL;

	jobqueue_p->has_jobs = (struct bsem*)malloc(sizeof(struct bsem));
	if (jobqueue_p->has_jobs == NULL){
		return ret;
	}

	pthread_mutex_init(&(jobqueue_p->rwmutex), NULL);
	ret = bsem_init(jobqueue_p->has_jobs, 0);

	return ret;
}


/* Clear the queue */
static void jobqueue_clear(jobqueue* jobqueue_p){

	while(jobqueue_length(jobqueue_p)){
		free(jobqueue_pull_front(jobqueue_p));
	}

	pthread_mutex_lock(&jobqueue_p->rwmutex);
	jobqueue_p->front = NULL;
	jobqueue_p->rear  = NULL;
	bsem_reset(jobqueue_p->has_jobs);
	jobqueue_p->len = 0;
	pthread_mutex_unlock(&jobqueue_p->rwmutex);
}


/* Add (allocated) job to queue
 */
static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob){

	pthread_mutex_lock(&jobqueue_p->rwmutex);
	newjob->prev = NULL;

	switch(jobqueue_p->len){

		case 0:  /* if no jobs in queue */
			jobqueue_p->front = newjob;
			jobqueue_p->rear  = newjob;
			break;

		default: /* if jobs in queue */
			jobqueue_p->rear->prev = newjob;
			jobqueue_p->rear = newjob;
	}
	jobqueue_p->len++;
	if (jobqueue_p->len > MAX_QUEUE_SIZE_WITHOUT_WARNING)
		printf("%s: WARNING: queue len > %d\n",
		       __func__, MAX_QUEUE_SIZE_WITHOUT_WARNING);

	bsem_post(jobqueue_p->has_jobs);
	pthread_mutex_unlock(&jobqueue_p->rwmutex);
#if THPOOL_DEBUG
	unsigned char uuid_str[37];
	uuid_unparse(newjob->uuid, uuid_str);
	printf("THPOOL_DEBUG: %s: job(%p), uuid=%s, queue(%p), pthread(%u))\n",
	       __func__, newjob, uuid_str, jobqueue_p, (unsigned int)pthread_self());
#endif
}


/* Get first job from queue(removes it from queue)
 * Notice: Caller MUST hold a mutex
 */
static struct job* jobqueue_pull_front(jobqueue* jobqueue_p){

	pthread_mutex_lock(&jobqueue_p->rwmutex);
	job* job_p = jobqueue_p->front;

	switch(jobqueue_p->len){

		case 0:  /* if no jobs in queue */
			break;

		case 1:  /* if one job in queue */
			jobqueue_p->front = NULL;
			jobqueue_p->rear  = NULL;
			jobqueue_p->len = 0;
			break;

		default: /* if >1 jobs in queue */
			jobqueue_p->front = job_p->prev;
			jobqueue_p->len--;
			if (jobqueue_p->len > MAX_QUEUE_SIZE_WITHOUT_WARNING)
				printf("%s: WARNING: queue len > %d\n",
				       __func__, MAX_QUEUE_SIZE_WITHOUT_WARNING);
			/* more than one job in queue -> post it */
			bsem_post(jobqueue_p->has_jobs);
	}

	pthread_mutex_unlock(&jobqueue_p->rwmutex);
#if THPOOL_DEBUG
	if (job_p){
		unsigned char uuid_str[37];
		uuid_unparse(job_p->uuid, uuid_str);
		printf("THPOOL_DEBUG: %s: found job(%p), uuid=%s, queue(%p), pthread(%u))\n",
		       __func__, job_p, uuid_str, jobqueue_p, (unsigned int)pthread_self());
	}
	else
		printf("THPOOL_DEBUG: %s: NO jobs found, queue(%p), pthread(%u))\n",
		       __func__, jobqueue_p, (unsigned int)pthread_self());
#endif

	return job_p;
}


/* Search for job uuid
 * Notice: Caller MUST hold a mutex
 */
// returned NULL indicates NOT FOUND
static struct job* jobqueue_pull_by_uuid(jobqueue* jobqueue_p, uuid_t uuid_job){

/*
TODO: Do I want to implement "trylock" like I did in POC?  If so, how?
		Pass in returned job pointer as param instead
		Use return value for error code from trylock
	Leave this work for AFTER it's in Propeller?
	WILL NEED: cuz drives may "vanish" unexpectedly.
		Don't want to endlessly wait for a drive that's no longer there.
*/
	pthread_mutex_lock(&jobqueue_p->rwmutex);

	job* curr_job_p = jobqueue_p->front;  /* scan queue front to back */
	job* last_job_p = NULL;  //Make queue a double-linked list?

	while (curr_job_p){
		if (uuid_compare(curr_job_p->uuid, uuid_job) == 0){
			break;
		}
		last_job_p = curr_job_p;
		curr_job_p = curr_job_p->prev;
	}

	if (curr_job_p){
		switch (jobqueue_p->len){

			case 0:  /* if no jobs in queue */
				break;

			case 1:  /* if one job in queue */
				jobqueue_p->front = NULL;
				jobqueue_p->rear  = NULL;
				jobqueue_p->len = 0;
				break;

			default: /* if >1 jobs in queue */
				if (!last_job_p) {
					/* Current job at queue front */
					jobqueue_p->front = curr_job_p->prev;
				}
				else if (!curr_job_p->prev){
					/* Current job at queue rear */
					jobqueue_p->rear = last_job_p;
					last_job_p->prev = NULL;
				}
				else {
					/* Current job somewhere in the middle */
					last_job_p->prev = curr_job_p->prev;
				}

				jobqueue_p->len--;
				if (jobqueue_p->len > MAX_QUEUE_SIZE_WITHOUT_WARNING)
					printf("%s: WARNING: queue len > %d\n",
					       __func__, MAX_QUEUE_SIZE_WITHOUT_WARNING);
				/* more than one job in queue -> post it */
				bsem_post(jobqueue_p->has_jobs);
		}
	}

	pthread_mutex_unlock(&jobqueue_p->rwmutex);
#if THPOOL_DEBUG
	unsigned char uuid_str[37];
	if (curr_job_p){
		uuid_unparse(curr_job_p->uuid, uuid_str);
		printf("THPOOL_DEBUG: %s: found job(%p), uuid=%s, queue(%p), pthread(%u))\n",
		       __func__, curr_job_p, uuid_str, jobqueue_p, (unsigned int)pthread_self());
	}
	else{
		uuid_unparse(uuid_job, uuid_str);
		printf("THPOOL_DEBUG: %s: job NOT found, uuid=%s, queue(%p), pthread(%u))\n",
		       __func__, uuid_str, jobqueue_p, (unsigned int)pthread_self());
	}
#endif
	return curr_job_p;
}


/* Get the queue's current length */
static int jobqueue_length(jobqueue* jobqueue_p){
	int len;
	pthread_mutex_lock(&jobqueue_p->rwmutex);
	len = jobqueue_p->len;
	pthread_mutex_unlock(&jobqueue_p->rwmutex);

	return len;
}


/* Free all queue resources back to the system */
static void jobqueue_destroy(jobqueue* jobqueue_p){
	jobqueue_clear(jobqueue_p);
	pthread_mutex_destroy(&jobqueue_p->rwmutex);
	bsem_destroy(jobqueue_p->has_jobs);
	free(jobqueue_p->has_jobs);
}





/* ======================== SYNCHRONISATION ========================= */


/* Init semaphore to 1 or 0 */
static int bsem_init(bsem *bsem_p, int value) {
	if (value < 0 || value > 1) {
		err("bsem_init(): Binary semaphore can take only values 1 or 0");
		return -1;
	}
	pthread_mutex_init(&(bsem_p->mutex), NULL);
	pthread_cond_init(&(bsem_p->cond), NULL);
	bsem_p->v = value;

	return 0;
}


/* Reset semaphore to 0 */
static void bsem_reset(bsem *bsem_p) {
	bsem_init(bsem_p, 0);
}


/* Post to at least one thread */
static void bsem_post(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_signal(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Post to all threads */
static void bsem_post_all(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_broadcast(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Wait on semaphore until semaphore has value 0 */
static void bsem_wait(bsem* bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	while (bsem_p->v != 1) {
		pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
	}
	bsem_p->v = 0;
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Wait on semaphore until semaphore has value 0 */
static void bsem_destroy(bsem* bsem_p) {
	pthread_mutex_destroy(&(bsem_p->mutex));
	pthread_cond_destroy(&(bsem_p->cond));
}
