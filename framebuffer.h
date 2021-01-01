#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <assert.h>
#include <stdint.h>

#include "llist.h"
#include "util.h"

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
	struct {
		union {
			struct {
				uint8_t red;
				uint8_t green;
				uint8_t blue;
			} color_bgr;
			unsigned char bgr[3];
		};
		uint8_t alpha;
	} color_be;
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

static inline union fb_pixel fb_get_pixel(struct fb* fb, unsigned int x, unsigned int y) {
	assert(x < fb->size.width);
	assert(y < fb->size.height);

	return fb->pixels[y * fb->size.width + x];
}

#define FB_ALPHA_BLEND_PIXEL(newpx, oldpx) do { \
	if (is_big_endian()) { \
		newpx.color_be.color_bgr.blue = ((uint16_t)newpx.color_be.color_bgr.blue * newpx.color_be.alpha + (uint16_t)oldpx.color_be.color_bgr.blue * (0xff - newpx.color_be.alpha)) / 255; \
		newpx.color_be.color_bgr.green = ((uint16_t)newpx.color_be.color_bgr.green * newpx.color_be.alpha + (uint16_t)oldpx.color_be.color_bgr.green * (0xff - newpx.color_be.alpha)) / 255; \
		newpx.color_be.color_bgr.red = ((uint16_t)newpx.color_be.color_bgr.red * newpx.color_be.alpha + (uint16_t)oldpx.color_be.color_bgr.red * (0xff - newpx.color_be.alpha)) / 255; \
		newpx.color_be.alpha = 0xff; \
	} else { \
		newpx.color.color_bgr.blue = ((uint16_t)newpx.color.color_bgr.blue * newpx.color.alpha + (uint16_t)oldpx.color.color_bgr.blue * (0xff - newpx.color.alpha)) / 255; \
		newpx.color.color_bgr.green = ((uint16_t)newpx.color.color_bgr.green * newpx.color.alpha + (uint16_t)oldpx.color.color_bgr.green * (0xff - newpx.color.alpha)) / 255; \
		newpx.color.color_bgr.red = ((uint16_t)newpx.color.color_bgr.red * newpx.color.alpha + (uint16_t)oldpx.color.color_bgr.red * (0xff - newpx.color.alpha)) / 255; \
		newpx.color.alpha = 0xff; \
	} \
} while (0)

// Info
static inline struct fb_size* fb_get_size(struct fb* fb) {
	return &fb->size;
}

static inline union fb_pixel* fb_get_line_base(struct fb* fb, unsigned int line) {
	return &fb->pixels[fb->size.width * line];
}

// Pixel fmt conversion
#define FB_GRAY8_TO_PIXEL(c) ( 0x000000ff | (c) << 24 | (c) << 16 | (c) << 8 )

#endif
