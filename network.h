#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <pthread.h>

#include "framebuffer.h"
#include "llist.h"
#include "ring.h"

enum {
	NET_STATE_IDLE,
	NET_STATE_LISTEN,
	NET_STATE_SHUTDOWN,
	NET_STATE_EXIT
};

struct net;

struct net_threadargs {
	struct net* net;
};

struct net_thread {
	pthread_t thread;
	struct net_threadargs threadargs;

	struct llist* threadlist;
};

struct net {
	size_t ring_size;

	unsigned int state;

	int socket;

	struct fb* fb;

	unsigned int num_threads;
	struct net_thread* threads;
	struct fb_size* fb_size;
	pthread_mutex_t fb_lock;
	struct llist* fb_list;
};

struct net_connection_threadargs {
	struct net* net;
	int socket;
};

struct net_connection_thread {
	pthread_t thread;
	struct llist_entry list;
	struct net_connection_threadargs threadargs;

	struct ring* ring;
};

#define likely(x)	__builtin_expect((x),1)
#define unlikely(x)	__builtin_expect((x),0)

inline ssize_t net_write(int socket, char* data, size_t len) {
	off_t write_cnt = 0;
	ssize_t write_len;

	while(write_cnt < len) {
		if((write_len = write(socket, data + write_cnt, len - write_cnt)) < 0) {
			return -errno;
		}
		write_cnt += write_len;
	}
	return 0;
}

int net_alloc(struct net** network, struct fb* fb, struct llist* fb_list, struct fb_size* fb_size, size_t ring_size);
void net_free(struct net* net);


void net_shutdown(struct net* net);
int net_listen(struct net* net, unsigned int num_threads, struct sockaddr_storage* addr, size_t addr_len);

#endif
