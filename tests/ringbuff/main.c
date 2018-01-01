#include <stdio.h>
#include <stdlib.h>

#include "../../ring.h"

int main(int argc, char** argv) {
	int err = 0;

	struct ring* ring;

	if((err = ring_alloc(&ring))) {
		fprintf(stderr, "Failed to allocate ring buffer\n");
		goto fail;
	}

	

	return 0;

fail:
	return err;
}
