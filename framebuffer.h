#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <assert.h>
#include <stdint.h>

#include "llist.h"

#define COLORDEPTH 24

struct fb_size {
	unsigned int width;
	unsigned int height;
};

// RGBA32
union fb_pixel {
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

struct fb {
	struct fb_size size;
	union fb_pixel* pixels;
	unsigned numa_node;
	struct llist_entry list;
#ifdef FEATURE_STATISTICS
#ifdef FEATURE_PIXEL_COUNT
	uint32_t pixel_count;
#endif
#endif
};


// Management
int fb_alloc(struct fb** framebuffer, unsigned int width, unsigned int height);
void fb_free(struct fb* fb);
void fb_free_all(struct llist* fbs);
struct fb* fb_get_fb_on_node(struct llist* fbs, unsigned numa_node);

// Manipulation
void fb_set_pixel(struct fb* fb, unsigned int x, unsigned int y, union fb_pixel* pixel);
void fb_set_pixel_rgb(struct fb* fb, unsigned int x, unsigned int y, uint8_t red, uint8_t green, uint8_t blue);
void fb_clear_rect(struct fb* fb, unsigned int x, unsigned int y, unsigned int width, unsigned int height);
int fb_resize(struct fb* fb, unsigned int width, unsigned int height);
int fb_coalesce(struct fb* fb, struct llist* fbs);
void fb_copy(struct fb* dst, struct fb* src);

inline union fb_pixel fb_get_pixel(struct fb* fb, unsigned int x, unsigned int y) {
	assert(x < fb->size.width);
	assert(y < fb->size.height);

	return fb->pixels[y * fb->size.width + x];
}


// Info
inline struct fb_size* fb_get_size(struct fb* fb) {
	return &fb->size;
}

inline union fb_pixel* fb_get_line_base(struct fb* fb, unsigned int line) {
	return &fb->pixels[fb->size.width * line];
}

// Pixel fmt conversion
#define FB_GRAY8_TO_PIXEL(c) ( 0x000000ff | (c) << 24 | (c) << 16 | (c) << 8 )

#endif
