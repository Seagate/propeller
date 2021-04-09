/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main (int argc, char *argv[])
{
	printf("killpath_notifier.c: path=%s\n", argv[1]);
	open(argv[1], O_RDWR | O_CREAT);
	return 0;
}
