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

static int connect_socket(int *sock_fd)
{
	int ret, s;
	struct sockaddr_un addr;

	*sock_fd = -1;
	s = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (s < 0)
		return -errno;

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_LOCAL;
	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/%s",
		 ILM_DEFAULT_RUN_DIR, ILM_SOCKET_NAME);

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
