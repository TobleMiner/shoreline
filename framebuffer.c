#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "util.h"
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

	fb->pixels = calloc(width * height, sizeof(union fb_pixel));
	if(!fb->pixels) {
		err = -ENOMEM;
		goto fail_fb;
	}

	fb->numa_node = get_numa_node();
	fb->list = LLIST_ENTRY_INIT;

	*framebuffer = fb;
	return 0;

fail_fb:
	free(fb);
fail:
	return err;
}

struct fb* fb_get_fb_on_node(struct llist* fbs, unsigned numa_node) {
	struct llist_entry* cursor;
	struct fb* fb;
	llist_for_each(fbs, cursor) {
		fb = llist_entry_get_value(cursor, struct fb, list);
		if(fb->numa_node == numa_node) {
			return fb;
		}
	}
	return NULL;
}

void fb_free(struct fb* fb) {
	free(fb->pixels);
	free(fb);
}

void fb_free_all(struct llist* fbs) {
	struct llist_entry* cursor;
	llist_for_each(fbs, cursor) {
		free(llist_entry_get_value(cursor, struct fb, list));
	}
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

static void fb_set_size(struct fb* fb, unsigned int width, unsigned int height) {
	fb->size.width = width;
	fb->size.height = height;
}

int fb_resize(struct fb* fb, unsigned int width, unsigned int height) {
	int err = 0;
	union fb_pixel* fbmem, *oldmem;
	struct fb_size oldsize = *fb_get_size(fb);
	size_t memsize = width * height * sizeof(union fb_pixel);
	size_t oldmemsize = oldsize.width * oldsize.height * sizeof(union fb_pixel);
	fbmem = malloc(memsize);
	if(!fbmem) {
		err = -ENOMEM;
		goto fail;
	}
	memset(fbmem, 0, memsize);

	oldmem = fb->pixels;
	// Try to prevent oob writes
	if(oldmemsize > memsize) {
		fb_set_size(fb, width, height);
		fb->pixels = fbmem;
	} else {
		fb->pixels = fbmem;
		fb_set_size(fb, width, height);
	}
	free(oldmem);
fail:
	return err;
}

int fb_coalesce(struct fb* fb, struct llist* fbs) {
	struct llist_entry* cursor;
	struct fb* other;
	size_t i, j, fb_size = fb->size.width * fb->size.height, num_fbs = llist_length(fbs);
	unsigned int indices[num_fbs];
	for(i = 0; i < num_fbs; i++) {
		indices[i] = i;
	}
	ARRAY_SHUFFLE(indices, num_fbs);
	for(i = 0; i < num_fbs; i++) {
		cursor = llist_get_entry(fbs, indices[i]);
		other = llist_entry_get_value(cursor, struct fb, list);
		if(fb->size.width != other->size.width || fb->size.height != other->size.height) {
			return -EINVAL;
		}
		for(j = 0; j < fb_size; j++) {
			// This type of transparency handling is crap. We should do proper coalescing
			if(other->pixels[j].color.alpha == 0) {
				continue;
			}
			fb->pixels[j] = other->pixels[j];
			// Reset to fully transparent
			other->pixels[j].color.alpha = 0;
		}
	}
	return 0;
}
