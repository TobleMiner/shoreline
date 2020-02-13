#ifndef _SHORELINE_UTIL_H_
#define _SHORELINE_UTIL_H_

#include <endian.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define container_of(ptr, type, member) ({ \
	const typeof(((type *)0)->member) * __mptr = (ptr); \
	(type *)((char *)__mptr - ((size_t) &((type *)0)->member)); })

#define ARRAY_SHUFFLE(arr, len) \
	{ \
		typeof((len)) i, j; \
		typeof(*(arr)) tmp; \
		for(i = 0; i + 1 < (len); i++) { \
			j = i + rand() / (RAND_MAX / ((len) - i) + 1); \
			tmp = (arr)[j]; \
			(arr)[j] = (arr)[i]; \
			(arr)[i] = tmp; \
		} \
	}

#define ARRAY_LEN(arr) (sizeof((arr)) / sizeof(*(arr)))

static inline unsigned get_numa_node() {
	unsigned numa_node;
	if(syscall(SYS_getcpu, NULL, &numa_node, NULL) == -1) {
		numa_node = 0;
	}
	return numa_node;
}

static inline long long int get_timespec_diff(struct timespec* a, struct timespec* b) {
	return (a->tv_sec - b->tv_sec) * 1000000000LL + (a->tv_nsec - b->tv_nsec);
}

static inline bool is_big_endian() {
	return htobe32(0x11223344) == 0x11223344;
}

#endif
