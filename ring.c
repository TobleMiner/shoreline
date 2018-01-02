#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

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


// Number of bytes that can be read from ringbuffer
size_t ring_available(struct ring* ring) {
	if(ring->ptr_write >= ring->ptr_read) {
		return ring->ptr_write - ring->ptr_read;
	}
	return ring->size - (ring->ptr_read - ring->ptr_write);
}

// Number of virtually contiguous bytes that can be read from ringbuffer
size_t ring_available_contig(struct ring* ring) {
	if(ring->ptr_write >= ring->ptr_read) {
		return ring->ptr_write - ring->ptr_read;
	}
	return ring->size - (ring->ptr_read - ring->data);
}

// Number of free bytes
size_t ring_free_space(struct ring* ring) {
	if(ring->ptr_read > ring->ptr_write) {
		return ring->ptr_read - ring->ptr_write - 1;
	}
	return ring->size - (ring->ptr_write - ring->ptr_read) - 1;
}

// Number of contiguous free bytes after ring->write_ptr
size_t ring_free_space_contig(struct ring* ring) {
	if(ring->ptr_read > ring->ptr_write) {
		return ring->ptr_read - ring->ptr_write - 1;
	}
	return ring->size - (ring->ptr_write - ring->data);
}


// Pointer to next byte to read from ringbuffer
char* ring_next(struct ring* ring, char* ptr) {
	if(ptr < ring->data + ring->size - 1) {
		return ptr + 1;
	}
	return ring->data;
}

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

void ring_advance_read(struct ring* ring, off_t offset) {
	assert(offset >= 0);
	assert(offset <= ring->size);

	if(offset) {
		if(ring->ptr_read + offset < ring->data + ring->size) {
			ring->ptr_read = ring_next(ring, ring->ptr_read + offset - 1);
		} else {
			ring->ptr_read = offset - ring->size + ring->ptr_read;
		}
	}
}

void ring_advance_write(struct ring* ring, off_t offset) {
	assert(offset >= 0);
	assert(offset <= ring->size);

	if(offset) {
		if(ring->ptr_write + offset < ring->data + ring->size) {
			ring->ptr_write = ring_next(ring, ring->ptr_write + offset - 1);
		} else {
			ring->ptr_write = offset - ring->size + ring->ptr_write;
		}
	}
}

/*
 Behaves totally different from memcmp!
 Return:
	< 0 error (not enough data in buffer)
	= 0 match
	> 0 no match
*/
int ring_memcmp(struct ring* ring, char* ref, unsigned int len, char** next_pos) {
	size_t avail_contig;

	if(ring_available(ring) < len) {
		return -EINVAL;
	}

	avail_contig = ring_available_contig(ring);

	if(avail_contig >= len) {
		// We are lucky
		if(memcmp(ring->ptr_read, ref, len)) {
			return 1;
		}
		if(next_pos) {
			*next_pos = ring_next(ring, ring->ptr_read + len - 1);
		} else {
			ring_advance_read(ring, len);
		}
		return 0;
	}


	// We (may) need to perform two memcmps
	if(memcmp(ring->ptr_read, ref, avail_contig) || memcmp(ring->data, ref + avail_contig, len - avail_contig)) {
		return 1;
	}
	if(next_pos) {
		*next_pos = ring->data + len - avail_contig;
	} else {
		ring_advance_read(ring, len);
	}
	return 0;
}
