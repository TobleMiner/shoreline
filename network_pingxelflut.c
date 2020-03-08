#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <sched.h>
#include <stdarg.h>

#include "network.h"
#include "ring.h"
#include "framebuffer.h"
#include "llist.h"
#include "util.h"

#define CONNECTION_QUEUE_SIZE 16
#define THREAD_NAME_MAX 16
#define SCRATCH_STR_MAX 32
#define WHITESPACE_SEARCH_GARBAGE_THRESHOLD 32

#if DEBUG > 1
#define debug_printf(...) printf(__VA_ARGS__)
#define debug_fprintf(s, ...) fprintf(s, __VA_ARGS__)
#else
#define debug_printf(...)
#define debug_fprintf(...)
#endif

int net_pingxelflut_alloc(struct net** network, struct fb* fb, struct llist* fb_list, struct fb_size* fb_size) {
	// *((int*)0) = 42; // SEGFAULT
	return 0;
}

void net_pingxelflut_free(struct net* net) {

}

void net_pingxelflut_shutdown(struct net* net) {

}

int net_pingxelflut_listen(struct net* net) {

	return 0;
}
