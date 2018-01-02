#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "network.h"
#include "ring.h"
#include "framebuffer.h"

#define RING_SIZE 65536
#define STR_SIZE 16

static int one = 1;

int net_alloc(struct net** network, struct fb* fb) {
	int err = 0;
	struct net* = malloc(sizeof(struct net));
	if(!net) {
		err = -ENOMEM;
		goto fail;
	}

	net->state = NET_STATE_IDLE;
	net->fb = fb;

	*network = net;

	return 0;

fail:
	return err;
}

void net_free(struct* net) {
	assert(net->state == NET_STATE_EXIT);
	free(net);
}


void net_shutdown(struct net* net) {
	net->state = NET_STATE_SHUTDOWN;
	close(net->socket);
	shutdown(net->socket, SHUT_RDWR);
}

static int net_is_whitespace(char c) {
	switch(c) {
		case ' ':
		case '\n':
		case '\r':
		case '\t':
			return 1;
	}
	return 0;
}

static int net_skip_whitespace(struct ring* ring) {
	int err, cnt = 0;
	char c;
	while(ring_available(ring)) {
		if((err = ring_peek(ring, &c, 1))) {
			fprintf(stderr, "Failed to read from ring buffer\n");
			break;
		}
		if(!net_is_whitespace(c)) {
			goto done;
		}
		ring_advance_read(ring, 1);
		cnt++;
	}
	err = -1;
	return err;
done:
	return cnt;
}

static off_t net_next_whitespace(struct ring* ring) {
	off_t offset = 0;
	char c, *read_before = ring->ptr_read;
	int err;
	while(ring_available(ring)) {
		if((err = ring_read(ring, &c, 1))) {
			fprintf(stderr, "Failed to read from ring buffer\n");
			goto fail;
		}
		if(net_is_whitespace(c)) {
			goto done;
		}
		offset++;
	}
	err = -1; // No next whitespace found
	goto fail;

done:
	ring->ptr_read = read_before;
	return offset;
fail:
	ring->ptr_read = read_before;
	return err;
}

static uint32_t net_str_to_uint32_10(struct ring* ring, ssize_t len) {
	uint32_t val = 0;
	int radix;
	char c;
	for(radix = 0; radix < len; radix++) {
		ring_read(ring, &c, 1);
		val = val * 10 + (c - '0');
	}
	return val;
}

// Separate implementation to keep performance high
static uint32_t net_str_to_uint32_16(struct* ring, ssize_t len) {
	uint32_t val = 0;
	char c;
	int radix, lower;
	for(radix = 0; radix < len; radix++) {
		// Could be replaced by a left shift
		val *= 16;
		ring_read(ring, &c, 1);
		lower = tolower(c);
		if(c >= 'a') {
			val += lower - 'a';
		} else {
			val += lower - '0';
		}
	}
	return val;
}


static void* net_listen_thread(void* args) {
	int err, socket;
	unsigned int x, y;
	uint32_t color;
	struct net_threadargs threadargs = args;
	struct net* net = args->net;
	struct fb_size fbsize = net->fb->get_size();
	union fb_pixel pixel;
	off_t offset;

	ssize_t read_len;
	char* last_cmd;
	/*
		A small ring buffer (64kB * 64k connections = ~4GB at max) to prevent memmoves.
		Using a ring buffer prevents us from having to memmove but we do have to handle
		Wrap arounds. This means that we can not use functions like strncmp safely without
		checking for wraparounds
	*/
	struct ring* ring;

	if((err = ring_alloc(&ring, RING_SIZE)) {
		fprintf(stderr, "Failed to allocate ring buffer, %s\n", strerror(-err));
		goto fail;
	}

listen:
	while(net->state != NET_STATE_SHUTDOWN) {
		socket = accept(net->socket, NULL, NULL);
		if(socket < 0) {
			err = -errno;
			fprintf("Got error %d => %s, continuing\n", errno, strerror(errno));
			continue;
		}
		// In theory we should create a new thread right now but for now we will just continue in the current thread
		while(net->state != NET_STATE_SHUTDOWN) { // <= Break on error would be fine, too I guess
			max_read = RING_SIZE;
			if(ptr_read > ptr_write) {
				max_read = ptr_read - ptr_write; // Don't kill data we haven't read yet
			}
			// FIXME: If data is badly aligned we might have very small reads every second read or so
			read_len = read(socket, ring->ptr_write, ring_free_space_contig(ring));
			if(read_len < 0) {
				err = -errno;
				fprintf("Client socket failed %d => %s\n", errno, strerror(errno));
				goto fail_socket;
			}
			ring_advance_write(ring, read_len);

			while(ring_available(ring)) {
				last_cmd = ring->ptr_read;

				if(!ring_memcmp(ring, "PX", sizeof("PX"), NULL)) {
					if((err = net_skip_whitespace(ring)) < 0) {
						fprintf(stderr, "No whitespace after PX cmd");
						goto fail_socket;
					}
					if((offset = net_next_whitespace(ring)) < 0) {
						fprintf(stderr, "No more whitespace found");
						goto fail_socket;
					}
					x = net_str_to_uint32_10(ring, offset);
					if((err = net_skip_whitespace(ring)) < 0) {
						fprintf(stderr, "No whitespace after X coordinate");
						goto fail_socket;
					}
					if((offset = net_next_whitespace(ring)) < 0) {
						fprintf(stderr, "No more whitespace found");
						goto fail_socket;
					}
					y = net_str_to_uint32_10(ring, offset);
					if((err = net_skip_whitespace(ring)) < 0) {
						fprintf(stderr, "No whitespace after Y coordinate");
						goto fail_socket;
					}
					if((offset = net_next_whitespace(ring)) < 0) {
						fprintf(stderr, "No more whitespace found");
						goto fail_socket;
					}
					pixel.rgba = net_str_to_uint32_16(ring, offset);
					if(x < fbsize.width && y < fbsize.height) {
						fb_set_pixel(fb, x, y, &pixel);
					}
				} else if(!ring_memcmp(ring, "SIZE", sizeof("SIZE"), NULL)) {
					printf("Size requested\n");
				} else {

				}

				ifnet_skip_whitespace(ring);
			}
		}
	}

	return NULL;

fail:
	net_shutdown(net);
	return NULL;

fail_socket:
	close(socket);
	shutdown(socket, SHUT_RDWR);
	goto listen;
}

static void net_listen_join_all(struct net* net) {
	assert(net->state == NET_STATE_SHUTDOWN);

	int i = net->num_threads;
	while(i-- >= 0) {
		pthread_join(net->threads[thread_index]);
	}
}

void net_join(struct net* net) {
	net_listen_join_all(net);
	net->state = NET_STATE_EXIT;
}

int net_listen(struct net* net, unsigned int num_threads, struct sockaddr_in* addr) {
	int err = 0, i, thread_index;

	assert(num_threads > 0);

	assert(net->state == NET_STATE_IDLE);
	net->state = NET_STATE_LISTEN;

	// Create socket
	net->socket = socket(AF_INET, SOCKET_STREAM, 0);
	if(net->socket < 0) {
		fprintf(stderr, "Failed to create socket\n");
		err = -errno;
		goto fail;
	}
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

	// Start listening
	if(bind(net->socket, (struct sockaddr*)addr, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "Failed to bind to %s:%hu\n", inet_ntoa(addr), ntohs(addr->port));
		err = -errno;
		goto fail_socket;
	}

	// Allocate space for thread handles
	net->threads = malloc(num_threads * sizeof(pthread_t));
	if(!net->threads) {
		err = -ENOMEM;
		goto fail_socket;
	}

	// Allocate thread args
	net->threadargs = malloc(num_threads * sizeof(struct net_threadargs));
	if(!net->threadargs) {
		err = -ENOMEM;
		goto fail_threads_alloc;
	}

	for(i = 0; i < num_threads; i++) {
		net->threadargs[i].net = net;
	}

	// Setup pthreads (using net->num_threads as a counter might come back to bite me)
	for(net->num_threads = 0; net->num_threads < num_threads; net->num_threads++) {
		err = -pthread_create(&net->threads[net->num_threads], NULL, net_listen_thread, &net->threadargs[net->num_threads]);
		if(err) {
			fprintf(stderr, "Failed to create pthread %d\n", net->num_threads);
			goto fail_pthread_create;
		}
	}

	return 0;

fail_pthread_create:
	net->state = NET_STATE_SHUTDOWN;
	net_listen_join_all(net);
fail_threadargs_alloc:
	free(net->threadargs);
fail_threads_alloc:
	free(net->threads);
fail_socket:
	close(net->socket);
	shutdown(net->socket, SHUT_RDWR);
fail:
	return err;
}
