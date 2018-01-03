#ifndef _SHORELINE_UTIL_H_
#define _SHORELINE_UTIL_H_

#define container_of(ptr, type, member) ({ \
	const typeof(((type *)0)->member) * __mptr = (ptr); \
	(type *)((char *)__mptr - ((size_t) &((type *)0)->member)); })

#endif
