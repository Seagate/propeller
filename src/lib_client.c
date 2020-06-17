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
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include "client.h"
#include "cmd.h"
#include "ilm_internal.h"
#include "lock.h"

static int connect_socket(int *sock_fd)
{
	int ret, s;
	struct sockaddr_un addr;
	char *run_dir;

	*sock_fd = -1;
	s = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (s < 0)
		return -errno;

	run_dir = getenv("ILM_RUN_DIR");
	if (!run_dir)
		run_dir = ILM_DEFAULT_RUN_DIR;

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_LOCAL;
	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/%s",
		 run_dir, ILM_SOCKET_NAME);

	ret = connect(s, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (ret < 0) {
		ret = -errno;
		close(s);
		return ret;
	}

	*sock_fd = s;
	return 0;
}

static int send_header(int sock, int cmd, int data_len)
{
	struct ilm_msg_header header;
	int ret;

	memset(&header, 0, sizeof(header));
	header.magic = ILM_MSG_MAGIC;
	header.cmd = cmd;
	header.length = sizeof(header) + data_len;

retry:
	ret = send(sock, (void *)&header, sizeof(header), 0);
	if (ret == -1 && errno == EINTR)
		goto retry;

	if (ret < 0)
		return -errno;

	return 0;
}

static int send_data(int sock, const void *buf, size_t len, int flags)
{
	int ret;

retry:
	ret = send(sock, buf, len, flags);
	if (ret == -1 && errno == EINTR)
		goto retry;

	return ret;
}

static int recv_data(int sock, void *buf, size_t len, int flags)
{
	int ret;

retry:
	ret = recv(sock, buf, len, flags);
	if (ret == -1 && errno == EINTR)
		goto retry;

	return ret;
}

static int send_command(int cmd, int data_len)
{
	int ret, sock;

	ret = connect_socket(&sock);
	if (ret < 0)
		return ret;

	ret = send_header(sock, cmd, data_len);
	if (ret < 0) {
		close(sock);
		return ret;
	}

	return sock;
}

static int recv_result(int sock)
{
	struct ilm_msg_header hdr;
	int ret;

	memset(&hdr, 0, sizeof(hdr));

retry:
	ret = recv(sock, &hdr, sizeof(hdr), MSG_WAITALL);
	if (ret == -1 && errno == EINTR)
		goto retry;
	if (ret < 0)
		return -errno;
	if (ret != sizeof(hdr))
		return -1;

	return (int)hdr.result;
}

int ilm_connect(int *sock)
{
	int s, ret;

	ret = send_command(ILM_CMD_ADD_LOCKSPACE, 0);
	if (ret < 0)
		return ret;

	s = ret;
	ret = recv_result(s);
	if (ret < 0)
		goto out;

	*sock = s;
	return 0;

out:
	close(s);
	return ret;
}

int ilm_disconnect(int sock)
{
	int s, ret;

	ret = send_header(sock, ILM_CMD_DEL_LOCKSPACE, 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	close(sock);
	return 0;
}

int ilm_version(int sock, char *drive, int *version)
{
	struct ilm_lock_payload payload;
	char path[PATH_MAX];
	int i, len, ret;

	len = sizeof(struct ilm_lock_payload) + PATH_MAX;

	ret = send_header(sock, ILM_CMD_VERSION, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	payload.drive_num = 1;

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	strncpy(path, drive, PATH_MAX);
	ret = send_data(sock, path, PATH_MAX, 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	ret = recv_data(sock, (char *)version, sizeof(int), 0);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_lock(int sock, struct idm_lock_id *id, struct idm_lock_op *op)
{
	struct ilm_lock_payload payload;
	int i, len, ret;
	char path[PATH_MAX];

	/* Return error when drive number is zero */
	if (!op || !op->drive_num)
		return -EINVAL;

	/* Return error when detect drive path is NULL */
	for (i = 0; i < op->drive_num; i++) {
		if (!op->drives[i])
			return -EINVAL;
	}

	len = sizeof(struct ilm_lock_payload) + op->drive_num * PATH_MAX;

	ret = send_header(sock, ILM_CMD_ACQUIRE, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	payload.mode = op->mode;
	payload.drive_num = op->drive_num;
	payload.timeout = op->timeout;

	memcpy(payload.lock_id, id, sizeof(*id));

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	for (i = 0; i < op->drive_num; i++) {
		strncpy(path, op->drives[i], PATH_MAX);
		ret = send_data(sock, path, PATH_MAX, 0);
		if (ret < 0)
			return ret;
	}

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_unlock(int sock, struct idm_lock_id *id)
{
	struct ilm_lock_payload payload;
	int i, len, ret;

	len = sizeof(struct ilm_lock_payload);

	ret = send_header(sock, ILM_CMD_RELEASE, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	memcpy(payload.lock_id, id, sizeof(*id));

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_convert(int sock, struct idm_lock_id *id, uint32_t mode)
{
	struct ilm_lock_payload payload;
	int i, len, ret;

	len = sizeof(struct ilm_lock_payload);

	ret = send_header(sock, ILM_CMD_CONVERT, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	payload.mode = mode;
	memcpy(payload.lock_id, id, sizeof(*id));

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_write_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len)
{
	struct ilm_lock_payload payload;
	int i, len, ret;

	len = sizeof(struct ilm_lock_payload);
	len += lvb_len;

	ret = send_header(sock, ILM_CMD_WRITE_LVB, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	memcpy(payload.lock_id, id, sizeof(*id));

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	ret = send_data(sock, lvb, lvb_len, 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_read_lvb(int sock, struct idm_lock_id *id, char *lvb, int lvb_len)
{
	struct ilm_lock_payload payload;
	int i, len, ret;

	len = sizeof(struct ilm_lock_payload);

	ret = send_header(sock, ILM_CMD_READ_LVB, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	memcpy(payload.lock_id, id, sizeof(*id));

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	ret = recv_data(sock, lvb, lvb_len, 0);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_set_signal(int sock, int signo)
{
	int ret;

	ret = send_header(sock, ILM_CMD_SET_SIGNAL, sizeof(int));
	if (ret < 0)
		return ret;

	ret = send_data(sock, &signo, sizeof(int), 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_set_killpath(int sock, char *killpath, char *killargs)
{
	char path[IDM_FAILURE_PATH_LEN];
	char args[IDM_FAILURE_ARGS_LEN];

	int len, ret;

	len = sizeof(struct ilm_lock_payload) +
	      IDM_FAILURE_PATH_LEN + IDM_FAILURE_ARGS_LEN;

	ret = send_header(sock, ILM_CMD_SET_KILLPATH, len);
	if (ret < 0)
		return ret;

	strncpy(path, killpath, IDM_FAILURE_PATH_LEN);
	ret = send_data(sock, path, IDM_FAILURE_PATH_LEN, 0);
	if (ret < 0)
		return ret;

	strncpy(args, killargs, IDM_FAILURE_ARGS_LEN);
	ret = send_data(sock, args, IDM_FAILURE_ARGS_LEN, 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_get_host_count(int sock, struct idm_lock_id *id,
		       struct idm_lock_op *op, int *count, int *self)
{
	struct ilm_lock_payload payload;
	int i, len, ret;
	char path[PATH_MAX];
	struct _host_account {
		int count;
		int self;
	} account;

	len = sizeof(struct ilm_lock_payload) + op->drive_num * PATH_MAX;

	ret = send_header(sock, ILM_CMD_LOCK_HOST_COUNT, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	payload.drive_num = op->drive_num;

	memcpy(payload.lock_id, id, sizeof(*id));

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	for (i = 0; i < op->drive_num; i++) {
		strncpy(path, op->drives[i], PATH_MAX);
		ret = send_data(sock, path, PATH_MAX, 0);
		if (ret < 0)
			return ret;
	}

	ret = recv_result(sock);
	if (ret == -ENOENT)
		*count = 0;

	if (ret < 0)
		return ret;

	ret = recv_data(sock, (char *)&account, sizeof(account), 0);
	if (ret < 0)
		return ret;

	*count = account.count;
	*self = account.self;
	return 0;
}

int ilm_get_mode(int sock, struct idm_lock_id *id,
		 struct idm_lock_op *op, int *mode)
{
	struct ilm_lock_payload payload;
	char path[PATH_MAX];
	int i, len, ret;

	len = sizeof(struct ilm_lock_payload) + op->drive_num * PATH_MAX;

	ret = send_header(sock, ILM_CMD_LOCK_MODE, len);
	if (ret < 0)
		return ret;

	memset(&payload, 0, sizeof(struct ilm_lock_payload));
	payload.magic = ILM_LOCK_MAGIC;
	payload.drive_num = op->drive_num;

	memcpy(payload.lock_id, id, sizeof(*id));

	ret = send_data(sock, &payload, sizeof(struct ilm_lock_payload), 0);
	if (ret < 0)
		return ret;

	for (i = 0; i < op->drive_num; i++) {
		strncpy(path, op->drives[i], PATH_MAX);
		ret = send_data(sock, path, PATH_MAX, 0);
		if (ret < 0)
			return ret;
	}

	ret = recv_result(sock);
	if (ret == -ENOENT)
		*mode = 0;

	if (ret < 0)
		return ret;

	ret = recv_data(sock, (char *)mode, sizeof(int), 0);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_set_host_id(int sock, char *id, int id_len)
{
	int ret;

	ret = send_header(sock, ILM_CMD_SET_HOST_ID, id_len);
	if (ret < 0)
		return ret;

	ret = send_data(sock, id, id_len, 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_stop_renew(int sock)
{
	int ret;

	ret = send_header(sock, ILM_CMD_STOP_RENEW, 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_start_renew(int sock)
{
	int ret;

	ret = send_header(sock, ILM_CMD_START_RENEW, 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}

int ilm_inject_fault(int sock, int percentage)
{
	int ret;

	ret = send_header(sock, ILM_CMD_INJECT_FAULT, sizeof(int));
	if (ret < 0)
		return ret;

	ret = send_data(sock, &percentage, sizeof(int), 0);
	if (ret < 0)
		return ret;

	ret = recv_result(sock);
	if (ret < 0)
		return ret;

	return 0;
}
