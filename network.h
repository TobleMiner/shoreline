#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <pthread.h>

enum {
	NET_STATE_IDLE,
	NET_STATE_LISTEN,
	NET_STATE_SHUTDOWN,
	NET_STATE_EXIT
}

struct net {
	unsigned int state;

	int socket;

	unsigned int num_threads;
	pthread_t* threads;
	struct net_theadargs* threadargs;
}

struct net_threadargs {
	struct net* net;
}

#endif
