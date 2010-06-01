/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <iv.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct handle {
	struct iv_fd fd;
	int port;
};


static int conns;

static void handler(void *_h)
{
	struct handle *h = (struct handle *)_h;
	struct sockaddr_in addr;
	socklen_t addrlen;
	int ret;

	addrlen = sizeof(addr);
	ret = accept(h->fd.fd, (struct sockaddr *)&addr, &addrlen);
	if (ret > 0) {
		char buf[128];
		int len;

		len = snprintf(buf, 128, "this is port %d\n", h->port);
		write(ret, buf, len);
		shutdown(ret, 2);
		close(ret);

		if (!(++conns % 10000))
			printf("%i\n", conns);
	}
}

int main()
{
	struct sockaddr_in addr;
	struct handle hh[1000];
	int i;

	printf("booting...\n");

	iv_init();

	for (i = 0; i < sizeof(hh) / sizeof(hh[0]); i++) {
		int sock;
		int yes;
		struct handle *h = hh + i;

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			perror("socket");
			return 1;
		}

		yes = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			       &yes, sizeof(yes)) < 0) {
			perror("setsockopt");
			return 1;
		}

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(20000 + i);
		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind");
			return 1;
		}

		listen(sock, 5);

		INIT_IV_FD(&h->fd);
		h->fd.fd = sock;
		h->fd.handler_in = handler;
		h->fd.handler_out = NULL;
		h->fd.cookie = h;
		h->port = 20000 + i;
		iv_register_fd(&h->fd);
	}

	iv_main();

	return 0;
}
