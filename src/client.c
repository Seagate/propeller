/*
 * Copyright (C) 2020-2021 Seagate
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "client.h"
#include "cmd.h"
#include "ilm_internal.h"
#include "list.h"
#include "log.h"
#include "lockspace.h"

#define CLIENT_STATE_RUN	1
#define CLIENT_STATE_SUSPEND	2
#define CLIENT_STATE_EXIT	3

static struct list_head client_list = LIST_HEAD_INIT(client_list);
static pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static int client_efd;
static int client_updated = 0;
static int client_num = 0;
static int sock_fd;
static int lock_fd;

static int _client_is_valid(struct client *cl)
{
        struct client *pos;
	int ret = 0;

	pthread_mutex_lock(&client_list_mutex);

	list_for_each_entry(pos, &client_list, list) {

		if (pos == cl) {
			ret = 1;
			break;
		}
	}

	pthread_mutex_unlock(&client_list_mutex);
	return ret;
}

static struct client *_fd_to_client(int fd)
{
        struct client *cl;
	int found = 0;

	pthread_mutex_lock(&client_list_mutex);

	list_for_each_entry(cl, &client_list, list) {

		if (cl->fd == fd) {
			found = 1;
			break;
		}
	}

	pthread_mutex_unlock(&client_list_mutex);

	if (found)
		return cl;

	return NULL;
}

static int ilm_get_peer_pid(int fd)
{
	struct ucred cred;
	unsigned int len = sizeof(struct ucred);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
		return -1;

	return cred.pid;
}

int ilm_client_is_updated(void)
{
	int updated;

	pthread_mutex_lock(&client_list_mutex);

	updated = client_updated;

	if (client_updated)
		client_updated = 0;

	pthread_mutex_unlock(&client_list_mutex);
	return updated;
}

int ilm_client_alloc_pollfd(struct pollfd **poll_fd, int *num)
{
	int i, ret = 0;
        struct client *cl;

	pthread_mutex_lock(&client_list_mutex);

	*poll_fd = malloc(sizeof(struct pollfd) * (client_num + 1));
	if (!*poll_fd) {
		ilm_log_err("Cannot allcoate pollfd array\n");
		ret = -1;
		goto out;
	}

	i = 0;
	list_for_each_entry(cl, &client_list, list) {
		pthread_mutex_lock(&cl->mutex);
		if (cl->state == CLIENT_STATE_RUN) {
			(*poll_fd)[i].fd = cl->fd;
			(*poll_fd)[i].events = POLLIN;
			i++;
		}
		pthread_mutex_unlock(&cl->mutex);
	}

	(*poll_fd)[i].fd = client_efd;
	(*poll_fd)[i].events = POLLIN;
	i++;

	*num = i;

out:
	pthread_mutex_unlock(&client_list_mutex);
	return ret;
}

int ilm_client_handle_request(struct pollfd *poll_fd, int num)
{
	struct client *cl;
	int i;
	uint64_t event_data;

	for (i = 0; i < num; i++) {

		if (poll_fd[i].fd == client_efd &&
		    poll_fd[i].revents & POLLIN) {
			eventfd_read(client_efd, &event_data);
			continue;
		}

		cl = _fd_to_client(poll_fd[i].fd);

		/*
		 * Failed to find client, handle next one.
		 */
		if (!cl)
			continue;

		if (poll_fd[i].revents & POLLIN) {
			if (cl->workfn)
				cl->workfn(cl);
		}

		if (poll_fd[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			if (cl->deadfn)
				cl->deadfn(cl);
		}
	}

	return 0;
}

int ilm_client_suspend(struct client *cl)
{
        int ret = 0;

	if (!_client_is_valid(cl)) {
		ilm_log_err("Cannot suspend client which is invalid\n");
		return -1;
	}

        pthread_mutex_lock(&cl->mutex);

	if (cl->state != CLIENT_STATE_RUN) {
		ilm_log_err("Cannot suspend client with state: %d\n",
			    cl->state);
		ret = -1;
	} else {
		cl->state = CLIENT_STATE_SUSPEND;
	}

        pthread_mutex_unlock(&cl->mutex);

	pthread_mutex_lock(&client_list_mutex);
	client_updated = 1;
	pthread_mutex_unlock(&client_list_mutex);

	return ret;
}

int ilm_client_resume(struct client *cl)
{
        int ret = 0;

	if (!_client_is_valid(cl)) {
		ilm_log_err("Cannot resume client which is invalid\n");
		return -1;
	}

        pthread_mutex_lock(&cl->mutex);

	if (cl->state != CLIENT_STATE_SUSPEND) {
		ilm_log_err("Cannot resume client with state: %d\n",
			    cl->state);
		ret = -1;
	} else {
		cl->state = CLIENT_STATE_RUN;
	}

        pthread_mutex_unlock(&cl->mutex);

	pthread_mutex_lock(&client_list_mutex);
	client_updated = 1;
	pthread_mutex_unlock(&client_list_mutex);

	/* Notify main poll that the client resumes back */
	eventfd_write(client_efd, 1);

	return ret;
}

static int ilm_client_del(struct client *cl)
{
	if (!_client_is_valid(cl)) {
		ilm_log_err("Cannot delete client which is invalid\n");
		return -1;
	}

	/* Change client state from RUN to EXIT */
	pthread_mutex_lock(&cl->mutex);

	if (cl->state != CLIENT_STATE_RUN) {
		ilm_log_err("Cannot delete client with state: %d\n",
			    cl->state);
		pthread_mutex_unlock(&cl->mutex);
		return -1;
	}
	cl->state = CLIENT_STATE_EXIT;

	pthread_mutex_unlock(&cl->mutex);

	/* Remove client from list */
	pthread_mutex_lock(&client_list_mutex);
	list_del(&cl->list);
	client_updated = 1;
	client_num--;
	pthread_mutex_unlock(&client_list_mutex);

	ilm_lockspace_terminate(cl->ls);

	/* Cleanup client */
	if (cl->fd != -1)
		close(cl->fd);
	free(cl);

	return 0;
}

static int ilm_client_add(int fd,
			  int (*workfn)(struct client *),
			  int (*deadfn)(struct client *))
{
        struct client *cl;

	cl = malloc(sizeof(struct client));
	if (!cl) {
		ilm_log_err("Failed to allocate client\n");
		return -1;
	}

	cl->state = CLIENT_STATE_RUN;
	cl->fd = fd;
	cl->pid = ilm_get_peer_pid(fd);
	cl->workfn = workfn;
	cl->deadfn = deadfn ? deadfn : ilm_client_del;
	pthread_mutex_init(&cl->mutex, NULL);

	/* Add client into list */
	pthread_mutex_lock(&client_list_mutex);
	list_add(&cl->list, &client_list);
	client_updated = 1;
	client_num++;
	pthread_mutex_unlock(&client_list_mutex);

	return 0;
}

void ilm_send_result(int fd, int result, char *data, int data_len)
{
	struct ilm_msg_header h;

	h.magic  = ILM_MSG_MAGIC;
	h.length = sizeof(h) + data_len;
	h.result = result;
	send(fd, &h, sizeof(h), MSG_NOSIGNAL);

	if (data_len)
		send(fd, data, data_len, MSG_NOSIGNAL);
}

static int ilm_client_request(struct client *cl)
{
	struct ilm_msg_header hdr;
	struct ilm_cmd *cmd;
	int ret;

	memset(&hdr, 0, sizeof(struct ilm_msg_header));

	ret = recv(cl->fd, &hdr, sizeof(hdr), MSG_WAITALL);
	if (!ret)
		return 0;

	if (ret != sizeof(hdr)) {
		ilm_log_err("client fd %d recv errno %d",
			    cl->fd, errno);
		goto dead;
	}

	if (hdr.magic != ILM_MSG_MAGIC) {
	        ilm_log_err("client fd %d ret %d magic %x vs %x",
			    cl->fd, ret, hdr.magic, ILM_MSG_MAGIC);
		goto dead;
	}

	cmd = malloc(sizeof(struct ilm_cmd));
	if (!cmd) {
	        ilm_log_err("Fail to allocate struct ilm_cmd\n");
		goto dead;
	}

	cmd->cmd = hdr.cmd;
	cmd->cl = cl;
	cmd->sock_msg_len = hdr.length;

	ret = ilm_client_suspend(cl);
	if (ret < 0) {
		free(cmd);
		goto dead;
	}

	ret = ilm_cmd_queue_add_work(cmd);
	if (ret < 0) {
		free(cmd);
		goto dead;
	}

	return 0;

dead:
	if (cl->deadfn)
		cl->deadfn(cl);
	return -1;
}

void ilm_client_recv_all(struct client *cl, int msg_len, int pos)
{
        char trash[64];
        int left = msg_len - sizeof(struct ilm_msg_header) - pos;
        int ret = 0, retries = 0, trash_len;

	ilm_log_dbg("%s: msg_len %d pos %d left %d", __func__,
		    msg_len, pos, left);

        while (left > 0) {
		/* The read length should always less or equal than left */
		trash_len = sizeof(trash);
		if (trash_len > left)
			trash_len = left;

                ret = recv(cl->fd, trash, trash_len, MSG_DONTWAIT);

		/* Read successfully */
		if (ret > 0)
			left -= ret;

		/* Failed, but can try again */
                if (ret == -1 && errno == EAGAIN) {
                        usleep(1000);
                        if (retries < 20) {
                                retries++;
                                continue;
			}
		}

		/* Failed and cannot fixup, bail out */
		if (ret <= 0)
			break;
	}

        ilm_log_dbg("%s: fd %d pid %d pos %d ret %d errno %d retries %d left %d",
		    __func__, cl->fd, cl->pid, pos, ret, errno, retries, left);
}

static int ilm_client_connect(struct client *cl)
{
	int fd, on = 1;
	int ret;

	fd = accept(cl->fd, NULL, NULL);
	if (fd < 0)
		return -1;

	setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

	ret = ilm_client_add(fd, ilm_client_request, NULL);
	if (ret < 0)
		return -1;

	return 0;
}

static int ilm_lock_file(void)
{
	char path[PATH_MAX];
	char buf[16];
	struct flock lock;
	int fd, ret;

	snprintf(path, PATH_MAX, "%s/%s", env.run_dir, ILM_LOCKFILE_NAME);

	fd = open(path, O_CREAT|O_WRONLY|O_CLOEXEC, 0644);
	if (fd < 0) {
		ilm_log_err("lockfile open error %s: %s", path,
			    strerror(errno));
		return -1;
	}

	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	ret = fcntl(fd, F_SETLK, &lock);
	if (ret < 0) {
		ilm_log_err("lockfile setlk error %s: %s", path,
			    strerror(errno));
		goto fail;
	}

	ret = ftruncate(fd, 0);
	if (ret < 0) {
		ilm_log_err("lockfile truncate error %s: %s", path,
			    strerror(errno));
		goto fail;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%d\n", getpid());

	ret = write(fd, buf, strlen(buf));
	if (ret <= 0) {
		ilm_log_err("lockfile write error %s: %s",
			    path, strerror(errno));
		goto fail;
	}

	return fd;
fail:
	close(fd);
	return -1;
}

int ilm_client_listener_init(void)
{
	struct sockaddr_un addr;
	int ret;

	lock_fd = ilm_lock_file();
	if (lock_fd < 0) {
		ilm_log_err("Cannot create lock file\n");
		return -1;
	}

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_LOCAL;
	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/%s",
		 env.run_dir, ILM_SOCKET_NAME);

	sock_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		ilm_log_err("Cannot create socket\n");
		goto sock_fail;
	}

	unlink(addr.sun_path);
	ret = bind(sock_fd, (struct sockaddr *)&addr,
		   sizeof(struct sockaddr_un));
	if (ret < 0) {
		ilm_log_err("Cannot bind socket\n");
		goto out;
	}

	ret = chmod(addr.sun_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (ret < 0) {
		ilm_log_err("Cannot chmod on socket file\n");
		goto out;
	}

	ret = listen(sock_fd, 5);
	if (ret < 0) {
		ilm_log_err("Failed to listen socket\n");
		goto out;
	}

	fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) | O_NONBLOCK);

	/* Initialize eventfd for client notification */
	if ((client_efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) == -1) {
		ilm_log_err("Cannot create eventfd");
		goto out;
	}

	ret = ilm_client_add(sock_fd, ilm_client_connect, NULL);
	if (ret < 0)
		goto out;

	return 0;

sock_fail:
	close(lock_fd);
out:
	close(sock_fd);
	return -1;
}

void ilm_client_listener_exit(void)
{
	close(lock_fd);
	close(sock_fd);
}
