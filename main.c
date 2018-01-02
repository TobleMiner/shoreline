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

#include "framebuffer.h"
#include "network.h"

#define LISTEN_ANY "0.0.0.0"
#define LISTEN_PORT 1234

static struct net* net;

void doshutdown(int signal)
{
	net_shutdown(net);
}

int main(int argc, char** argv) {
	int err;
	struct fb* fb;
	struct sockaddr_in inaddr;

	if((err = fb_alloc(&fb, 800, 800))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	if((err = net_alloc(&net, fb))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail_fb;
	}

	if(signal(SIGINT, doshutdown))
	{
		fprintf(stderr, "Failed to bind signal\n");
		err = -EINVAL;
		goto fail_fb;
	}
	if(signal(SIGPIPE, SIG_IGN))
	{
		fprintf(stderr, "Failed to bind signal\n");
		err = -EINVAL;
		goto fail_fb;
	}

	inet_pton(AF_INET, LISTEN_ANY, &(inaddr.sin_addr.s_addr));
	inaddr.sin_port = htons(LISTEN_PORT);
	inaddr.sin_family = AF_INET;

	if((err = net_listen(net, 1, &inaddr))) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", err, strerror(-err));
		goto fail_net;
	}

	net_join(net);

fail_net:
	net_free(net);
fail_fb:
	fb_free(fb);
fail:
	return err;
}
