#ifndef _TEXTRENDER_H_
#define _TEXTRENDER_H_

#include <sys/types.h>

#include <ft2build.h>
#include FT_FREETYPE_H

struct textrender {
	FT_Library ftlib;
	FT_Face ftface;
};

// Management
int textrender_alloc(struct textrender** ret, char* fontfile);
void textrender_free(struct textrender* txtrndr);

// Drawing
int textrender_draw_string(struct textrender* txtrndr, struct fb* fb, unsigned int x, unsigned int y, const char* text, unsigned int size);

#endif
