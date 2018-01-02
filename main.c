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

#include "framebuffer.h"
#include "sdl.h"
#include "network.h"

#define LISTEN_ANY "0.0.0.0"
#define LISTEN_PORT 1234

#define UPDATE_RATE 60

static bool do_exit = false;

void doshutdown(int signal)
{
	printf("Shutting down\n");
	do_exit = true;
}

int main(int argc, char** argv) {
	int err;
	struct fb* fb;
	struct sdl* sdl;
	struct sockaddr_in inaddr;
	struct net* net;


	if((err = fb_alloc(&fb, 800, 800))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	if((err = sdl_alloc(&sdl, fb))) {
		fprintf(stderr, "Failed to create SDL context\n");
		goto fail_fb;
	}

	if((err = net_alloc(&net, fb))) {
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

	inet_pton(AF_INET, LISTEN_ANY, &(inaddr.sin_addr.s_addr));
	inaddr.sin_port = htons(LISTEN_PORT);
	inaddr.sin_family = AF_INET;

	if((err = net_listen(net, 1, &inaddr))) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", err, strerror(-err));
		goto fail_net;
	}

	while(!do_exit) {
		sdl_update(sdl);
		usleep(1000000UL / UPDATE_RATE);
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
