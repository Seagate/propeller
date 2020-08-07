/*
 * Copyright (C) 2020-2021 Seagate
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include "client.h"
#include "cmd.h"
#include "inject_fault.h"
#include "log.h"
#include "util.h"

static volatile int _failure_percentage = 0;
static int _is_injected;

int ilm_inject_fault_set_percentage(struct ilm_cmd *cmd)
{
	int percentage, ret;

	ret = recv(cmd->cl->fd, &percentage, sizeof(int), MSG_WAITALL);
	if (ret != sizeof(int)) {
		ilm_log_err("Fail to receive percentage %d errno %d\n",
			    cmd->cl->fd, errno);
		ret = -EIO;
		goto out;
	}

	if (percentage > 100 || percentage < 0) {
		ilm_log_err("Inject percentage is out of range %d\n",
			    percentage);
		ret = -EINVAL;
		goto out;
	}

	_failure_percentage = percentage;
	__sync_synchronize();

out:
	ilm_send_result(cmd->cl->fd, ret, NULL, 0);
	return ret;
}

void ilm_inject_fault_update(int total, int index)
{
	int step;

	if (!_failure_percentage) {
		_is_injected = 0;
		return;
	}

	if (_failure_percentage == 100) {
		_is_injected = 1;
		return;
	}

	if (_failure_percentage > 50) {
		step = (100 + (100 - _failure_percentage) / 2) /
				(100 - _failure_percentage);
	} else {
		step = (100 + (_failure_percentage / 2)) /
				_failure_percentage;
	}

	if (!(index % step)) {
		if (_failure_percentage > 50)
			_is_injected = 0;
		else
			_is_injected = 1;
	} else {
		if (_failure_percentage > 50)
			_is_injected = 1;
		else
			_is_injected = 0;
	}

	return;
}

int ilm_inject_fault_is_hit(void)
{
	return _is_injected;
}
