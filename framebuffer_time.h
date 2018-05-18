#ifndef _FRAMEBUFFER_TIME_H_
#define _FRAMEBUFFER_TIME_H_

#include <stdint.h>

#include "framebuffer.h"
#include "llist.h"

#define COLORDEPTH 24

struct fbt_pixel {
	union {
		struct {
			uint8_t alpha;
			union {
				struct {
					uint8_t blue;
					uint8_t green;
					uint8_t red;
				} color_bgr;
				unsigned char bgr[3];
			};
		} color;
		uint32_t abgr;
	};
	uint32_t timestamp;
};

struct fbt {
	struct fb_size size;
	struct fbt_pixel* pixels;
	unsigned numa_node;
	struct llist_entry list;
};


// Management
int fbt_alloc(struct fbt** framebuffer, unsigned int width, unsigned int height);
void fbt_free(struct fbt* fb);
void fbt_free_all(struct llist* fbs);
struct fbt* fbt_get_fb_on_node(struct llist* fbs, unsigned numa_node);

// Manipulation
int fbt_resize(struct fbt* fb, unsigned int width, unsigned int height);

inline void fbt_set_pixel(struct fbt* fb, unsigned int x, unsigned int y, struct fbt_pixel pixel) {
	fb->pixels[y * fb->size.width + x] = pixel;
}


// Info
inline struct fb_size* fbt_get_size(struct fbt* fb) {
	return &fb->size;
}

#endif
