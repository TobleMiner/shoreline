#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <stdint.h>

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
};


// Management
int fb_alloc(struct fb** framebuffer, unsigned int width, unsigned int height);
void fb_free(struct fb* fb);

// Manipulation
void fb_set_pixel(struct fb* fb, unsigned int x, unsigned int y, union fb_pixel* pixel);
void fb_set_pixel_rgb(struct fb* fb, unsigned int x, unsigned int y, uint8_t red, uint8_t green, uint8_t blue);
union fb_pixel fb_get_pixel(struct fb* fb, unsigned int x, unsigned int y);

// Info
struct fb_size fb_get_size(struct fb* fb);

#endif
