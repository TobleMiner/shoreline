#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <pthread.h>

#include "framebuffer.h"

enum {
	NET_STATE_IDLE,
	NET_STATE_LISTEN,
	NET_STATE_SHUTDOWN,
	NET_STATE_EXIT
};

struct net_threadargs;

struct net {
	unsigned int state;

	int socket;

	unsigned int num_threads;
	pthread_t* threads;
	struct net_threadargs* threadargs;
	struct fb* fb;
};

struct net_threadargs {
	struct net* net;
};



int net_alloc(struct net** network, struct fb* fb);
void net_free(struct net* net);


void net_shutdown(struct net* net);
void net_join(struct net* net);
int net_listen(struct net* net, unsigned int num_threads, struct sockaddr_in* addr);



#endif
