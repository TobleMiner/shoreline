#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "util.h"
#include "framebuffer_time.h"

int fbt_alloc(struct fbt** framebuffer, unsigned int width, unsigned int height) {
	int err = 0;

	struct fbt* fb = malloc(sizeof(struct fb));
	if(!fb) {
		err = -ENOMEM;
		goto fail;
	}

	fb->size.width = width;
	fb->size.height = height;

	fb->pixels = calloc(width * height, sizeof(struct fbt_pixel));
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

struct fbt* fbt_get_fb_on_node(struct llist* fbs, unsigned numa_node) {
	struct llist_entry* cursor;
	struct fbt* fb;
	llist_for_each(fbs, cursor) {
		fb = llist_entry_get_value(cursor, struct fbt, list);
		if(fb->numa_node == numa_node) {
			return fb;
		}
	}
	return NULL;
}

void fbt_free(struct fbt* fb) {
	free(fb->pixels);
	free(fb);
}

void fbt_free_all(struct llist* fbs) {
	struct llist_entry* cursor;
	llist_for_each(fbs, cursor) {
		free(llist_entry_get_value(cursor, struct fbt, list));
	}
}

static void fbt_set_size(struct fbt* fb, unsigned int width, unsigned int height) {
	fb->size.width = width;
	fb->size.height = height;
}

int fbt_resize(struct fbt* fb, unsigned int width, unsigned int height) {
	int err = 0;
	struct fbt_pixel* fbmem, *oldmem;
	struct fb_size oldsize = *fbt_get_size(fb);
	size_t memsize = width * height * sizeof(struct fbt_pixel);
	size_t oldmemsize = oldsize.width * oldsize.height * sizeof(struct fbt_pixel);
	fbmem = malloc(memsize);
	if(!fbmem) {
		err = -ENOMEM;
		goto fail;
	}
	memset(fbmem, 0, memsize);

	oldmem = fb->pixels;
	// Try to prevent oob writes
	if(oldmemsize > memsize) {
		fbt_set_size(fb, width, height);
		fb->pixels = fbmem;
	} else {
		fb->pixels = fbmem;
		fbt_set_size(fb, width, height);
	}
	free(oldmem);
fail:
	return err;
}
