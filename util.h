#ifndef _SHORELINE_UTIL_H_
#define _SHORELINE_UTIL_H_

#include <unistd.h>
#include <sys/syscall.h>

#define container_of(ptr, type, member) ({ \
	const typeof(((type *)0)->member) * __mptr = (ptr); \
	(type *)((char *)__mptr - ((size_t) &((type *)0)->member)); })

inline unsigned get_numa_node() {
	unsigned numa_node;
	if(syscall(SYS_getcpu, NULL, &numa_node, NULL) == -1) {
		numa_node = 0;
	}
	return numa_node;
}

#endif
