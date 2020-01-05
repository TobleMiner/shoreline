#ifndef _NUMA_H_
#define _NUMA_H_

#include <stdbool.h>

#ifdef FEATURE_NUMA
#include <numa.h>
#else
#define numa_set_preferred(x) ((void)x)
#define numa_available() ((int)false)
#define numa_max_node() 1;
#endif

#endif
