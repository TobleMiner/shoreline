#ifndef _RING_H_
#define _RING_H_

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
char* ring_next(struct ring* ring, char* ptr);

int ring_read(struct ring* ring, char* data, size_t len);
int ring_write(struct ring* ring, char* data, size_t len);

// Special zero-copy optimizations
int ring_strncmp(struct ring*, char* ref, unsigned int len, char** next_pos);

#endif
