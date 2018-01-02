#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "../../ring.h"

#define RING_SIZE 256

void show_ring_stats(struct ring* ring) {
	printf("Free bytes: %zu\n", ring_free_space(ring));
	printf("Free (contiguous) bytes: %zu\n", ring_free_space_contig(ring));
	printf("Available bytes: %zu\n", ring_available(ring));
	printf("Available (contiguous) bytes: %zu\n", ring_available_contig(ring));
}

int main(int argc, char** argv) {
	int err = 0, i, j;
	long seed;
	struct timeval time;
	char strbuff[RING_SIZE + 1];
	strbuff[RING_SIZE] = 0;

	gettimeofday(&time, NULL);
	seed = time.tv_sec * 1000000L + time.tv_usec;

	printf("Using seed %ld\n", seed);
	srand(seed);

	struct ring* ring;

	if((err = ring_alloc(&ring, RING_SIZE))) {
		fprintf(stderr, "Failed to allocate ring buffer\n");
		goto fail;
	}

	printf("Ring buffer created\n");
	printf("Data: %p\n", ring->data);
	printf("Read ptr: %p\n", ring->ptr_read);
	printf("Write ptr: %p\n", ring->ptr_write);

/*	show_ring_stats(ring);

	// ===========
	//     16
	// ===========
	printf("Writing 16 byte string (including \\x00) to ring buffer\n");

	if((err = ring_write(ring, "___Hello World!", 16))) {
		fprintf(stderr, "Write failed. %d => %s\n", err, strerror(err));
		show_ring_stats(ring);
		goto fail_ring;
	}

	show_ring_stats(ring);

	memset(strbuff, '?', RING_SIZE);

	printf("Reading back 16 byte string from ring buffer\n");

	if((err = ring_read(ring, strbuff, 16))) {
		fprintf(stderr, "Read failed. %d => %s\n", err, strerror(err));
		show_ring_stats(ring);
		goto fail_ring;
	}

	printf("Length of string is %lu\n", strlen(strbuff));
	printf("String is %s\n", strbuff);

	show_ring_stats(ring);

	// ===========
	//     32
	// ===========
	printf("Writing 32 byte string (including \\x00) to ring buffer\n");

	if((err = ring_write(ring, "!dlroW olleH______Hello World!", 32))) {
		fprintf(stderr, "Write failed. %d => %s\n", err, strerror(err));
		show_ring_stats(ring);
		goto fail_ring;
	}

	show_ring_stats(ring);

	memset(strbuff, '?', RING_SIZE);

	printf("Reading back 32 byte string from ring buffer\n");

	if((err = ring_read(ring, strbuff, 32))) {
		fprintf(stderr, "Read failed. %d => %s\n", err, strerror(err));
		show_ring_stats(ring);
		goto fail_ring;
	}

	printf("Length of string is %lu\n", strlen(strbuff));
	printf("String is %s\n", strbuff);

	show_ring_stats(ring);

	// ===========
	//     256
	// ===========
	printf("Writing 256 byte string (including \\x00) to ring buffer\n");

	if((err = ring_write(ring, "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijkmnopqrstuvwxyzABCDEFGHIJKLMNOPQRTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijkmnopqrstuvwxyzABCDEFGHIJKLMNOPQRTUVWXYZ1234567890", 255))) {
		fprintf(stderr, "Write failed. %d => %s\n", err, strerror(err));
		show_ring_stats(ring);
		goto fail_ring;
	}

	show_ring_stats(ring);

	memset(strbuff, '?', RING_SIZE);

	printf("Reading back 256 byte string from ring buffer\n");

	if((err = ring_read(ring, strbuff, 255))) {
		fprintf(stderr, "Read failed. %d => %s\n", err, strerror(err));
		show_ring_stats(ring);
		goto fail_ring;
	}

	printf("Length of string is %lu\n", strlen(strbuff));
	printf("String is %s\n", strbuff);

	show_ring_stats(ring);
*/
	char refbuff[RING_SIZE];
	for(i = 0; i < 1000; i++) {
		size_t len = rand() % RING_SIZE;
		for(j = 0; j < len; j++) {
			refbuff[j] = rand() % 256;
		}
		if((err = ring_write(ring, refbuff, len))) {
			fprintf(stderr, "Write failed, len %zu, %s\n", len, strerror(-err));
			goto fail_ring;
		}
		if((err = ring_read(ring, strbuff, len))) {
			fprintf(stderr, "Read failed, len %zu, %s\n", len, strerror(-err));
			goto fail_ring;
		}

		if(ring_available(ring)) {
			fprintf(stderr, "There should be no more bytes available but there were (len %zu)\n", len);
		}

		if(strncmp(refbuff, strbuff, len)) {
			fprintf(stderr, "Cmp failed, len %zu\n", len);
			goto fail_ring;
		}
		printf("Round %d passed with length %zu\n", i, len);
	}

	printf("All tests passed!\n");

fail_ring:
	ring_free(ring);

fail:
	return err;
}

