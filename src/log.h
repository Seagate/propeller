/*
 * Copyright (C) 2020-2021 Seagate
 * Copyright (C) 2020-2021 Linaro Ltd
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <syslog.h>

int log_file_priority;
int log_file_use_utc;
int log_syslog_priority;
int log_stderr_priority;

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

#define ilm_log_dbg(fmt, args...)	ilm_log(LOG_DEBUG, fmt, ##args)
#define ilm_log_warn(fmt, args...)	ilm_log(LOG_WARNING, fmt, ##args)
#define ilm_log_err(fmt, args...)	ilm_log(LOG_ERR, fmt, ##args)

extern int ilm_log_init(void);
extern void ilm_log_exit(void);
extern void ilm_log(int level, const char *fmt, ...);

#endif
