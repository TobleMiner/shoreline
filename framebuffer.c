#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "framebuffer.h"

int fb_alloc(struct fb** framebuffer, unsigned int width, unsigned int height) {
	int err = 0;

	struct fb* fb = malloc(sizeof(struct fb));
	if(!fb) {
		err = -ENOMEM;
		goto fail;
	}

	fb->size.width = width;
	fb->size.height = height;

	fb->pixels = malloc(width * height * sizeof(union fb_pixel));
	if(!fb->pixels) {
		err = -ENOMEM;
		goto fail_fb;
	}

	*framebuffer = fb;
	return 0;

fail_fb:
	free(fb);
fail:
	return err;
}

void fb_free(struct fb* fb) {
	free(fb->pixels);
	free(fb);
}



void fb_set_pixel(struct fb* fb, unsigned int x, unsigned int y, union fb_pixel* pixel) {
	union fb_pixel* target;
	assert(x < fb->size.width);
	assert(y < fb->size.height);

	target = &(fb->pixels[y * fb->size.width + x]);
	memcpy(target, pixel, sizeof(*pixel));
}

void fb_set_pixel_rgb(struct fb* fb, unsigned int x, unsigned int y, uint8_t red, uint8_t green, uint8_t blue) {
	union fb_pixel* target;
	assert(x < fb->size.width);
	assert(y < fb->size.height);

	target = &(fb->pixels[y * fb->size.width + x]);
	target->color.color_bgr.red = red;
	target->color.color_bgr.green = green;
	target->color.color_bgr.blue = blue;
}

// It might be a good idea to offer a variant returning a pointer to avoid unnecessary copies
union fb_pixel fb_get_pixel(struct fb* fb, unsigned int x, unsigned int y) {
	assert(x < fb->size.width);
	assert(y < fb->size.height);

	return fb->pixels[y * fb->size.width + x];
}



struct fb_size fb_get_size(struct fb* fb) {
	return fb->size;
}
