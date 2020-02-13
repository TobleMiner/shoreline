#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "simple_rfb.h"
#include "llist.h"
#include "main.h"

#define LISTEN_PORT_DEFAULT "1236"
#define LISTEN_ADDRESS_DEFAULT "::"

static int simple_rfb_frontend_alloc(struct frontend** ret, struct fb* fb, void* priv) {
	struct simple_rfb_frontend* front = calloc(1, sizeof(struct simple_rfb_frontend));
	if(!front) {
		return -ENOMEM;
	}
	front->fb =fb;
	front->socket = -1;
	front->listen_port = LISTEN_PORT_DEFAULT;
	front->listen_address = LISTEN_ADDRESS_DEFAULT;
	*ret = &front->front;
	return 0;
}

static void* rfb_thread(void* args) {
	struct simple_rfb_frontend* front = args;

	while(!front->exit) {
		size_t len;
		ssize_t write_len;
		char* buf;
		int sock = accept(front->socket, NULL, NULL);
		if(sock < 0) {
			fprintf(stderr, "Listener failed, bailing out: %d => %s\n", errno, strerror(errno));
			break;
		}

		len = front->fb->size.width * front->fb->size.height * sizeof(union fb_pixel);
		buf = (char*)front->fb->pixels;
		while(len > 0) {
			write_len = write(sock, buf, len);
			if(write_len < 0) {
				fprintf(stderr, "Failed to write to simple RFB client: %d => %s\n", errno, strerror(errno));
				break;
			}
			len -= write_len;
			buf += write_len;
		}
		shutdown(sock, SHUT_RDWR);
		close(sock);
	}
	return NULL;
}

static int simple_rfb_frontend_start(struct frontend* front) {
	struct simple_rfb_frontend* sfront = container_of(front, struct simple_rfb_frontend, front);
	int err;
	int sock;
	int one = 1;
	struct addrinfo* addr_list;
	size_t addr_len;
	struct sockaddr_storage* listen_addr;

	if((err = -getaddrinfo(sfront->listen_address, sfront->listen_port, NULL, &addr_list))) {
		fprintf(stderr, "Failed to resolve listen address for simple rfb '%s', %d => %s\n", sfront->listen_address, err, gai_strerror(-err));
		goto fail;
	}
	sfront->addr_list = addr_list;
	listen_addr = (struct sockaddr_storage*)addr_list->ai_addr;
	addr_len = addr_list->ai_addrlen;

	if((sock = socket(listen_addr->ss_family, SOCK_STREAM, 0)) < 0) {
		err = -errno;
		goto fail_addrlist;
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

	if(bind(sock, (struct sockaddr*)listen_addr, addr_len) < 0) {
		fprintf(stderr, "Failed to bind to %s:%s %d => %s\n", sfront->listen_address, sfront->listen_port, errno, strerror(errno));
		err = -errno;
		goto fail_socket;
	}

	if(listen(sock, 10)) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", errno, strerror(errno));
		err = -errno;
		goto fail_socket;
	}
	sfront->socket = sock;

	if((err = -pthread_create(&sfront->listen_thread, NULL, rfb_thread, sfront))) {
		goto fail_socket;
	}
	sfront->thread_created = true;

	return 0;

fail_socket:
	close(sock);
	sfront->socket = -1;
fail_addrlist:
	freeaddrinfo(addr_list);
	sfront->addr_list = NULL;
fail:
	return err;
}

static void simple_rfb_frontend_free(struct frontend* front) {
	struct simple_rfb_frontend* sfront = container_of(front, struct simple_rfb_frontend, front);
	sfront->exit = true;
	if(sfront->thread_created) {
		pthread_cancel(sfront->listen_thread);
		pthread_join(sfront->listen_thread, NULL);
	}
	if(sfront->socket >= 0) {
		close(sfront->socket);
	}
	if(sfront->addr_list) {
		freeaddrinfo(sfront->addr_list);
	}
	free(sfront);
}

static int simple_rfb_frontend_configure_port(struct frontend* front, char* value) {
	struct simple_rfb_frontend* sfront = container_of(front, struct simple_rfb_frontend, front);
	if(!value) {
		return -EINVAL;
	}

	int port = atoi(value);
	if(port < 0 || port > 65535) {
		return -EINVAL;
	}

	sfront->listen_port = value;

	return 0;
}

static int simple_rfb_frontend_configure_listen_address(struct frontend* front, char* value) {
	struct simple_rfb_frontend* sfront = container_of(front, struct simple_rfb_frontend, front);
	if(!value) {
		return -EINVAL;
	}

	sfront->listen_address = value;
	return 0;
}

static const struct frontend_ops fops = {
	.alloc = simple_rfb_frontend_alloc,
	.start = simple_rfb_frontend_start,
	.free = simple_rfb_frontend_free,
};

static const struct frontend_arg fargs[] = {
	{ .name = "port", .configure = simple_rfb_frontend_configure_port },
	{ .name = "listen", .configure = simple_rfb_frontend_configure_listen_address },
	{ .name = "", .configure = NULL },
};

DECLARE_FRONTEND_SIG_ARGS(front_simple_rfb, "FB remote provider frontend", &fops, fargs);
