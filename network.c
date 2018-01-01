#include <stdlib.h>
#include <stdio.h>

#include "network.h"

#define RING_SIZE 65536
#define STR_SIZE 16

static int one = 1;

int net_alloc(struct net** network) {
	int err = 0;
	struct net* = malloc(sizeof(struct net));
	if(!net) {
		err = -ENOMEM;
		goto fail;
	}

	net->state = NET_STATE_IDLE;

	*network = net;

	return 0;

fail:
	return err;
}

void net_free(struct* net) {
	assert(net->state == NET_STATE_EXIT);
	free(net);
}


static void* net_listen_thread(void* args) {
	int err, socket;
	struct net_threadargs threadargs = args;
	struct net* net = args->net;

	ssize_t read_len, max_read;
	/*
		A small ring buffer (64kB * 64k connections = ~4GB at max) to prevent memmoves.
		Using a ring buffer prevents us from having to memmove but we do have to handle
		Wrap arounds. This means that we can not use functions like strncmp safely without
		checking for wraparounds
	*/
	char* ring[RING_SIZE];
	char* ptr_read = ring, *ptr_parse;
	char* ptr_write = ring;
	// Use only if there is a wrap within next n bytes
	char* str_store[STR_SIZE];
	unsigned int str_len = 0;

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
			read_len = read(socket, ptr_write, max_read);
			if(read_len < 0) {
				err = -errno;
				fprintf("Client socket failed %d => %s\n", errno, strerror(errno));
				goto fail_socket;
			}

			if(read_len)
			// Update write poiter
			if(ptr_write + read_len < ring + RING_SIZE) {
				ptr_write += read_len;
			} else {
				ptr_write = ring;
			}

			// Parse data in ring buffer (TODO: Maybe we could do this async, too?)
			ptr_parse = ptr_read;
			// We should always be at the start of a command

			if(ptr_parse + 4 < ptr_write) { // Could be SIZE
				if(ptr_parse + 4 < ring + RING_SIZE) {
					// Easy, we can strncmp
					if(strncmp("SIZE", ptr_parse, strlen("SIZE")) && strncmp("size", ptr_parse, strlen("size"))) {
						goto check_px;
					}
				} else {
					// We need to copy (but this should be quite a rare event) :(
					memcpy(str_store, ptr_parse, ring + RING_SIZE - str_store)
				}
			}
check_px:

			while(ptr_parse != ptr_write) {
				if(str_len >= STR_SIZE) {
					fprintf("Command string too long %d => %s\n", errno, strerror(errno));
				}
				str_store[str_len++] = *ptr_parse;
				ptr_parse++;
			}
		}
	}

fail:
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

void net_shutdown(struct net* net) {
	net->state = NET_STATE_SHUTDOWN;
	close(net->socket);
	shutdown(net->socket, SHUT_RDWR);
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
