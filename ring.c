#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
