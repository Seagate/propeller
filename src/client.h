#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#include "list.h"

struct ilm_lockspace;

struct client {
	struct list_head list;
	int fd;  /* unset is -1 */
	int pid; /* unset is -1 */
	int state;
	struct ilm_lockspace *ls;
	pthread_mutex_t mutex;
	int (*workfn)(struct client *);
	int (*deadfn)(struct client *);
};

#define ILM_MSG_MAGIC		0x494C4D00

struct ilm_msg_header {
	uint32_t magic;
	uint32_t cmd;
	uint32_t length;
	uint32_t result;
};

int ilm_client_is_updated(void);
int ilm_client_alloc_pollfd(struct pollfd **poll_fd, int *num);
int ilm_client_handle_request(struct pollfd *poll_fd, int num);
void ilm_send_result(int fd, int result, char *data, int data_len);
int ilm_client_listener_init(void);
void ilm_client_listener_exit(void);
int ilm_client_suspend(struct client *cl);
int ilm_client_resume(struct client *cl);

#endif
