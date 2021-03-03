/*
 * Copyright (C) 2020-2021 Seagate
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
#include <poll.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <grp.h>

#include "failure.h"
#include "log.h"
#include "ilm_internal.h"

#define MAX_AV_COUNT 8

static __maybe_unused void ilm_run_path(char *kill_path, char *kill_args)
{
	char arg[IDM_FAILURE_ARGS_LEN];
	char *args = kill_args;
	char *argv[MAX_AV_COUNT + 1]; /* +1 for NULL */
	int argv_count = 0;
	int i, arg_len, args_len;

	for (i = 0; i < MAX_AV_COUNT + 1; i++)
		argv[i] = NULL;

	argv[argv_count++] = strdup(kill_path);

	if (!args[0])
		goto exec_prog;

	/* this should already be done, but make sure */
	args[IDM_FAILURE_ARGS_LEN - 1] = '\0';

	memset(&arg, 0, sizeof(arg));
	arg_len = 0;
	args_len = strlen(args);

	for (i = 0; i < args_len; i++) {
		if (!args[i])
			break;

		if (argv_count == MAX_AV_COUNT)
			break;

		if (args[i] == '\\') {
			if (i == (args_len - 1))
				break;
			i++;

			if (args[i] == '\\') {
				arg[arg_len++] = args[i];
				continue;
			}
			if (isspace(args[i])) {
				arg[arg_len++] = args[i];
				continue;
			} else {
				break;
			}
		}

		if (isalnum(args[i]) || ispunct(args[i])) {
			arg[arg_len++] = args[i];
		} else if (isspace(args[i])) {
			if (arg_len)
				argv[argv_count++] = strdup(arg);

			memset(arg, 0, sizeof(arg));
			arg_len = 0;
		} else {
			break;
		}
	}

	if ((argv_count < MAX_AV_COUNT) && arg_len) {
		argv[argv_count++] = strdup(arg);
	}

exec_prog:
	execvp(argv[0], argv);
}

int ilm_failure_handler(struct ilm_lockspace *ls)
{
	int ret;
	char cmd[512];

	if (ls->kill_path) {
		ret = snprintf(cmd, sizeof(cmd), "%s %s", ls->kill_path,
			       ls->kill_args);
		if (ret < 0 || ret == sizeof(cmd)) {
			ilm_log_err("%s: Fail to generate kill path cmd %d",
				    __func__, ret);
			return -1;
		}

		ilm_log_warn("%s: kill command=%s", __func__, cmd);

		if (!(popen(cmd, "r"))) {
			ilm_log_err("%s: Fail to execute kill path cmd %s",
				    __func__, cmd);
			return -1;
		}
	} else if (ls->kill_sig) {
		ilm_log_warn("%s: kill_pid=%d kill_sig=%d",
			     __func__, ls->kill_pid, ls->kill_sig);
		kill(ls->kill_pid, ls->kill_sig);
	}

	return 0;
}
