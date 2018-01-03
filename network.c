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

#include "network.h"
#include "ring.h"
#include "framebuffer.h"
#include "llist.h"
#include "util.h"

#define CONNECTION_QUEUE_SIZE 16
#define THREAD_NAME_MAX 16
#define SIZE_INFO_MAX 32

static int one = 1;

int net_alloc(struct net** network, struct fb* fb, size_t ring_size) {
	int err = 0;
	struct net* net = malloc(sizeof(struct net));
	if(!net) {
		err = -ENOMEM;
		goto fail;
	}

	net->state = NET_STATE_IDLE;
	net->fb = fb;
	net->ring_size = ring_size;

	*network = net;

	return 0;

fail:
	return err;
}

void net_free(struct net* net) {
	assert(net->state == NET_STATE_EXIT);
	free(net);
}


void net_shutdown(struct net* net) {
	net->state = NET_STATE_SHUTDOWN;
	fcntl(net->socket, F_SETFL, O_NONBLOCK);
	int i = net->num_threads;
	while(i-- > 0) {
		pthread_kill(net->threads[i], SIGINT);
	}
	close(net->socket);
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
	while(ring_any_available(ring)) {
		c = ring_peek_one(ring);
		if(!net_is_whitespace(c)) {
			goto done;
		}
		ring_inc_read(ring);
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
	while(ring_any_available(ring)) {
		c = ring_read_one(ring);
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
		c = ring_read_one(ring);
		val = val * 10 + (c - '0');
	}
	return val;
}

// Separate implementation to keep performance high
static uint32_t net_str_to_uint32_16(struct ring* ring, ssize_t len) {
	uint32_t val = 0;
	char c;
	int radix, lower;
	for(radix = 0; radix < len; radix++) {
		// Could be replaced by a left shift
		val *= 16;
		c = ring_read_one(ring);
		lower = tolower(c);
		if(c >= 'a') {
			val += lower - 'a' + 10;
		} else {
			val += lower - '0';
		}
	}
	return val;
}

static void* net_connection_thread(void* args) {
	struct net_connection_threadargs* threadargs = args;
	int err, socket = threadargs->socket;
	struct net* net = threadargs->net;
	struct net_connection_thread* thread =
		container_of(threadargs, struct net_connection_thread, threadargs);

	struct fb* fb = net->fb;
	struct fb_size fbsize = fb_get_size(fb);
	union fb_pixel pixel;
	unsigned int x, y;

	off_t offset;
	ssize_t read_len, write_len, write_cnt;
	char* last_cmd;

	/*
		A small ring buffer (64kB * 64k connections = ~4GB at max) to prevent memmoves.
		Using a ring buffer prevents us from having to memmove but we do have to handle
		Wrap arounds. This means that we can not use functions like strncmp safely without
		checking for wraparounds
	*/
	struct ring* ring;

	char size_info[SIZE_INFO_MAX];

	int size_info_len = snprintf(size_info, SIZE_INFO_MAX, "SIZE %u %u\n", fbsize.width, fbsize.height);


	if((err = ring_alloc(&ring, net->ring_size))) {
		fprintf(stderr, "Failed to allocate ring buffer, %s\n", strerror(-err));
		goto fail_socket;
	}


recv:
	while(net->state != NET_STATE_SHUTDOWN) { // <= Break on error would be fine, too I guess
		// FIXME: If data is badly aligned we might have very small reads every second read or so
		read_len = read(socket, ring->ptr_write, ring_free_space_contig(ring));
		if(read_len <= 0) {
			err = -errno;
			fprintf(stderr, "Client socket failed %d => %s\n", errno, strerror(errno));
			goto fail_ring;
		}
//		printf("Read %zd bytes\n", read_len);
		ring_advance_write(ring, read_len);

		while(ring_any_available(ring)) {
			last_cmd = ring->ptr_read;

			if(!ring_memcmp(ring, "PX", strlen("PX"), NULL)) {
				if((err = net_skip_whitespace(ring)) < 0) {
//					fprintf(stderr, "No whitespace after PX cmd\n");
					goto recv_more;
				}
				if((offset = net_next_whitespace(ring)) < 0) {
//					fprintf(stderr, "No more whitespace found, missing X\n");
					goto recv_more;
				}
				x = net_str_to_uint32_10(ring, offset);
				if((err = net_skip_whitespace(ring)) < 0) {
//					fprintf(stderr, "No whitespace after X coordinate\n");
					goto recv_more;
				}
				if((offset = net_next_whitespace(ring)) < 0) {
//					fprintf(stderr, "No more whitespace found, missing Y\n");
					goto recv_more;
				}
				y = net_str_to_uint32_10(ring, offset);
				if((err = net_skip_whitespace(ring)) < 0) {
//					fprintf(stderr, "No whitespace after Y coordinate\n");
					goto recv_more;
				}
				if((offset = net_next_whitespace(ring)) < 0) {
//					fprintf(stderr, "No more whitespace found, missing color\n");
					goto recv_more;
				}
				pixel.rgba = net_str_to_uint32_16(ring, offset);
//				printf("Got pixel command: PX %u %u %06x\n", x, y, pixel.rgba);
				if(x < fbsize.width && y < fbsize.height) {
					fb_set_pixel(fb, x, y, &pixel);
				}
			} else if(!ring_memcmp(ring, "SIZE", strlen("SIZE"), NULL)) {
				write_cnt = 0;
				while(write_cnt < size_info_len) {
					if((write_len = write(socket, size_info + write_cnt, size_info_len - write_cnt)) < 0) {
						fprintf(stderr, "Failed to write to socket: %d => %s\n", errno, strerror(errno));
						err = -errno;
						goto fail_ring;
					}
					write_cnt += write_len;
				}
			} else {
				if((offset = net_next_whitespace(ring)) >= 0) {
					printf("Encountered unknown command\n");
					ring_advance_read(ring, offset);
				} else {
					goto recv;
				}
			}

			net_skip_whitespace(ring);
		}
	}

fail_ring:
	ring_free(ring);
fail_socket:
	llist_remove(&thread->list);
	free(thread);

	shutdown(socket, SHUT_RDWR);
	close(socket);
	return NULL;

recv_more:
	ring->ptr_read = last_cmd;
	goto recv;
}

static void* net_listen_thread(void* args) {
	int err, socket;
	struct net_threadargs* threadargs = args;
	struct net* net = threadargs->net;

	struct llist* threadlist;
	struct net_connection_thread* thread;

	if((err = llist_alloc(&threadlist))) {
		fprintf(stderr, "Failed to allocate thread list\n");
		goto fail;
	}

	while(net->state != NET_STATE_SHUTDOWN) {
		socket = accept(net->socket, NULL, NULL);
		if(socket < 0) {
			err = -errno;
			fprintf(stderr, "Got error %d => %s, continuing\n", errno, strerror(errno));
			goto fail_threadlist;
		}
		printf("Got a new connection\n");

		thread = malloc(sizeof(struct net_connection_thread));
		if(!thread) {
			fprintf(stderr, "Failed to allocate memory for connection thread\n");
			goto fail_connection;
		}
		llist_entry_init(&thread->list);
		thread->threadargs.socket = socket;
		thread->threadargs.net = net;

		if((err = -pthread_create(&thread->thread, NULL, net_connection_thread, &thread->threadargs))) {
			fprintf(stderr, "Failed to create thread: %d => %s\n", err, strerror(-err));
			goto fail_thread_entry;
		}

		llist_append(threadlist, &thread->list);

		continue;

fail_thread_entry:
		free(thread);
fail_connection:
		shutdown(socket, SHUT_RDWR);
		close(socket);
	}
fail_threadlist:
	llist_lock(threadlist);
	while(threadlist->head) {
		thread = llist_entry_get_value(threadlist->head, struct net_connection_thread, list);
		pthread_kill(thread->thread, SIGINT);
		llist_unlock(threadlist)
		pthread_join(thread->thread, NULL);
		llist_lock(threadlist);
	}
	llist_unlock(threadlist);
	llist_free(threadlist);
fail:
	net_shutdown(net);
	return NULL;

}

static void net_listen_join_all(struct net* net) {
	int i = net->num_threads;
	while(i-- > 0) {
		pthread_join(net->threads[i], NULL);
	}
}

void net_join(struct net* net) {
	net_listen_join_all(net);
	shutdown(net->socket, SHUT_RDWR);
	net->state = NET_STATE_EXIT;
}

int net_listen(struct net* net, unsigned int num_threads, struct sockaddr_in* addr) {
	int err = 0, i;
	char threadname[THREAD_NAME_MAX];

	assert(num_threads > 0);

	assert(net->state == NET_STATE_IDLE);
	net->state = NET_STATE_LISTEN;

	// Create socket
	net->socket = socket(AF_INET, SOCK_STREAM, 0);
	if(net->socket < 0) {
		fprintf(stderr, "Failed to create socket\n");
		err = -errno;
		goto fail;
	}
	setsockopt(net->socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

	// Start listening
	if(bind(net->socket, (struct sockaddr*)addr, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "Failed to bind to %s:%hu\n", inet_ntoa(*((struct in_addr*)addr)), ntohs(addr->sin_port));
		err = -errno;
		goto fail_socket;
	}

	if(listen(net->socket, CONNECTION_QUEUE_SIZE)) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", errno, strerror(errno));
		err = -errno;
		goto fail_socket;
	}

	printf("Listening on %s:%hu\n", inet_ntoa(*((struct in_addr*)(&addr->sin_addr))), ntohs(addr->sin_port));

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
		snprintf(threadname, THREAD_NAME_MAX, "network %d", net->num_threads);
		pthread_setname_np(net->threads[net->num_threads], threadname);
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
