/*
 * Copyright (C) 2020-2021 Seagate
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __ILM_INTERNAL_H__
#define __ILM_INTERNAL_H__

#define ILM_DEFAULT_RUN_DIR	"/run/seagate_ilm"
#define ILM_SOCKET_NAME		"main.sock"
#define ILM_LOCKFILE_NAME	"main.pid"

#define ILM_DEFAULT_LOG_DIR	"/var/log"

#define __maybe_unused		__attribute__((__unused__))

struct ilm_env {
	int debug;
	int mlock;
	const char *run_dir;
	const char *log_dir;
};

extern struct ilm_env env;

#endif /* __ILM_INTERNAL_H__ */
