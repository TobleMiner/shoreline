#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "statistics.h"
#include "llist.h"
#include "main.h"

#define STATISTICS_API_LISTEN_PORT_DEFAULT "1235"
#define STATISTICS_API_LISTEN_ADDRESS_DEFAULT "::"

static const char* UNITS[] = {
	"",
	"k",
	"M",
	"G",
	"T",
	"P"
};

void statistics_update(struct statistics* stats, struct net* net) {
	int i = net->num_threads;
	struct timespec now;
	uint64_t bytes_prev = stats->num_bytes;
	uint64_t num_connections = 0;
#ifdef FEATURE_PIXEL_COUNT
	uint64_t pixels_prev = stats->num_pixels;
	struct llist_entry* cursor;
#endif

	clock_gettime(CLOCK_MONOTONIC, &now);
	while(i-- > 0) {
		struct net_thread* thread = &net->threads[i];
		struct llist* threadlist = thread->threadlist;
		struct llist_entry* cursor;

		if(thread->initialized) {
			llist_lock(threadlist);
			llist_for_each(threadlist, cursor) {
				struct net_connection_thread* conn_thread = llist_entry_get_value(cursor, struct net_connection_thread, list);
				stats->num_bytes += conn_thread->byte_count;
				conn_thread->byte_count = 0;
				num_connections++;
			}
			llist_unlock(threadlist);
		}
	}
	stats->num_connections = num_connections;
	stats->bytes_per_second[stats->average_index] = (stats->num_bytes - bytes_prev) * 1000000000UL / get_timespec_diff(&now, &stats->last_update);

#ifdef FEATURE_PIXEL_COUNT
	llist_lock(net->fb_list);
	llist_for_each(net->fb_list, cursor) {
		struct fb* fb = llist_entry_get_value(cursor, struct fb, list);
		stats->num_pixels += fb->pixel_count;
		fb->pixel_count = 0;
	}
	llist_unlock(net->fb_list);
	stats->pixels_per_second[stats->average_index] = (stats->num_pixels - pixels_prev) * 1000000000UL / get_timespec_diff(&now, &stats->last_update);
#else
	// Do a crude estimation if actual pixel count is not available. Average Pixel is about |PX XXX YYY rrggbb\n| = 18 bytes
	stats->num_pixels += (stats->num_bytes - bytes_prev) / 18;
	stats->pixels_per_second[stats->average_index] = stats->bytes_per_second[stats->average_index] / 18;
#endif
	stats->frames_per_second[stats->average_index] = (stats->num_frames - stats->last_num_frames) * 1000000000UL / get_timespec_diff(&now, &stats->last_update);

	stats->average_index++;
	stats->average_index %= STATISTICS_NUM_AVERAGES;
	stats->last_update = now;
	stats->last_num_frames = stats->num_frames;
}

#define GET_AVERAGE(var, stats, field) \
	do { \
		int i_; \
		(var) = 0; \
		for(i_ = 0; i_ < STATISTICS_NUM_AVERAGES; i_++) { \
			(var) += ((stats)->field)[i_]; \
		} \
		(var) /= STATISTICS_NUM_AVERAGES; \
	} while(0)

#define DEFINE_UNIT(base) \
	static const char* value_get_unit_##base(uint64_t value) { \
		int i = 0; \
		while(value > base) { \
			if(i == ARRAY_LEN(UNITS) - 1) { \
				break; \
			} \
			i++; \
			value /= base; \
		} \
		return UNITS[i]; \
	} \
	static double value_get_scaled_##base(uint64_t value) { \
		double val = value; \
		int i = 0; \
		while(val > base) { \
			i++; \
			if(i >= ARRAY_LEN(UNITS)) { \
				break; \
			} \
			val /= base; \
		} \
		return val; \
	}

DEFINE_UNIT(1024);
DEFINE_UNIT(1000);

const char* statistics_traffic_get_unit(struct statistics* stats) {
	return value_get_unit_1024(stats->num_bytes);
}

double statistics_traffic_get_scaled(struct statistics* stats) {
	return value_get_scaled_1024(stats->num_bytes);
}

const char* statistics_throughput_get_unit(struct statistics* stats) {
	uint64_t bytes_per_second;
	GET_AVERAGE(bytes_per_second, stats, bytes_per_second);
	return value_get_unit_1000(bytes_per_second * 8ULL);
}

double statistics_throughput_get_scaled(struct statistics* stats) {
	uint64_t bytes_per_second;
	GET_AVERAGE(bytes_per_second, stats, bytes_per_second);
	return value_get_scaled_1000(bytes_per_second * 8ULL);
}

const char* statistics_pixels_get_unit(struct statistics* stats) {
	return value_get_unit_1000(stats->num_pixels);
}

double statistics_pixels_get_scaled(struct statistics* stats) {
	return value_get_scaled_1000(stats->num_pixels);
}

const char* statistics_pps_get_unit(struct statistics* stats) {
	uint64_t pixels_per_second;
	GET_AVERAGE(pixels_per_second, stats, pixels_per_second);
	return value_get_unit_1000(pixels_per_second);
}

double statistics_pps_get_scaled(struct statistics* stats) {
	uint64_t pixels_per_second;
	GET_AVERAGE(pixels_per_second, stats, pixels_per_second);
	return value_get_scaled_1000(pixels_per_second);
}

int statistics_get_frames_per_second(struct statistics* stats) {
	unsigned int fps;
	GET_AVERAGE(fps, stats, frames_per_second);
	return fps;
}


static int statistics_frontend_alloc(struct frontend** ret, struct fb* fb, void* priv) {
	struct statistics_frontend* sfront = calloc(1, sizeof(struct statistics_frontend));
	if(!sfront) {
		return -ENOMEM;
	}
	sfront->socket = -1;
	sfront->listen_port = STATISTICS_API_LISTEN_PORT_DEFAULT;
	sfront->listen_address = STATISTICS_API_LISTEN_ADDRESS_DEFAULT;
	pthread_mutex_init(&sfront->stats_lock, NULL);
	*ret = &sfront->front;
	return 0;
}

#define API_STRBUF_LEN 1024

static void* api_thread(void* args) {
	struct statistics_frontend* sfront = args;
	char strbuf[API_STRBUF_LEN];
	uint64_t bytes_per_second;
	uint64_t pixels_per_second;
	uint64_t frames_per_second;

	while(!sfront->exit) {
		size_t len;
		ssize_t write_len;
		char* buf = strbuf;
		int sock = accept(sfront->socket, NULL, NULL);
		if(sock < 0) {
			fprintf(stderr, "Listener failed, bailing out: %d => %s\n", errno, strerror(errno));
			break;
		}
		pthread_mutex_lock(&sfront->stats_lock);
		GET_AVERAGE(bytes_per_second, &sfront->stats, bytes_per_second);
		GET_AVERAGE(pixels_per_second, &sfront->stats, pixels_per_second);
		GET_AVERAGE(frames_per_second, &sfront->stats, frames_per_second);

		len = snprintf(strbuf, sizeof(strbuf), "{ \"traffic\": { \"bytes\": %lu, \"pixels\": %lu }, \"throughput\": { \"bytes\": %lu, \"pixels\": %lu }, \"connections\": %lu, \"fps\": %lu }\n",
			sfront->stats.num_bytes, sfront->stats.num_pixels,
			bytes_per_second, pixels_per_second,
			sfront->stats.num_connections,
			frames_per_second);
		pthread_mutex_unlock(&sfront->stats_lock);
		while(len > 0) {
			write_len = write(sock, buf, len);
			if(write_len < 0) {
				fprintf(stderr, "Failed to write to API client: %d => %s\n", errno, strerror(errno));
				break;
			}
			len -= write_len;
			buf += write_len;
		}
		shutdown(sock, SHUT_RDWR);
		close(sock);
	}
	return NULL;
}

static int statistics_frontend_start(struct frontend* front) {
	struct statistics_frontend* sfront = container_of(front, struct statistics_frontend, front);
	int err;
	int sock;
	int one = 1;
	struct addrinfo* addr_list;
	size_t addr_len;
	struct sockaddr_storage* listen_addr;

	if((err = -getaddrinfo(sfront->listen_address, sfront->listen_port, NULL, &addr_list))) {
		goto fail;
	}
	sfront->addr_list = addr_list;
	listen_addr = (struct sockaddr_storage*)addr_list->ai_addr;
	addr_len = addr_list->ai_addrlen;

	if((sock = socket(listen_addr->ss_family, SOCK_STREAM, 0)) < 0) {
		err = -errno;
		goto fail_addrlist;
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

	if(bind(sock, (struct sockaddr*)listen_addr, addr_len) < 0) {
		fprintf(stderr, "Failed to bind to %s:%s %d => %s\n", sfront->listen_address, sfront->listen_port, errno, strerror(errno));
		err = -errno;
		goto fail_socket;
	}

	if(listen(sock, 10)) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", errno, strerror(errno));
		err = -errno;
		goto fail_socket;
	}
	sfront->socket = sock;

	if((err = -pthread_create(&sfront->listen_thread, NULL, api_thread, sfront))) {
		goto fail_socket;
	}
	sfront->thread_created = true;

	return 0;

fail_socket:
	close(sock);
	sfront->socket = -1;
fail_addrlist:
	freeaddrinfo(addr_list);
	sfront->addr_list = NULL;
fail:
	return err;
}

static void statistics_frontend_free(struct frontend* front) {
	struct statistics_frontend* sfront = container_of(front, struct statistics_frontend, front);
	sfront->exit = true;
	if(sfront->thread_created) {
		pthread_cancel(sfront->listen_thread);
		pthread_join(sfront->listen_thread, NULL);
	}
	if(sfront->socket >= 0) {
		close(sfront->socket);
	}
	if(sfront->addr_list) {
		freeaddrinfo(sfront->addr_list);
	}
	free(sfront);
}

static int statistics_frontend_update_statistics(struct frontend* front) {
	struct statistics_frontend* sfront = container_of(front, struct statistics_frontend, front);
	pthread_mutex_lock(&sfront->stats_lock);
	sfront->stats = stats;
	pthread_mutex_unlock(&sfront->stats_lock);
	return 0;
}

static int statistics_frontend_configure_port(struct frontend* front, char* value) {
	struct statistics_frontend* sfront = container_of(front, struct statistics_frontend, front);
	if(!value) {
		return -EINVAL;
	}

	int port = atoi(value);
	if(port < 0 || port > 65535) {
		return -EINVAL;
	}

	sfront->listen_port = value;

	return 0;
}

static int statistics_frontend_configure_listen_address(struct frontend* front, char* value) {
	struct statistics_frontend* sfront = container_of(front, struct statistics_frontend, front);
	if(!value) {
		return -EINVAL;
	}

	sfront->listen_address = value;
	return 0;
}

static const struct frontend_ops fops = {
	.alloc = statistics_frontend_alloc,
	.start = statistics_frontend_start,
	.free = statistics_frontend_free,
	.update = statistics_frontend_update_statistics,
};

static const struct frontend_arg fargs[] = {
	{ .name = "port", .configure = statistics_frontend_configure_port },
	{ .name = "listen", .configure = statistics_frontend_configure_listen_address },
	{ .name = "", .configure = NULL },
};

DECLARE_FRONTEND_SIG_ARGS(front_statistics, "Statistics API provider frontend", &fops, fargs);
