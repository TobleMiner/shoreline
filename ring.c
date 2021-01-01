#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "ring.h"

int ring_alloc(struct ring** ret, size_t size) {
	int err;

	struct ring* ring = malloc(sizeof(struct ring));
	if(!ring) {
		err = -ENOMEM;
		goto fail;
	}

	ring->data = malloc(size);
	if(!ring->data) {
		err = -ENOMEM;
		goto fail_ring;
	}
	ring->ptr_read = ring->data;
	ring->ptr_write = ring->data;
	ring->size = size;

	*ret = ring;

	return 0;

fail_ring:
	free(ring);
fail:
	return err;
}

void ring_free(struct ring* ring) {
	free(ring->data);
	free(ring);
}


// Take a peek into the ring buffer
int ring_peek(struct ring* ring, char* data, size_t len) {
	size_t avail_contig;

	if(ring_available(ring) < len) {
		return -EINVAL;
	}

	avail_contig = ring_available_contig(ring);

	if(avail_contig >= len) {
		memcpy(data, ring->ptr_read, len);
	} else {
		memcpy(data, ring->ptr_read, avail_contig);
		memcpy(data + avail_contig, ring->data, len - avail_contig);
	}

	return 0;
}

// Read from this ring buffer
int ring_read(struct ring* ring, char* data, size_t len) {
	size_t avail_contig;

	if(ring_available(ring) < len) {
		return -EINVAL;
	}

	avail_contig = ring_available_contig(ring);

	if(avail_contig >= len) {
		memcpy(data, ring->ptr_read, len);
		ring->ptr_read = ring_next(ring, ring->ptr_read + len - 1);
	} else {
		memcpy(data, ring->ptr_read, avail_contig);
		memcpy(data + avail_contig, ring->data, len - avail_contig);
		ring->ptr_read = ring->data + len - avail_contig;
	}

	return 0;
}

// Write to this ring buffer
int ring_write(struct ring* ring, char* data, size_t len) {
	size_t free_contig;

	if(ring_free_space(ring) < len) {
		return -EINVAL;
	}

	free_contig = ring_free_space_contig(ring);

	if(free_contig >= len) {
		memcpy(ring->ptr_write, data, len);
		ring->ptr_write = ring_next(ring, ring->ptr_write + len - 1);
	} else {
		memcpy(ring->ptr_write, data, free_contig);
		memcpy(ring->data, data + free_contig, len - free_contig);
		ring->ptr_write = ring->data + len - free_contig;
	}
	return 0;
}
