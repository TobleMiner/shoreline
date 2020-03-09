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

#include <linux/bpf.h>

#include "network_pingxelflut.h"
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

#define MAP_FILENAME "/sys/fs/bpf/xdp/globals/xdp_pixelflut"

// // TODO
// // FIXME
// int bpf_obj_get(char* foo) {
// 	return -1;
// }

int net_pingxelflut_alloc(struct net_pingxelflut** network, struct fb* fb, struct llist* fb_list, struct fb_size* fb_size) {
	int err = 0;
	struct net_pingxelflut* net = calloc(1, sizeof(struct net_pingxelflut));
	if(!net) {
		err = -ENOMEM;
		goto fail;
	}

	net->fb = fb;
	net->fb_list = fb_list;
	net->fb_size = fb_size;
	net->map_fd = bpf_obj_get(MAP_FILENAME);
	if (net->map_fd) {
		printf("Cannot open map at %s. Check, that you run this program as root and the ebpf kernel is running.\n", MAP_FILENAME);
		err = -ENOENT;
		goto fail;
	}

	*network = net;

	return 0;

fail:
	return err;
}

void net_pingxelflut_free(struct net_pingxelflut* net) {

}

void net_pingxelflut_shutdown(struct net_pingxelflut* net) {

}

int net_pingxelflut_listen(struct net_pingxelflut* net) {
	for (int i = 0; i < net->fb_size->width; i++) {
		fb_set_pixel_rgb(net->fb, i, 10, 0xff, 0x00, 0x00);
	}
	return 0;
}
