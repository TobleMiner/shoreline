#ifndef _SIMPLE_RFB_H_
#define _STMPLE_RFB_H_

#include <stdbool.h>
#include <stdint.h>

#include "framebuffer.h"
#include "network.h"

struct simple_rfb_frontend {
	struct frontend front;
	struct fb* fb;
	int socket;
	char* listen_port;
	char* listen_address;
	struct sockaddr_storage listen_addr;
	bool thread_created;
	pthread_t listen_thread;
	struct addrinfo* addr_list;
	bool exit;
};

#endif
