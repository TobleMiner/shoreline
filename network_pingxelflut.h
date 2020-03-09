#ifndef _NETWORK_PINGXELFLUT_H_
#define _NETWORK_PINGXELFLUT_H_

#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdbool.h>

struct net_pingxelflut;

#include "framebuffer.h"
#include "llist.h"
#include "ring.h"
#include "statistics.h"

struct net_pingxelflut {
	struct fb* fb;

	struct fb_size* fb_size;
	pthread_mutex_t fb_lock;
	struct llist* fb_list;

	int map_fd;
};

#define likely(x)	__builtin_expect((x),1)
#define unlikely(x)	__builtin_expect((x),0)

int net_pingxelflut_alloc(struct net_pingxelflut** network, struct fb* fb, struct llist* fb_list, struct fb_size* fb_size);
void net_pingxelflut_free(struct net_pingxelflut* net);


void net_pingxelflut_shutdown(struct net_pingxelflut* net);
int net_pingxelflut_listen(struct net_pingxelflut* net);

#endif
