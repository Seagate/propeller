/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 *
 * Derived from the sanlock file of the same name.
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>
#include <syslog.h>

extern int log_file_priority;
extern int log_file_use_utc;
extern int log_syslog_priority;
extern int log_stderr_priority;
extern int log_replay_count;

#ifndef TEST

/*
 * Log levels are used mainly to indicate where the message should be
 * recorded:
 *
 * ilm_log_dbg():  Write to stdout, not to file
 * ilm_log_warn(): Write to file /var/log/seagate_ilm.log
 * ilm_log_err():  Write to files /var/log/messages and /var/log/seagate_ilm.log
 */

extern void ilm_log(int level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

extern void ilm_log_array(int level, const char *array_name,
			  char *buf, int buf_len);

#else

static inline void ilm_log(int level, const char *fmt, ...)
{
	va_list ap;
	char log_str[512];

	va_start(ap, fmt);
	vsnprintf(log_str, 511, fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s\n", log_str);
        return;
}

static inline void ilm_log_array(int level, const char *array_name,
				 char *buf, int buf_len)
{
	int i;

	fprintf(stderr, "array: %s\n", array_name);

	i = 0;
	while (buf_len > 0) {
		fprintf(stderr, "%02x ", buf[i] & 0xff);
		i += 1;

		if (!(i % 16))
			fprintf(stderr, "\n");

		buf_len -= 1;
	}

	if (i % 16)
		fprintf(stderr, "\n");

	return;
}

#endif

#define ilm_log_dbg(fmt, args...)	ilm_log(LOG_DEBUG, fmt, ##args)
#define ilm_log_warn(fmt, args...)	ilm_log(LOG_WARNING, fmt, ##args)
#define ilm_log_err(fmt, args...)	ilm_log(LOG_ERR, fmt, ##args)

#define ilm_log_array_dbg(array_name, buf, buf_len)	\
	ilm_log_array(LOG_DEBUG, array_name, buf, buf_len)
#define ilm_log_array_warn(array_name, buf, buf_len)	\
	ilm_log_array(LOG_WARNING, array_name, buf, buf_len)
#define ilm_log_array_err(array_name, buf, buf_len)	\
	ilm_log_array(LOG_ERR, array_name, buf, buf_len)

extern int ilm_log_init(void);
extern void ilm_log_exit(void);

#endif
