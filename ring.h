#ifndef _RING_H_
#define _RING_H_

struct ring {
	size_t size;
	char* data;
	char* ptr_read, ptr_write;
}

#endif
