#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>
#include <netdb.h>
#include <time.h>
#include <stdint.h>

#include "framebuffer.h"
#include "framebuffer_time.h"
#include "sdl.h"
#include "network.h"
#include "llist.h"
#include "util.h"


#define PORT_DEFAULT "1234"
#define LISTEN_DEFAULT "::"
#define RATE_DEFAULT 60
#define WIDTH_DEFAULT 1024
#define HEIGHT_DEFAULT 768
#define RINGBUFFER_DEFAULT 65536
#define LISTEN_THREADS_DEFAULT 10

#define SPINNER_CHECK_INTERVAL 10000000UL

static bool do_exit = false;

volatile uint32_t global_counter = 0;

void doshutdown(int sig)
{
	printf("Shutting down\n");
	do_exit = true;
}

void show_usage(char* binary) {
	fprintf(stderr, "Usage: %s [-p <port>] [-b <bind address>] [-w <width>] [-h <height>] [-r <screen update rate>] [-s <ring buffer size>] [-l <number of listening threads>]\n", binary);
}

int resize_cb(struct sdl* sdl, unsigned int width, unsigned int height) {
	struct llist* fbt_list = sdl->cb_private;
	struct llist_entry* cursor;
	struct fbt* fb;
	int err;
	llist_for_each(fbt_list, cursor) {
		fb = llist_entry_get_value(cursor, struct fbt, list);
		if((err = fbt_resize(fb, width, height))) {
			return err;
		}
	}
	return 0;
}

void* counter_run(void* args) {
	(void)args;
	unsigned long long i;
	while(true) {
		for(i = 0; i < SPINNER_CHECK_INTERVAL; i++) {
			__sync_fetch_and_add(&global_counter, 1);
		}
		pthread_testcancel();
	}
}

int main(int argc, char** argv) {
	int err, opt, i = 0;
	struct fb* fb;
	struct llist fbt_list;
	struct sdl* sdl;
	struct sockaddr_storage* inaddr;
	struct addrinfo* addr_list;
	struct net* net;
	size_t addr_len;

	char* port = PORT_DEFAULT;
	char* listen_address = LISTEN_DEFAULT;

	int width = WIDTH_DEFAULT;
	int height = HEIGHT_DEFAULT;
	int screen_update_rate = RATE_DEFAULT;

	int ringbuffer_size = RINGBUFFER_DEFAULT;
	int listen_threads = LISTEN_THREADS_DEFAULT;

	unsigned long long pixel_cnt_last = 0;

	struct timespec before, after;
	long long time_delta;

	pthread_t counter_thread;

	while((opt = getopt(argc, argv, "p:b:w:h:r:s:l:?")) != -1) {
		switch(opt) {
			case('p'):
				port = optarg;
				break;
			case('b'):
				listen_address = optarg;
				break;
			case('w'):
				width = atoi(optarg);
				if(width <= 0) {
					fprintf(stderr, "Width must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('h'):
				height = atoi(optarg);
				if(height <= 0) {
					fprintf(stderr, "Height must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('r'):
				screen_update_rate = atoi(optarg);
				if(screen_update_rate <= 0) {
					fprintf(stderr, "Screen update rate must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('s'):
				ringbuffer_size = atoi(optarg);
				if(ringbuffer_size < 2) {
					fprintf(stderr, "Ring buffer size must be >= 2\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('l'):
				listen_threads = atoi(optarg);
				if(listen_threads <= 0) {
					fprintf(stderr, "Number of listening threads must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			default:
				show_usage(argv[0]);
				err = -EINVAL;
				goto fail;
		}
	}


	if((err = fb_alloc(&fb, width, height))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	llist_init(&fbt_list);
	if((err = sdl_alloc(&sdl, fb, &fbt_list))) {
		fprintf(stderr, "Failed to create SDL context\n");
		goto fail_fb;
	}

	if((err = net_alloc(&net, &fbt_list, &fb->size, ringbuffer_size))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail_sdl;
	}

	if(signal(SIGINT, doshutdown))
	{
		fprintf(stderr, "Failed to bind signal\n");
		err = -EINVAL;
		goto fail_net;
	}
	if(signal(SIGPIPE, SIG_IGN))
	{
		fprintf(stderr, "Failed to bind signal\n");
		err = -EINVAL;
		goto fail_net;
	}

	if((err = -getaddrinfo(listen_address, port, NULL, &addr_list))) {
		goto fail_net;
	}

	inaddr = (struct sockaddr_storage*)addr_list->ai_addr;
	addr_len = addr_list->ai_addrlen;

	if((err = net_listen(net, listen_threads, inaddr, addr_len))) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", err, strerror(-err));
		goto fail_addrinfo;
	}

	if((err = pthread_create(&counter_thread, NULL, counter_run, NULL))) {
		fprintf(stderr, "Failed to start spinner: %d => %s\n", err, strerror(err));
		goto fail_listen;
	}

	while(!do_exit) {
		clock_gettime(CLOCK_MONOTONIC, &before);
		llist_lock(&fbt_list);
		fb_coalesce(fb, &fbt_list);
		llist_unlock(&fbt_list);
		if(sdl_update(sdl, resize_cb)) {
			doshutdown(SIGINT);
		}
		clock_gettime(CLOCK_MONOTONIC, &after);
		time_delta = get_timespec_diff(&after, &before);
		time_delta = 1000000000UL / screen_update_rate - time_delta;
		if(time_delta > 0) {
			usleep(time_delta / 1000UL);
		}
		if(++i == screen_update_rate) {
			printf("%llu pixels/s\n", net->pixel_cnt - pixel_cnt_last);
			pixel_cnt_last = net->pixel_cnt;
		}
		i = i % screen_update_rate;
	}

	pthread_cancel(counter_thread);

fail_listen:
	net_shutdown(net);

	fbt_free_all(&fbt_list);
fail_addrinfo:
	freeaddrinfo(addr_list);
fail_net:
	net_free(net);
fail_sdl:
	sdl_free(sdl);
fail_fb:
	fb_free(fb);
fail:
	return err;
}
