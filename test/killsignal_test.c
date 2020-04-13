#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <unistd.h>

#include <ilm.h>

static int signal_received = 0;

static void sigterm_handler(int sig,
			    siginfo_t *info,
			    void *ctx)
{
	signal_received = 1;
}

static int signal_setup(void)
{
	struct sigaction act;
        int ret;

	memset(&act, 0, sizeof(act));
	act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = sigterm_handler;

        ret = sigaction(SIGTERM, &act, NULL);
	if (ret < 0) {
		fprintf(stderr, "Cannot set the signal handler\n");
		return -1;
	}

	return 0;
}

int main(void)
{
	int ret, s;
	struct idm_lock_id lock_id;
	struct idm_lock_op lock_op;

	ret = ilm_connect(&s);
	if (ret == 0) {
		printf("ilm_connect: SUCCESS\n");
	} else {
		printf("ilm_connect: FAIL\n");
		exit(-1);
	}

	ret = signal_setup();
	if (ret == 0) {
		printf("signal setup: SUCCESS\n");
	} else {
		printf("signal setup: FAIL\n");
		exit(-1);
	}

	ret = ilm_set_signal(s, SIGTERM);
	if (ret == 0) {
		printf("set_signal: SUCCESS\n");
	} else {
		printf("set_signal: FAIL\n");
		exit(-1);
	}

	uuid_generate(lock_id.vg_uuid);
	uuid_generate(lock_id.lv_uuid);

	lock_op.mode = IDM_MODE_EXCLUSIVE;
	lock_op.drive_num = 2;
	lock_op.drives[0] = "/dev/sda1";
	lock_op.drives[1] = "/dev/sda2";

	/* Set timeout to 3s */
	lock_op.timeout = 3000;

	ret = ilm_lock(s, &lock_id, &lock_op);
	if (ret == 0) {
		printf("ilm_lock: SUCCESS\n");
	} else {
		printf("ilm_lock: FAIL\n");
		exit(-1);
	}

	ret = ilm_inject_fault(s, 100);
	if (ret == 0) {
		printf("ilm_inject_fault (100): SUCCESS\n");
	} else {
		printf("ilm_inject_fault (100): FAIL\n");
		exit(-1);
	}

	while(!signal_received)
		sleep(1);

	ret = ilm_inject_fault(s, 0);
	if (ret == 0) {
		printf("ilm_inject_fault (0): SUCCESS\n");
	} else {
		printf("ilm_inject_fault (0): FAIL\n");
		exit(-1);
	}

	ret = ilm_unlock(s, &lock_id);
	if (ret == -ETIME) {
		printf("ilm_unlock: SUCCESS with timeout\n");
	} else {
		printf("ilm_unlock: FAIL %d\n", ret);
		exit(-1);
	}

	ret = ilm_disconnect(s);
	if (ret == 0) {
		printf("ilm_disconnect: SUCCESS\n");
	} else {
		printf("ilm_disconnect: FAIL\n");
		exit(-1);
	}

	return 0;
}
