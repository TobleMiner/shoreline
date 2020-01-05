#ifndef _STATISTICS_H_
#define _STATISTICS_H_

#include <stdbool.h>
#include <stdint.h>

struct statistics;

#include "network.h"

#define STATISTICS_NUM_AVERAGES 20

struct statistics {
	uint64_t num_bytes;
	uint64_t num_pixels;
	uint64_t num_connections;
	int average_index;
	uint64_t bytes_per_second[STATISTICS_NUM_AVERAGES];
	uint64_t pixels_per_second[STATISTICS_NUM_AVERAGES];
	uint64_t frames_per_second[STATISTICS_NUM_AVERAGES];
	uint64_t last_num_frames;
	uint64_t num_frames;
	struct timespec last_update;
};

#include "frontend.h"

struct statistics_frontend {
	struct frontend front;
	int socket;
	char* listen_port;
	char* listen_address;
	struct sockaddr_storage listen_addr;
	bool thread_created;
	pthread_t listen_thread;
	pthread_mutex_t stats_lock;
	struct statistics stats;
	struct addrinfo* addr_list;
	bool exit;
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

int statistics_get_frames_per_second(struct statistics* stats);
#endif
