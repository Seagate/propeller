/*
 * Copyright (C) 2020-2021 Seagate
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <unistd.h>

#include "cmd.h"
#include "client.h"
#include "drive.h"
#include "ilm_internal.h"
#include "log.h"

#define ILM_MAIN_LOOP_INTERVAL		1000 /* milliseconds */

int ilm_shutdown = 0;
uuid_t ilm_uuid;

struct ilm_env env;

static int ilm_read_args(int argc, char *argv[])
{
	char opt;
	char *arg, *p;
	mode_t old_umask;
	int i, ret;

	/* Skip the command name, start from argument 1 */
	for (i = 1; i < argc;) {
		p = argv[i];
		if ((p[0] != '-') || (strlen(p) != 2)) {
			fprintf(stderr, "Unknown option %s\n", p);
			exit(EXIT_FAILURE);
		}

		opt = p[1];
		if ((i + 1) >= argc) {
			fprintf(stderr, "Option '%c' requires arg", opt);
			exit(EXIT_FAILURE);
		}

		i++;
		arg = argv[i];

		switch (opt) {
		case 'D':
			env.debug = atoi(arg);
			break;
		case 'L':
			log_file_priority = atoi(arg);
			break;
		case 'U':
			log_file_use_utc = atoi(arg);
			break;
		case 'S':
			log_syslog_priority = atoi(arg);
			break;
		case 'E':
			log_stderr_priority = atoi(arg);
			break;
		case 'l':
			env.mlock = atoi(arg);
			break;
		default:
			fprintf(stderr, "Unknown Option '%c'", opt);
			exit(EXIT_FAILURE);
		}

		i++;
	}

	env.run_dir = getenv("ILM_RUN_DIR");
	if (!env.run_dir)
		env.run_dir = ILM_DEFAULT_RUN_DIR;

	env.log_dir = getenv("ILM_LOG_DIR");
	if (!env.log_dir)
		env.log_dir = ILM_DEFAULT_LOG_DIR;

	old_umask = umask(0002);

	/* Create run directory */
	ret = mkdir(env.run_dir, 0775);
	if (ret < 0 && errno != EEXIST) {
		umask(old_umask);
		return ret;
	}
	umask(old_umask);

	return 0;
}

static int ilm_daemon_setup(void)
{
#if 0
	struct sched_param sched_param;
	int max_prio;
#endif
	int ret;

	if (!env.debug && daemon(0, 0) < 0) {
		fprintf(stderr, "Cannot set process as daemon\n");
		return -1;
	}

	/* Lock process's virtual address space into RAM */
	if (env.mlock) {
		ret = mlockall(MCL_CURRENT | MCL_FUTURE);
		if (ret < 0) {
			fprintf(stderr, "mlockall failed: %s", strerror(errno));
			return -1;
		}
	}

	/* Disable below code due it fails to build on Ubuntu */
#if 0
	max_prio = sched_get_priority_max(SCHED_RR);
	if (max_prio < 0) {
		ilm_log_err("Could not get max sched priority: %s",
			    strerror(errno));
		return -1;
	}

	/* Set the main process to SCHED_RR with highest prio */
	sched_param.sched_priority = max_prio;
	ret = sched_setscheduler(0, SCHED_RR | SCHED_RESET_ON_FORK,
				 &sched_param);
	if (ret < 0) {
		ilm_log_err("Set sched priority %d (RR) failed: %s",
			    sched_param.sched_priority,
			    strerror(errno));
		return -1;
	}
#endif

	return 0;
}

static void ilm_sigterm_handler(int sig __maybe_unused,
				siginfo_t *info __maybe_unused,
				void *ctx __maybe_unused)
{
	ilm_shutdown = 1;
}

static int ilm_signal_setup(void)
{
	struct sigaction act;
	int rv, i, sig_list[] = { SIGHUP, SIGINT, SIGTERM, 0 };

	memset(&act, 0, sizeof(act));

	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = ilm_sigterm_handler;

	for (i = 0; sig_list[i] != 0; i++) {
		rv = sigaction(sig_list[i], &act, NULL);
		if (rv < 0) {
			ilm_log_err("Cannot set the signal handler for: %i",
				    sig_list[i]);
			return -1;
		}
	}

	return 0;
}

static int ilm_main_loop(void)
{
	struct pollfd *pollfd = NULL;
	int num;
	int ret;

	while (1) {
		if (ilm_client_is_updated()) {
			if (pollfd)
				free(pollfd);

			ilm_client_alloc_pollfd(&pollfd, &num);
		}

		ret = poll(pollfd, num, ILM_MAIN_LOOP_INTERVAL);
		if (ret == -1 && errno == EINTR)
			continue;

		ilm_client_handle_request(pollfd, num);

		if (ilm_shutdown)
			break;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	ret = ilm_read_args(argc, argv);
	if (ret < 0)
		return EXIT_FAILURE;

	ret = ilm_daemon_setup();
	if (ret < 0)
		return EXIT_FAILURE;

	ret = ilm_log_init();
	if (ret < 0)
		return EXIT_FAILURE;

	ret = ilm_signal_setup();
	if (ret < 0)
		goto signal_setup_fail;

	ret = ilm_scsi_list_init();
	if (ret < 0)
		goto signal_setup_fail;

	ret = ilm_client_listener_init();
	if (ret < 0)
		goto client_fail;

	ret = ilm_cmd_queue_create();
	if (ret < 0)
		goto queue_fail;

	uuid_generate(ilm_uuid);

	ilm_main_loop();

	ilm_cmd_queue_free();

queue_fail:
	ilm_client_listener_exit();
client_fail:
	ilm_scsi_list_exit();
signal_setup_fail:
	ilm_log_exit();
	return 0;
}
