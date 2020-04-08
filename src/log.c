/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdarg.h>

#include "ilm_internal.h"
#include "log.h"

#define ILM_DAEMON_NAME		"seagate_ilm"

#define ILM_LOG_FILE		"seagate_ilm.log"

#define ILM_LOG_STR_LEN		(512)
#define ILM_LOG_DUMP_SIZE	(1024 * 1024)

#define ILM_LOG_ENTRIES		4096

static char log_str[ILM_LOG_STR_LEN];

static pthread_t log_thd;

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t log_cond = PTHREAD_COND_INITIALIZER;

struct log_entry {
	int level;
	char str[ILM_LOG_STR_LEN];
};

static struct log_entry *log_records;
static unsigned int log_head; /* add at head */
static unsigned int log_tail; /* remove from tail */
static unsigned int log_dropped;
static unsigned int log_pending_ents;
static unsigned int log_thread_done;

static FILE *log_file_fp;

int log_file_priority = LOG_WARNING;
int log_file_use_utc;
int log_syslog_priority = LOG_WARNING;
int log_stderr_priority = LOG_DEBUG;

static void log_save_record(int level, char *str, int len)
{
	struct log_entry *e;

	if (!log_records)
		return;

	if (log_pending_ents == ILM_LOG_ENTRIES) {
		log_dropped++;
		return;
	}

	e = &log_records[log_head++];
	log_head = log_head % ILM_LOG_ENTRIES;
	log_pending_ents++;

	e->level = level;
	memcpy(e->str, str, len);
}

/**
 * The main log function, the log level is descent ordered
 * so less value with higher priority.
 *
 * It copies log_str into the log_records circular array to be
 * written to logfile and/or syslog (so callers don't block
 * writing messages to files).
 */
void ilm_log(int level, const char *fmt, ...)
{
	va_list ap;
	int ret, pos = 0;
	int len = ILM_LOG_STR_LEN - 2; /* leave room for \n\0 */
	struct timeval cur_time;
	struct tm time_info;
	pid_t tid;
	struct timespec ts;

	pthread_mutex_lock(&log_mutex);

	gettimeofday(&cur_time, NULL);

	if (log_file_use_utc)
		gmtime_r(&cur_time.tv_sec, &time_info);
	else
		localtime_r(&cur_time.tv_sec, &time_info);

	ret = strftime(log_str + pos, len - pos,
		       "%Y-%m-%d %H:%M:%S ", &time_info);
	pos += ret;

	tid = syscall(SYS_gettid);
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ret = snprintf(log_str + pos, len - pos, "%lu [%u]: ",
		       ts.tv_sec, tid);
	pos += ret;

	va_start(ap, fmt);
	ret = vsnprintf(log_str + pos, len - pos, fmt, ap);
	va_end(ap);

	if (ret >= len - pos)
		pos = len - 1;
	else
		pos += ret;

	log_str[pos++] = '\n';
	log_str[pos++] = '\0';

	/*
	 * Save messages in circular array "log_records" that a thread
	 * writes to logfile/syslog
	 */
	if (level <= log_file_priority || level <= log_syslog_priority)
		log_save_record(level, log_str, pos);

	if (level <= log_stderr_priority)
		fprintf(stderr, "%s", log_str);

	pthread_cond_signal(&log_cond);
	pthread_mutex_unlock(&log_mutex);
}

void ilm_log_array(int level, const char *array_name, char *buf, int buf_len)
{
	int i, tail, tail_len;

	ilm_log(level, "array: %s", array_name);

	tail_len = buf_len % 4;

	tail = 0;
	for (i = 0; i < tail_len; i++)
		tail |= (buf[buf_len - tail_len + i] << (i * 8));

	i = 0;
	while (buf_len >= 16) {
		ilm_log(level, "%08x %08x %08x %08x",
			*(int *)(buf + i), *(int *)(buf + i + 4),
			*(int *)(buf + i + 8), *(int *)(buf + i + 12));
		i += 16;
		buf_len -= 16;
	}

	if (buf_len > 12) {
		ilm_log(level, "%08x %08x %08x %08x",
			*(int *)(buf + i), *(int *)(buf + i + 4),
			*(int *)(buf + i + 8), tail);
	} else if (buf_len > 8) {
		if (buf_len == 12)
			ilm_log(level, "%08x %08x %08x",
				*(int *)(buf + i), *(int *)(buf + i + 4),
				*(int *)(buf + i + 8));
		else
			ilm_log(level, "%08x %08x %08x",
				*(int *)(buf + i), *(int *)(buf + i + 4), tail);
	} else if (buf_len > 4) {
		if (buf_len == 8)
			ilm_log(level, "%08x %08x",
				*(int *)(buf + i), *(int *)(buf + i + 4));
		else
			ilm_log(level, "%08x %08x", *(int *)(buf + i), tail);
	} else if (buf_len > 0) {
		if (buf_len == 4)
			ilm_log(level, "%08x", *(int *)(buf + i));
		else
			ilm_log(level, "%08x", tail);
	}
}

static void log_write_record(int level, char *str)
{
	if (level <= log_file_priority && log_file_fp) {
		fprintf(log_file_fp, "%s", str);
		fflush(log_file_fp);
	}

	if (level <= log_syslog_priority)
		syslog(level, "%s", str);
}

static void log_write_dropped(int level, int num)
{
	char str[ILM_LOG_STR_LEN];

	sprintf(str, "dropped %d entries", num);
	log_write_record(level, str);
}

static void *log_thd_fn(void *arg __maybe_unused)
{
	char str[ILM_LOG_STR_LEN];
	struct log_entry *e;
	int level, prev_dropped;

	while (1) {
		pthread_mutex_lock(&log_mutex);
		while (log_head == log_tail) {
			if (log_thread_done) {
				pthread_mutex_unlock(&log_mutex);
				goto out;
			}
			pthread_cond_wait(&log_cond, &log_mutex);
		}

		e = &log_records[log_tail++];
		log_tail = log_tail % ILM_LOG_ENTRIES;
		log_pending_ents--;

		memcpy(str, e->str, ILM_LOG_STR_LEN);
		level = e->level;

		prev_dropped = log_dropped;
		log_dropped = 0;
		pthread_mutex_unlock(&log_mutex);

		if (prev_dropped)
			log_write_dropped(level, prev_dropped);

		log_write_record(level, str);
	}

out:
	pthread_exit(NULL);
}

int ilm_log_init(void)
{
	int fd, rv;
	char logfile_path[PATH_MAX];

	snprintf(logfile_path, PATH_MAX, "%s/%s", env.run_dir,
		 ILM_LOG_FILE);

	log_file_fp = fopen(logfile_path, "a+");
	if (!log_file_fp)
		return -1;

	fd = fileno(log_file_fp);
	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);

	log_records = malloc(ILM_LOG_ENTRIES * sizeof(struct log_entry));
	if (!log_records) {
		fclose(log_file_fp);
		log_file_fp = NULL;
		return -1;
	}
	memset(log_records, 0, ILM_LOG_ENTRIES * sizeof(struct log_entry));

	openlog(ILM_DAEMON_NAME, LOG_CONS | LOG_PID, LOG_DAEMON);

	rv = pthread_create(&log_thd, NULL, log_thd_fn, NULL);
	if (rv)
		return -1;

	return 0;
}

void ilm_log_exit(void)
{
	/* Notify log thread to exit */
	pthread_mutex_lock(&log_mutex);
	log_thread_done = 1;
	pthread_cond_signal(&log_cond);
	pthread_mutex_unlock(&log_mutex);

	/* Wait for log thread to exit */
	pthread_join(log_thd, NULL);

	pthread_mutex_lock(&log_mutex);

	/* Close syslog */
	closelog();

	/* Close log file */
	if (log_file_fp) {
		fclose(log_file_fp);
		log_file_fp = NULL;
	}

	pthread_mutex_unlock(&log_mutex);
}
