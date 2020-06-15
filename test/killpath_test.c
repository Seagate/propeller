#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ilm.h>

#define EVENT_BUF_LEN     (16)

#define DRIVE1	"/dev/sda2"
#define DRIVE2	"/dev/sdb2"

static int wait_for_notify(void)
{
	char cwd[100];
	char dir[IDM_FAILURE_PATH_LEN];
	int fd, wd;
	int length;
	char buffer[EVENT_BUF_LEN];

	getcwd(cwd, sizeof(cwd));
	snprintf(dir, IDM_FAILURE_PATH_LEN, "%s/%s",
		 cwd, "tmp");
	mkdir(dir, 0755);

	/*creating the INOTIFY instance*/
	fd = inotify_init();

	/*checking for error*/
	if (fd < 0) {
	        printf("inotify_init failed\n");
		return -1;
	}

	wd = inotify_add_watch(fd, dir, IN_CREATE | IN_DELETE);

        read(fd, buffer, EVENT_BUF_LEN);

	inotify_rm_watch(fd, wd);

	/* closing the INOTIFY instance */
	close(fd);
	return 0;
}

int main(void)
{
	char cwd[100];
	char kill_path[IDM_FAILURE_PATH_LEN];
	char kill_args[IDM_FAILURE_ARGS_LEN];
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

	getcwd(cwd, sizeof(cwd));
	snprintf(kill_path, IDM_FAILURE_PATH_LEN, "%s/%s",
		 cwd, "killpath_notifier");
	snprintf(kill_args, IDM_FAILURE_ARGS_LEN, "%s/%s/%s",
		 cwd, "tmp", "aaa");

	remove(kill_args);
	printf("kill_path=%s\n", kill_path);
	printf("kill_args=%s\n", kill_args);
	ret = ilm_set_killpath(s, kill_path, kill_args);
	if (ret == 0) {
		printf("ilm_set_killpath: SUCCESS\n");
	} else {
		printf("ilm_set_killpath: FAIL\n");
		exit(-1);
	}

	uuid_generate(lock_id.vg_uuid);
	uuid_generate(lock_id.lv_uuid);

	lock_op.mode = IDM_MODE_EXCLUSIVE;
	lock_op.drive_num = 2;
	lock_op.drives[0] = DRIVE1;
	lock_op.drives[1] = DRIVE2;

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

	ret = wait_for_notify();
	if (ret == 0) {
		printf("wait_for_notify: Recieved notification\n");
	} else {
		printf("wait_for_notify: FAIL\n");
		exit(-1);
	}

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
