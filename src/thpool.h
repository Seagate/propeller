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

#ifndef _THPOOL_
#define _THPOOL_

#ifdef __cplusplus
extern "C" {
#endif

/* =================================== API ======================================= */

struct threadpool;

typedef	int (*th_func_p)(void *arg);       /* function pointer          */


/**
 * @brief  Initialize threadpool
 *
 * Initializes a threadpool. This function will not return until all
 * threads have initialized successfully.
 *
 * @example
 *
 *    ..
 *    threadpool thpool;                     //First we declare a threadpool
 *    thpool = thpool_init(4);               //then we initialize it to 4 threads
 *    ..
 *
 * @param  num_threads         number of threads to be created in the threadpool
 * @return struct threadpool*  created threadpool on success,
 *                             NULL on error
 */
struct threadpool* thpool_init(int num_threads);


/**
 * @brief Add work to the pool's input job queue
 *
 * Takes an action and its argument and adds it to the threadpool's input job
 * queue.  If you want to add to work a function with more than one arguments
 * then a way to implement this is by passing a pointer to a structure.
 *
 * NOTICE: You have to cast both the function and argument to not get warnings.
 *
 * @example
 *
 *    void print_num(int num){
 *       printf("%d\n", num);
 *    }
 *
 *    int main() {
 *       ..
 *       int a = 10;
 *       thpool_add_work(thpool_p, job_uuid, (void*)print_num, (void*)a);
 *       ..
 *    }
 *
 * @param  thpool_P      threadpool to which the work will be added
 * @param  uuid_job      unique job identifier
 * @param  func_p        pointer to function to add as work
 * @param  arg_p         pointer to an argument
 * @return 0 on success, -1 otherwise.
 */
int thpool_add_work(struct threadpool *thpool_p, uuid_t uuid_job,
                    th_func_p func_p, void *arg_p);


/**
 * @brief Searches for completed job and, if found, retrieves it's result
 *
 * Each job has a single result
 * Each job is identified by a specific job_uuid.
 * The result is the return value from the executed job's function pointer.
 *
 * NOTICE: After thpool_add_work() is called, if this function is called too
 * soon, or the rety values are too small, the desired job_uuid may not
 * be found.
 *
 * @example
 *
 *    void print_num(int num){
 *       printf("%d\n", num);
 *    }
 *
 *    int main() {
 *       ..
 *       int a = 10;
 *       thpool_add_work(thpool_p, job_uuid, (void*)print_num, (void*)a);
 *       ..
 *       int res = 10;
 *       int ret;
 *       ret = thpool_find_result(thpool_p, job_uuid, 1000, 1000, &res);
 *       ..
 *    }
 *
 * @param  thpool_p              threadpool to which the work will be added
 * @param  job_uuid              unique job identifier to search for in queue_out
 * @param  retry_count_max       max retries for job_uuid search
 * @param  retry_interval_ns     wait time between job_uuid searches in nsec
 * @param  result_p              returned result from function pointer execution (-1 if result NOT found)
 * @return 0 on success, -1 otherwise.
 * 			Currently, -1 only represents a "job not found" condition
 */
int thpool_find_result(struct threadpool *thpool_p, uuid_t job_uuid,
                       int retry_count_max, int retry_interval_ns, int *result_p);


/**
 * @brief Wait for all queued input jobs to finish
 *
 * Will wait for all jobs - both queued and currently running to finish.
 * Once the queue is empty and all work has completed, the calling thread
 * (probably the main program) will continue.
 *
 * Smart polling is used in wait. The polling is initially 0 - meaning that
 * there is virtually no polling at all. If after 1 seconds the threads
 * haven't finished, the polling interval starts growing exponentially
 * until it reaches max_secs seconds. Then it jumps down to a maximum polling
 * interval assuming that heavy processing is being used in the threadpool.
 *
 * @example
 *
 *    ..
 *    threadpool thpool = thpool_init(4);
 *    ..
 *    // Add a bunch of work
 *    ..
 *    thpool_wait(thpool);
 *    puts("All added work has finished");
 *    ..
 *
 * @param thpool_p     the threadpool to wait for
 * @return nothing
 */
void thpool_wait(struct threadpool *thpool_p);


/**
 * @brief Pauses all threads immediately
 *
 * The threads will be paused no matter if they are idle or working.
 * The threads return to their previous states once thpool_resume
 * is called.
 *
 * While the thread is being paused, new work can be added.
 *
 * @example
 *
 *    threadpool thpool = thpool_init(4);
 *    thpool_pause(thpool);
 *    ..
 *    // Add a bunch of work
 *    ..
 *    thpool_resume(thpool); // Let the threads start their magic
 *
 * @param thpool_p    the threadpool where the threads should be paused
 * @return nothing
 */
void thpool_pause(struct threadpool *thpool_p);


/**
 * @brief Unpauses all threads if they are paused
 *
 * @example
 *    ..
 *    thpool_pause(thpool);
 *    sleep(10);              // Delay execution 10 seconds
 *    thpool_resume(thpool);
 *    ..
 *
 * @param thpool_p     the threadpool where the threads should be unpaused
 * @return nothing
 */
void thpool_resume(struct threadpool *thpool_p);


/**
 * @brief Destroy the threadpool
 *
 * This will wait for the currently active threads to finish and then 'kill'
 * the whole threadpool to free up memory.
 *
 * @example
 * int main() {
 *    threadpool thpool1 = thpool_init(2);
 *    threadpool thpool2 = thpool_init(2);
 *    ..
 *    thpool_destroy(thpool1);
 *    ..
 *    return 0;
 * }
 *
 * @param thpool_p     the threadpool to destroy
 * @return nothing
 */
void thpool_destroy(struct threadpool *thpool_p);


/**
 * @brief Show number of currently active threads
 *
 * This is total total number of active thread in the pool, working
 * and idle.  Should always be the same.
 * Function created to ensure thread-safe access of value.
 *
 * @example
 * int main() {
 *    struct threadpool* thpool1 = thpool_init(2);
 *    struct threadpool* thpool2 = thpool_init(2);
 *    ..
 *    printf("Working threads: %d\n", thpool_num_threads_alive(thpool1));
 *    ..
 *    return 0;
 * }
 *
 * @param thpool_p     the threadpool of interest
 * @return integer       number of threads alive in pool
 */
int thpool_num_threads_alive(struct threadpool *thpool_p);


/**
 * @brief Show number of currently working threads
 *
 * Working threads are the threads that are performing work (not idle).
 *
 * @example
 * int main() {
 *    struct threadpool* thpool1 = thpool_init(2);
 *    struct threadpool* thpool2 = thpool_init(2);
 *    ..
 *    printf("Working threads: %d\n", thpool_num_threads_working(thpool1));
 *    ..
 *    return 0;
 * }
 *
 * @param thpool_p       the threadpool of interest
 * @return integer       number of threads working
 */
int thpool_num_threads_working(struct threadpool *thpool_p);


/**
 * @brief Show current number jobs in out queue.
 *
 * @example
 * int main() {
 *    threadpool thpool1 = thpool_init(2);
 *    threadpool thpool2 = thpool_init(2);
 *    ..
 *    printf("Queue out length: %d\n", thpool_queue_out_len(thpool1));
 *    ..
 *    return 0;
 * }
 *
 * @param thpool_p       the threadpool of interest
 * @return integer       number of completed jobs in queue_out
 */
int thpool_queue_out_len(struct threadpool *thpool_p);

/**
 * @brief Show current state of thpool "keepalive" flag.
 *
 * @example
 * int main() {
 *    threadpool thpool1 = thpool_init(2);
 *    threadpool thpool2 = thpool_init(2);
 *    ..
 *    printf("Queue out length: %d\n", thpool_alive_state(thpool1));
 *    ..
 *    return 0;
 * }
 *
 * @param thpool_p       the threadpool of interest
 * @return integer       thpool's "keepalive" flag state.
 *                       1 - keep all threads alive, 0 - kill all threads
 */
int thpool_alive_state(struct threadpool *thpool_p);

#ifdef __cplusplus
}
#endif

#endif
