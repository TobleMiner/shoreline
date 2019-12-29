#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>
#include <netdb.h>
#include <time.h>

#include "framebuffer.h"
#include "sdl.h"
#include "vnc.h"
#include "network.h"
#include "llist.h"
#include "util.h"
#include "frontend.h"
#include "workqueue.h"
#include "textrender.h"
#ifdef FEATURE_STATISTICS
#include "statistics.h"
#endif


#define PORT_DEFAULT "1234"
#define LISTEN_DEFAULT "::"
#define RATE_DEFAULT 60
#define WIDTH_DEFAULT 1024
#define HEIGHT_DEFAULT 768
#define RINGBUFFER_DEFAULT 65536
#define LISTEN_THREADS_DEFAULT 10
#define MAX_STAT_LENGTH 265

#define MAX_FRONTENDS 16

#define REPO_URL "https://github.com/TobleMiner/shoreline"

static char* default_description = REPO_URL;

static bool do_exit = false;

extern struct frontend_id frontends[];

void show_frontends() {
	fprintf(stderr, "Available frontends:\n");
	struct frontend_id* front = frontends;
	for(; front->def != NULL; front++) {
		const struct frontend_arg* options = front->def->args;
		fprintf(stderr, "\t%s: %s ", front->id, front->def->name);
		if(options) {
			fprintf(stderr, "(Options:");
			while(strlen(options->name)) {
				fprintf(stderr, " %s", options->name);
				options++;
			}
			fprintf(stderr, ")");
		}
		fprintf(stderr, "\n");
	}
}

void doshutdown(int sig)
{
	printf("Shutting down\n");
	do_exit = true;
}

void show_usage(char* binary) {
	fprintf(stderr, "Usage: %s [-p <port>] [-b <bind address>] [-w <width>] [-h <height>] [-r <screen update rate>] "\
		"[-s <ring buffer size>] [-l <number of listening threads>] [-f <frontend>] [-t <fontfile>] [-d <description>] [-?]\n", binary);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -p <port>                        Port to listen on (default %s)\n", PORT_DEFAULT);
	fprintf(stderr, "  -b <address>                     Address to listen on (default %s)\n", LISTEN_DEFAULT);
	fprintf(stderr, "  -w <width>                       Width of drawing surface (default %u)\n", WIDTH_DEFAULT);
	fprintf(stderr, "  -h <height>                      Height of drawing surface (default %u)\n", HEIGHT_DEFAULT);
	fprintf(stderr, "  -r <update rate>                 Screen update rate in HZ (default %u)\n", RATE_DEFAULT);
	fprintf(stderr, "  -s <ring size>                   Size of network ring buffer in bytes (default %u)\n", RINGBUFFER_DEFAULT);
	fprintf(stderr, "  -l <listen threads>              Number of threads used to listen for incoming connections (default %u)\n", LISTEN_THREADS_DEFAULT);
	fprintf(stderr, "  -f <frontend,[option=value,...]> Frontend to use as a display. May be specified multiple times. "\
		"Use -f ? to list available frontends and options\n");
	fprintf(stderr, "  -t <fontfile>                    Enable fancy text rendering using TTF, OTF or CFF font from <fontfile>\n");
	fprintf(stderr, "  -d <description>                 Set description text to be displayed in upper left corner (default %s)\n", REPO_URL);
	fprintf(stderr, "  -?                               Show this help\n");
}

struct resize_wq_priv {
	struct fb* fb;
	struct fb_size size;
};

int resize_wq_cb(void* priv) {
	struct resize_wq_priv* resize_priv = priv;
	return fb_resize(resize_priv->fb, resize_priv->size.width, resize_priv->size.height);
}

int resize_wq_err(int err, void* priv) {
	doshutdown(SIGTERM);
	return err;
}

void resize_wq_cleanup(int err, void* priv) {
	free(priv);
}

int resize_cb(struct sdl* sdl, unsigned int width, unsigned int height) {
	struct llist* fb_list = sdl->cb_private;
	struct llist_entry* cursor;
	struct fb* fb;
	int err = 0;

	llist_for_each(fb_list, cursor) {
		struct resize_wq_priv* resize_priv = malloc(sizeof(struct resize_wq_priv));
		if(!resize_priv) {
			err = -ENOMEM;
			goto fail;
		}

		resize_priv->size.width = width;
		resize_priv->size.height = height;
		fb = llist_entry_get_value(cursor, struct fb, list);
		resize_priv->fb = fb;

		// TODO: Add error callback
		if((err = workqueue_enqueue(fb->numa_node, resize_priv, resize_wq_cb, resize_wq_err, resize_wq_cleanup))) {
			free(resize_priv);
			goto fail;
		}
	}

fail:
	return err;
}

#ifdef FEATURE_STATISTICS
struct statistics stats = { 0 };
#endif
struct textrender* txtrndr = NULL;
char* description = REPO_URL;

void draw_overlays(struct fb* fb) {
#ifdef FEATURE_STATISTICS
	char stat_line[MAX_STAT_LENGTH];
	snprintf(stat_line, sizeof(stat_line), "Traffic: %.3f %sB / %.3f %sPixels "
"Throughput: %.3f %sb/s / %.3f %sPixels/s FPS: %d frames/s %lu connections",
		statistics_traffic_get_scaled(&stats), statistics_traffic_get_unit(&stats),
		statistics_pixels_get_scaled(&stats), statistics_pixels_get_unit(&stats),
		statistics_throughput_get_scaled(&stats), statistics_throughput_get_unit(&stats),
		statistics_pps_get_scaled(&stats), statistics_pps_get_unit(&stats),
		statistics_get_frames_per_second(&stats), stats.num_connections);
#endif
	if(txtrndr) {
		textrender_draw_string(txtrndr, fb, 100, fb->size.height / 20, description, 16);
#ifdef FEATURE_STATISTICS
		textrender_draw_string(txtrndr, fb, 100, fb->size.height - fb->size.height / 10, stat_line, 16);
#endif
	}
}

int main(int argc, char** argv) {
	int err, opt;
	struct fb* fb;
	struct llist fb_list;
	struct sockaddr_storage* inaddr;
	struct addrinfo* addr_list;
	struct net* net;
	struct llist fronts;
	struct llist_entry* cursor, *next;
	struct frontend* front;
	struct sdl_param sdl_param;
	size_t addr_len;
#ifdef FEATURE_STATISTICS
	char stat_line[MAX_STAT_LENGTH];
#endif
	unsigned int frontend_cnt = 0;
	char* frontend_names[MAX_FRONTENDS];
	bool handle_signals = true;

	char* port = PORT_DEFAULT;
	char* listen_address = LISTEN_DEFAULT;

	int width = WIDTH_DEFAULT;
	int height = HEIGHT_DEFAULT;
	int screen_update_rate = RATE_DEFAULT;

	int ringbuffer_size = RINGBUFFER_DEFAULT;
	int listen_threads = LISTEN_THREADS_DEFAULT;

	struct timespec before, after;
	long long time_delta;

	while((opt = getopt(argc, argv, "p:b:w:h:r:s:l:f:t:d:?")) != -1) {
		switch(opt) {
			case('p'):
				port = optarg;
				break;
			case('b'):
				listen_address = optarg;
				break;
			case('w'):
				width = atoi(optarg);
				if(width <= 0) {
					fprintf(stderr, "Width must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('h'):
				height = atoi(optarg);
				if(height <= 0) {
					fprintf(stderr, "Height must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('r'):
				screen_update_rate = atoi(optarg);
				if(screen_update_rate <= 0) {
					fprintf(stderr, "Screen update rate must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('s'):
				ringbuffer_size = atoi(optarg);
				if(ringbuffer_size < 2) {
					fprintf(stderr, "Ring buffer size must be >= 2\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('l'):
				listen_threads = atoi(optarg);
				if(listen_threads <= 0) {
					fprintf(stderr, "Number of listening threads must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('f'):
				if(frontend_cnt >= MAX_FRONTENDS) {
					fprintf(stderr, "Maximum number of frontends reached.\n");
					err = -EINVAL;
					goto fail;
				}
				frontend_names[frontend_cnt] = strdup(optarg);
				if(!frontend_names[frontend_cnt]) {
					fprintf(stderr, "Can not copy frontend spec. Out of memory\n");
					err = -ENOMEM;
					goto fail;
				}
				frontend_cnt++;
				break;
			case('t'):
				if((err = textrender_alloc(&txtrndr, optarg))) {
					fprintf(stderr, "Failed to allocate freetype text renderer\n");
					goto fail;
				}
				break;
			case('d'):
				description = strdup(optarg);
				if(!description) {
					fprintf(stderr, "Failed to allocate memory for description string\n");
					goto fail;
				}
				break;
			default:
				show_usage(argv[0]);
				err = -EINVAL;
				goto fail;
		}
	}

	if(frontend_cnt == 0) {
		printf("WARNING: No frontends specified, continuing without any frontends\n");
	}

	if((err = workqueue_init())) {
		fprintf(stderr, "Failed to initialize workqueues: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	if((err = fb_alloc(&fb, width, height))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	llist_init(&fb_list);
	sdl_param.cb_private = &fb_list;
	sdl_param.resize_cb = resize_cb;
	llist_init(&fronts);
	while(frontend_cnt > 0 && frontend_cnt--) {
		char* frontid = frontend_names[frontend_cnt];
		char* options = frontend_spec_extract_name(frontid);
		struct frontend_def* frontdef = frontend_get_def(frontid);
		if(!frontdef) {
			fprintf(stderr, "Unknown frontend '%s'\n", frontid);
			show_frontends();
			goto fail_fronts_free_name;
		}
		handle_signals = handle_signals && !frontdef->handles_signals;
		if((err = frontend_alloc(frontdef, &front, fb, &sdl_param))) {
			fprintf(stderr, "Failed to allocate frontend '%s'\n", frontdef->name);
			goto fail_fronts_free_name;
		}
		front->def = frontdef;
		llist_append(&fronts, &front->list);

		if(frontend_can_configure(front) && options) {
			if((err = frontend_configure(front, options))) {
				fprintf(stderr, "Failed to configure frontend '%s'\n", frontdef->name);
				goto fail_fronts_free_name;
			}
		}

		if(frontend_can_start(front)) {
			if((err = frontend_start(front))) {
				fprintf(stderr, "Failed to start frontend '%s'\n", frontdef->name);
				goto fail_fronts_free_name;
			}
		}

		free(frontid);
	}

	if((err = net_alloc(&net, fb, &fb_list, &fb->size, ringbuffer_size))) {
		fprintf(stderr, "Failed to initialize network: %d => %s\n", err, strerror(-err));
		goto fail_fronts;
	}
	if(handle_signals) {
		if(signal(SIGINT, doshutdown)) {
			fprintf(stderr, "Failed to bind signal\n");
			err = -EINVAL;
			goto fail_net;
		}
		if(signal(SIGPIPE, SIG_IGN)) {
			fprintf(stderr, "Failed to bind signal\n");
			err = -EINVAL;
			goto fail_net;
		}
	}

	if((err = -getaddrinfo(listen_address, port, NULL, &addr_list))) {
		fprintf(stderr, "Failed to resolve listen address '%s', %d => %s\n", listen_address, err, gai_strerror(-err));
		goto fail_net;
	}

	inaddr = (struct sockaddr_storage*)addr_list->ai_addr;
	addr_len = addr_list->ai_addrlen;

	if((err = net_listen(net, listen_threads, inaddr, addr_len))) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", err, strerror(-err));
		goto fail_addrinfo;
	}

	nice(-20);
	while(!do_exit) {
		clock_gettime(CLOCK_MONOTONIC, &before);
		llist_lock(&fb_list);
		fb_coalesce(fb, &fb_list);
		llist_unlock(&fb_list);
#ifdef FEATURE_STATISTICS
		statistics_update(&stats, net);
		snprintf(stat_line, sizeof(stat_line), "Traffic: %.3f %sB / %.3f %sPixels "
"Throughput: %.3f %sb/s / %.3f %sPixels/s FPS: %d frames/s %lu connections",
			statistics_traffic_get_scaled(&stats), statistics_traffic_get_unit(&stats),
			statistics_pixels_get_scaled(&stats), statistics_pixels_get_unit(&stats),
			statistics_throughput_get_scaled(&stats), statistics_throughput_get_unit(&stats),
			statistics_pps_get_scaled(&stats), statistics_pps_get_unit(&stats),
			statistics_get_frames_per_second(&stats), stats.num_connections);
#endif
		draw_overlays(fb);
		if(txtrndr) {
			textrender_draw_string(txtrndr, fb, 100, fb->size.height / 20, description, 16);
#ifdef FEATURE_STATISTICS
			textrender_draw_string(txtrndr, fb, 100, fb->size.height - fb->size.height / 10, stat_line, 16);
#endif
		}
		llist_for_each(&fronts, cursor) {
			front = llist_entry_get_value(cursor, struct frontend, list);
			if(!txtrndr) {
				if(frontend_can_draw_string(front)) {
					frontend_draw_string(front, 0, 0, description);
#ifdef FEATURE_STATISTICS
					frontend_draw_string(front, 0, fb->size.height - fb->size.height / 10, stat_line);
#endif
				}
			}
			if((err = frontend_update(front))) {
				fprintf(stderr, "Failed to update frontend '%s', %d => %s, bailing out\n", front->def->name, err, strerror(-err));
				doshutdown(SIGINT);
				break;
			}
		}
#ifdef FEATURE_STATISTICS
		stats.num_frames++;
#endif
		clock_gettime(CLOCK_MONOTONIC, &after);
		time_delta = get_timespec_diff(&after, &before);
		time_delta = 1000000000UL / screen_update_rate - time_delta;
		if(time_delta > 0) {
			usleep(time_delta / 1000UL);
		}
	}
	net_shutdown(net);

	fb_free_all(&fb_list);
fail_addrinfo:
	freeaddrinfo(addr_list);
fail_net:
	net_free(net);
fail_fronts:
	llist_for_each_safe(&fronts, cursor, next) {
		front = llist_entry_get_value(cursor, struct frontend, list);
		printf("Shutting down frontend '%s'\n", front->def->name);
		frontend_free(front);
	}
//fail_fb:
	fb_free(fb);
fail:
	while(frontend_cnt > 0 && frontend_cnt--) {
		free(frontend_names[frontend_cnt]);
	}
	if(txtrndr) {
		textrender_free(txtrndr);
	}
	if(description && description != default_description) {
		free(description);
	}
	workqueue_deinit();
	return err;

fail_fronts_free_name:
	frontend_cnt++;
	goto fail_fronts;
}
