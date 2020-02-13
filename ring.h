#ifndef _RING_H_
#define _RING_H_

#include <stdbool.h>

struct ring {
	size_t size;
	char* data;
	char* ptr_read, *ptr_write;
};

int ring_alloc(struct ring** ret, size_t size);
void ring_free(struct ring* ring);


size_t ring_available(struct ring* ring);
size_t ring_available_contig(struct ring* ring);
size_t ring_free_space(struct ring* ring);
size_t ring_free_space_contig(struct ring* ring);

int ring_peek(struct ring* ring, char* data, size_t len);
int ring_read(struct ring* ring, char* data, size_t len);
int ring_write(struct ring* ring, char* data, size_t len);
void ring_advance_read(struct ring* ring, off_t offset);
void ring_advance_write(struct ring* ring, off_t offset);

// Special zero-copy optimizations
int ring_memcmp(struct ring*, char* ref, unsigned int len, char** next_pos);

// Optimized inline funnctions
static inline bool ring_any_available(struct ring* ring) {
	return ring->ptr_read != ring->ptr_write;
}

// Take a small peek into the buffer
static inline char ring_peek_one(struct ring* ring) {
	return *ring->ptr_read;
}

// Peek previous byte
static inline char ring_peek_prev(struct ring* ring) {
	if(ring->ptr_read - 1 < ring->data) {
		return *(ring->data + ring->size - 1);
	}
	return *(ring->ptr_read - 1);
}

// Pointer to next byte to read from ringbuffer
static inline char* ring_next(struct ring* ring, char* ptr) {
	if(ptr < ring->data + ring->size - 1) {
		return ptr + 1;
	}
	return ring->data;
}

// Read one byte from the buffer
static inline char ring_read_one(struct ring* ring) {
	char c = *ring->ptr_read;
	ring->ptr_read = ring_next(ring, ring->ptr_read);
	return c;
}

static inline void ring_inc_read(struct ring* ring) {
	ring->ptr_read = ring_next(ring, ring->ptr_read);
}

#endif
