#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "framebuffer.h"
#include "textrender.h"
#include "util.h"

#define DEFAULT_FONT_SIZE 16
#define CACHE_CHUNK_SIZE 8

#define TEXT_BORDER 3

#define DPI 100

#define PIXEL_TO_CARTESIAN(x) (64 * (x))
#define CARTESIAN_TO_PIXELS(x) ((x) / 64)

int textrender_alloc(struct textrender** ret, char* fontfile) {
	int err = 0;
	FT_Error fterr;
	struct textrender* txtrndr;

	txtrndr = calloc(1, sizeof(struct textrender));
	if(!txtrndr) {
		err = -ENOMEM;
		goto fail;
	}

	fterr = FT_Init_FreeType(&txtrndr->ftlib);
	if(fterr) {
		err = fterr;
		fprintf(stderr, "Failed to initialize free type library: %s(%d)\n", FT_Error_String(fterr), err);
		goto fail_alloc;
	}

	fterr = FT_New_Face(txtrndr->ftlib, fontfile, 0, &txtrndr->ftface);
	if(fterr) {
		err = fterr;
		fprintf(stderr, "Failed to load font face \"%s\": %s(%d)\n", fontfile, FT_Error_String(fterr), err);
		goto fail_ft;
	}

	*ret = txtrndr;
	return 0;

	fail_ft:
		FT_Done_FreeType(txtrndr->ftlib);
	fail_alloc:
		free(txtrndr);
	fail:
		return err;
}

void textrender_free(struct textrender* txtrndr) {
	FT_Done_Face(txtrndr->ftface);
	FT_Done_FreeType(txtrndr->ftlib);
	free(txtrndr);
}

static int draw_bitmap(struct fb* fb, FT_Bitmap* ftbmp, unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	size_t i, j;

	y -= height;

	width = ftbmp->width;
	height = ftbmp->rows;

	if(ftbmp->pixel_mode != FT_PIXEL_MODE_GRAY) {
		return -EINVAL;
	}

	for(i = 0; i < height; i++) {
		union fb_pixel* line_base = fb_get_line_base(fb, y + i);
		union fb_pixel* line_start = &line_base[x];
		unsigned char* gray_base = &ftbmp->buffer[ftbmp->width * i];
		for(j = 0; j < width; j++) {
			line_start[j].abgr = FB_GRAY8_TO_PIXEL(gray_base[j]);
		}
	}
	return 0;
}

int textrender_draw_string(struct textrender* txtrndr, struct fb* fb, unsigned int x, unsigned int y, const char* text, unsigned int size) {
	int err = 0, i;
	FT_Error fterr;
	FT_GlyphSlot ftslot;
	FT_Vector ftpen;

	fterr = FT_Set_Char_Size(txtrndr->ftface, PIXEL_TO_CARTESIAN(size), 0, DPI, DPI);
	if(fterr) {
		err = fterr;
		fprintf(stderr, "Failed to set font size to %u: %s(%d)\n", size, FT_Error_String(fterr), err);
		goto fail;
	}

	ftslot = txtrndr->ftface->glyph;

	ftpen.x = ftpen.y = 0;

	for(i = 0; i < strlen(text); i++) {
		fterr = FT_Load_Char(txtrndr->ftface, text[i], FT_LOAD_RENDER);
		if(fterr) {
			err = fterr;
			fprintf(stderr, "Warning: Failed to find glyph for char '%c': %s(%d)\n", text[i], FT_Error_String(fterr), err);
			continue;
		}

		draw_bitmap(fb, &ftslot->bitmap, x + CARTESIAN_TO_PIXELS(ftpen.x), y + CARTESIAN_TO_PIXELS(ftpen.y), ftslot->bitmap_left, ftslot->bitmap_top);

		ftpen.x += ftslot->advance.x;
		ftpen.y += ftslot->advance.y;
	}
fail:
	return err;
}
