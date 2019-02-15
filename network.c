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

#include "network.h"
#include "ring.h"
#include "framebuffer.h"
#include "llist.h"
#include "util.h"

#define CONNECTION_QUEUE_SIZE 16
#define THREAD_NAME_MAX 16
#define SIZE_INFO_MAX 32
#define WHITESPACE_SEARCH_GARBAGE_THRESHOLD 32

/* Theory Of Operation
 * ===================
 *
 * Using net_alloc the caller grabs a net struct. After that
 * he uses net_listen with the desired number of threads used
 * for accepting connections.
 *
 * Each of the threads used for accepting connections starts
 * another thread for each connection it accepted.
 *
 * The newly created thread then sets up a ring buffer to avoid
 * memmoves while parsing and starts reading data to it.
 * After receiving any number of bytes the thread tries to read
 * a valid command verb from the current read position in the
 * ring buffer. If that fails the thread tries to skip to the
 * next whitespace-separated token and tries to parse it as a
 * command verb. One a valid command verb is detected the
 * parser tries to fetch all arguments required to form a
 * complete command.
 * If there are any required parts missing from a command the
 * parser will assume that it has simply not been received yet
 * and go back to reading from the socket.
 */
static int one = 1;

int net_alloc(struct net** network, struct llist* fb_list, struct fb_size* fb_size, size_t ring_size) {
	int err = 0;
	struct net* net = calloc(1, sizeof(struct net));
	if(!net) {
		err = -ENOMEM;
		goto fail;
	}

	net->state = NET_STATE_IDLE;
	net->fb_list = fb_list;
	net->fb_size = fb_size;
	pthread_mutex_init(&net->fb_lock, NULL);
	net->ring_size = ring_size;

	*network = net;

	return 0;

fail:
	return err;
}

void net_free(struct net* net) {
	assert(net->state == NET_STATE_EXIT || net->state == NET_STATE_IDLE);
	if(net->threads) {
		free(net->threads);
	}
	free(net);
}


static void net_kill_threads(struct net* net) {
	int i = net->num_threads;
	struct net_thread* thread;
	net->state = NET_STATE_SHUTDOWN;
	while(i-- > 0) {
		thread = &net->threads[i];
		pthread_cancel(thread->thread);
		pthread_join(thread->thread, NULL);
	}
}

void net_shutdown(struct net* net) {
	net_kill_threads(net);
	shutdown(net->socket, SHUT_RDWR);
	close(net->socket);
	net->state = NET_STATE_EXIT;
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
	int cnt = 0;
	char c;
	while(ring_any_available(ring)) {
		c = ring_peek_one(ring);
		if(!net_is_whitespace(c)) {
			goto done;
		}
		ring_inc_read(ring);
		cnt++;
	}
done:
	return cnt ? cnt : -1;
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
		if(offset++ >= WHITESPACE_SEARCH_GARBAGE_THRESHOLD) {
			err = -EINVAL;
			goto fail;
		}
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
		if(lower >= 'a') {
			val += lower - 'a' + 10;
		} else {
			val += lower - '0';
		}
	}
	return val;
}

static void net_connection_thread_cleanup_ring(void* args) {
	struct net_connection_thread* thread = args;
	ring_free(thread->ring);
}

static void net_connection_thread_cleanup_socket(void* args) {
	struct net_connection_thread* thread = args;
	shutdown(thread->threadargs.socket, SHUT_RDWR);
	close(thread->threadargs.socket);
}

static void net_connection_thread_cleanup_self(void* args) {
	struct net_connection_thread* thread = args;
	llist_remove(&thread->list);
	free(thread);
}

static void* net_connection_thread(void* args) {
	struct net_connection_threadargs* threadargs = args;
	int err, socket = threadargs->socket;
	struct net* net = threadargs->net;
	struct net_connection_thread* thread =
		container_of(threadargs, struct net_connection_thread, threadargs);

	unsigned numa_node = get_numa_node();
	struct fb* fb;
	struct fb_size* fbsize;
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
	int size_info_len;

	cpu_set_t nodemask;
	int cpuid = sched_getcpu();
	if(cpuid < 0) {
		fprintf(stderr, "Failed to get cpuid of network thread, continuing without affinity setting\n");
	} else {
		CPU_ZERO(&nodemask);
		CPU_SET(cpuid, &nodemask);
		if((err = pthread_setaffinity_np(thread->thread, sizeof(nodemask), &nodemask))) {
			fprintf(stderr, "Failed to set cpu affinity, continuing without affinity setting: %s (%d)\n", strerror(err), err);
		}
	}

	pthread_mutex_lock(&net->fb_lock);
	fb = fb_get_fb_on_node(net->fb_list, numa_node);
	if(!fb) {
		printf("Failed to find fb on NUMA node %u, creating new fb\n", numa_node);
		if(fb_alloc(&fb, net->fb_size->width, net->fb_size->height)) {
			fprintf(stderr, "Failed to allocate fb on node\n");
			goto fail;
		}
		printf("Allocated fb on NUMA node %u\n", fb->numa_node);
		llist_append(net->fb_list, &fb->list);
	}
	pthread_mutex_unlock(&net->fb_lock);

	fbsize = fb_get_size(fb);

	pthread_cleanup_push(net_connection_thread_cleanup_self, thread);
	pthread_cleanup_push(net_connection_thread_cleanup_socket, thread);


	if((err = ring_alloc(&ring, net->ring_size))) {
		fprintf(stderr, "Failed to allocate ring buffer, %s\n", strerror(-err));
		goto fail_socket;
	}
	thread->ring = ring;

	pthread_cleanup_push(net_connection_thread_cleanup_ring, thread);
recv:
	while(net->state != NET_STATE_SHUTDOWN) {
		// FIXME: If data is badly aligned we might have very small reads every second read or so
		read_len = read(socket, ring->ptr_write, ring_free_space_contig(ring));
		if(read_len <= 0) {
			if(read_len < 0) {
				err = -errno;
				fprintf(stderr, "Client socket failed %d => %s\n", errno, strerror(errno));
			}
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
				if(offset > 6) {
					pixel.abgr = net_str_to_uint32_16(ring, offset);
				} else {
					pixel.abgr = net_str_to_uint32_16(ring, offset) << 8;
					pixel.color.alpha = 0xFF;
				}
//				printf("Got pixel command: PX %u %u %06x\n", x, y, pixel.rgba);
				if(x < fbsize->width && y < fbsize->height) {
					fb_set_pixel(fb, x, y, &pixel);
				} else {
//					printf("Got pixel outside screen area: %u, %u outside %u, %u\n", x, y, fbsize->width, fbsize->height);
				}
			} else if(!ring_memcmp(ring, "SIZE", strlen("SIZE"), NULL)) {
				size_info_len = snprintf(size_info, SIZE_INFO_MAX, "SIZE %u %u\n", fbsize->width, fbsize->height);
				if(size_info_len >= SIZE_INFO_MAX) {
					fprintf(stderr, "SIZE output too long\n");
					goto fail_ring;
				}
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
					if(offset == -EINVAL) {
						// We have a missbehaving client
						goto fail_ring;
					}
					goto recv;
				}
			}

			net_skip_whitespace(ring);
		}
	}

fail_ring:
	pthread_cleanup_pop(true);
fail_socket:
	pthread_cleanup_pop(true);
	pthread_cleanup_pop(true);
fail:
	return NULL;

recv_more:
	ring->ptr_read = last_cmd;
	goto recv;
}

static void net_listen_thread_cleanup_threadlist(void* args) {
	struct net_thread* thread = args;
	struct llist* threadlist = thread->threadlist;
	struct net_connection_thread* conn_thread;

	llist_lock(threadlist);
	while(threadlist->head) {
		conn_thread = llist_entry_get_value(threadlist->head, struct net_connection_thread, list);
		pthread_cancel(conn_thread->thread);
		llist_unlock(threadlist)
		pthread_join(conn_thread->thread, NULL);
		llist_lock(threadlist);
	}
	llist_unlock(threadlist);
	llist_free(threadlist);
}


static void* net_listen_thread(void* args) {
	int err, socket;
	struct net_threadargs* threadargs = args;
	struct net* net = threadargs->net;
	struct net_thread* thread = container_of(args, struct net_thread, threadargs);

	struct llist* threadlist;
	struct net_connection_thread* conn_thread;

	if((err = llist_alloc(&threadlist))) {
		fprintf(stderr, "Failed to allocate thread list\n");
		goto fail;
	}
	thread->threadlist = threadlist;

	pthread_cleanup_push(net_listen_thread_cleanup_threadlist, thread);

	while(net->state != NET_STATE_SHUTDOWN) {
		socket = accept(net->socket, NULL, NULL);
		if(socket < 0) {
			err = -errno;
			fprintf(stderr, "Got error %d => %s, shutting down\n", errno, strerror(errno));
			goto fail_threadlist;
		}
		printf("Got a new connection\n");

		conn_thread = malloc(sizeof(struct net_connection_thread));
		if(!conn_thread) {
			fprintf(stderr, "Failed to allocate memory for connection thread\n");
			goto fail_connection;
		}
		llist_entry_init(&conn_thread->list);
		conn_thread->threadargs.socket = socket;
		conn_thread->threadargs.net = net;

		if((err = -pthread_create(&conn_thread->thread, NULL, net_connection_thread, &conn_thread->threadargs))) {
			fprintf(stderr, "Failed to create thread: %d => %s\n", err, strerror(-err));
			goto fail_thread_entry;
		}

		llist_append(threadlist, &conn_thread->list);

		continue;

fail_thread_entry:
		free(conn_thread);
fail_connection:
		shutdown(socket, SHUT_RDWR);
		close(socket);
	}
fail_threadlist:
	pthread_cleanup_pop(true);
fail:
	return NULL;

}

int net_listen(struct net* net, unsigned int num_threads, struct sockaddr_storage* addr, size_t addr_len) {
	int err = 0, i;
	char threadname[THREAD_NAME_MAX];
	char host_tmp[NI_MAXHOST], port_tmp[NI_MAXSERV];

	assert(num_threads > 0);

	assert(net->state == NET_STATE_IDLE);
	net->state = NET_STATE_LISTEN;

	// Create socket
	net->socket = socket(addr->ss_family, SOCK_STREAM, 0);
	if(net->socket < 0) {
		fprintf(stderr, "Failed to create socket\n");
		err = -errno;
		goto fail;
	}
	setsockopt(net->socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

	assert(!getnameinfo((struct sockaddr*)addr, addr_len, host_tmp, NI_MAXHOST, port_tmp, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV));

	// Start listening
	if(bind(net->socket, (struct sockaddr*)addr, addr_len) < 0) {
		fprintf(stderr, "Failed to bind to %s:%s\n", host_tmp, port_tmp);
		err = -errno;
		goto fail_socket;
	}

	if(listen(net->socket, CONNECTION_QUEUE_SIZE)) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", errno, strerror(errno));
		err = -errno;
		goto fail_socket;
	}

	printf("Listening on %s:%s\n", host_tmp, port_tmp);

	// Allocate space for threads
	net->threads = malloc(num_threads * sizeof(struct net_thread));
	if(!net->threads) {
		err = -ENOMEM;
		goto fail_socket;
	}

	for(i = 0; i < num_threads; i++) {
		net->threads[i].threadargs.net = net;
	}

	// Setup pthreads (using net->num_threads as a counter might come back to bite me)
	for(net->num_threads = 0; net->num_threads < num_threads; net->num_threads++) {
		err = -pthread_create(&net->threads[net->num_threads].thread, NULL, net_listen_thread, &net->threads[net->num_threads].threadargs);
		if(err) {
			fprintf(stderr, "Failed to create pthread %d\n", net->num_threads);
			goto fail_pthread_create;
		}
		snprintf(threadname, THREAD_NAME_MAX, "network %d", net->num_threads);
		pthread_setname_np(net->threads[net->num_threads].thread, threadname);
	}

	return 0;

fail_pthread_create:
	net_kill_threads(net);
//fail_threads_alloc:
	free(net->threads);
fail_socket:
	close(net->socket);
	shutdown(net->socket, SHUT_RDWR);
fail:
	net->state = NET_STATE_IDLE;
	return err;
}
