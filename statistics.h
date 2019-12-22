#ifdef FEATURE_STATISTICS
#ifndef _STATISTICS_H_
#define _STATISTICS_H_

#include <stdint.h>

#include "network.h"

#define STATISTICS_NUM_AVERAGES 20

struct statistics {
	uint64_t num_bytes;
	uint64_t num_pixels;
	int average_index;
	uint64_t bytes_per_second[STATISTICS_NUM_AVERAGES];
	uint64_t pixels_per_second[STATISTICS_NUM_AVERAGES];
	struct timespec last_update;
};

void statistics_update(struct statistics* stats, struct net* net);

const char* statistics_traffic_get_unit(struct statistics* stats);
double statistics_traffic_get_scaled(struct statistics* stats);
const char* statistics_throughput_get_unit(struct statistics* stats);
double statistics_throughput_get_scaled(struct statistics* stats);

const char* statistics_pixels_get_unit(struct statistics* stats);
double statistics_pixels_get_scaled(struct statistics* stats);
const char* statistics_pps_get_unit(struct statistics* stats);
double statistics_pps_get_scaled(struct statistics* stats);

#endif
#endif
