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

#include "framebuffer.h"
#include "sdl.h"
#include "network.h"


#define PORT_DEFAULT 1234
#define LISTEN_DEFAULT "0.0.0.0"
#define RATE_DEFAULT 60
#define WIDTH_DEFAULT 1024
#define HEIGHT_DEFAULT 768
#define RINGBUFFER_DEFAULT 65536
#define LISTEN_THREADS_DEFAULT 10

static bool do_exit = false;

void doshutdown(int signal)
{
	printf("Shutting down\n");
	do_exit = true;
}

void show_usage(char* binary) {
	fprintf(stderr, "Usage: %s [-p <port>] [-b <bind address>] [-w <width>] [-h <height>] [-r <screen update rate>] [-s <ring buffer size>] [-l <number of listening threads>]\n", binary);
}

int main(int argc, char** argv) {
	int err;
	char opt;
	struct fb* fb;
	struct sdl* sdl;
	struct sockaddr_in inaddr;
	struct net* net;

	unsigned short port = PORT_DEFAULT;
	char* listen_address = LISTEN_DEFAULT;

	int width = WIDTH_DEFAULT;
	int height = HEIGHT_DEFAULT;
	int screen_update_rate = RATE_DEFAULT;

	int ringbuffer_size = RINGBUFFER_DEFAULT;
	int listen_threads = LISTEN_THREADS_DEFAULT;

	while((opt = getopt(argc, argv, "p:b:w:h:r:s:l:?")) != -1) {
		switch(opt) {
			case('p'):
				port = (unsigned short)strtoul(optarg, NULL, 10);
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

	if((err = sdl_alloc(&sdl, fb))) {
		fprintf(stderr, "Failed to create SDL context\n");
		goto fail_fb;
	}

	if((err = net_alloc(&net, fb, ringbuffer_size))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail_sdl;
	}

	if(signal(SIGINT, doshutdown))
	{
		fprintf(stderr, "Failed to bind signal\n");
		err = -EINVAL;
		goto fail_sdl;
	}
	if(signal(SIGPIPE, SIG_IGN))
	{
		fprintf(stderr, "Failed to bind signal\n");
		err = -EINVAL;
		goto fail_sdl;
	}

	inet_pton(AF_INET, listen_address, &(inaddr.sin_addr.s_addr));
	inaddr.sin_port = htons(port);
	inaddr.sin_family = AF_INET;

	if((err = net_listen(net, listen_threads, &inaddr))) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", err, strerror(-err));
		goto fail_net;
	}

	while(!do_exit) {
		sdl_update(sdl);
		usleep(1000000UL / screen_update_rate);
	}
	net_shutdown(net);
	net_join(net);

fail_net:
	net_free(net);
fail_sdl:
	sdl_free(sdl);
fail_fb:
	fb_free(fb);
fail:
	return err;
}
