#ifndef _TEXTRENDER_H_
#define _TEXTRENDER_H_

#include <pthread.h>
#include <sys/types.h>

#include <ft2build.h>
#include FT_FREETYPE_H

// FT_Error_String is not supported by older libfreetype2 versions
#ifndef FT_Error_String
#define FT_Error_String(_) ("FT_Error_String not supported")
#endif

struct textrender {
	pthread_mutex_t font_lock;
	FT_Library ftlib;
	FT_Face ftface;
};

// Management
int textrender_alloc(struct textrender** ret, char* fontfile);
void textrender_free(struct textrender* txtrndr);

// Drawing
int textrender_draw_string(struct textrender* txtrndr, struct fb* fb, unsigned int x, unsigned int y, const char* text, unsigned int size);

#endif
