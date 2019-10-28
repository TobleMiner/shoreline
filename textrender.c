#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "framebuffer.h"
#include "textrender.h"

#define DEFAULT_FONT_SIZE 16
#define CACHE_CHUNK_SIZE 8

static int cache_add_entry(struct textrender* txtrndr, TTF_Font* font, unsigned int size) {
  // Cache is full, extend
  if(txtrndr->num_cached_fonts >= txtrndr->font_cache_size) {
    size_t new_size = txtrndr->font_cache_size + CACHE_CHUNK_SIZE;
    struct textrender_font* new_buf = realloc(txtrndr->font_cache, new_size * sizeof(struct textrender_font));
    if(!new_buf) {
      return -ENOMEM;
    }
    txtrndr->font_cache_size = new_size;
  }
  struct textrender_font* cache_entry = txtrndr->font_cache[txtrndr->num_cached_fonts];
  cache_entry->font = font;
  cache_entry->size = size;
  txtrndr->num_cached_fonts++;
  return 0;
}

static int textrender_get_font(TTF_Font** ret, struct textrender* txtrndr, unsigned int size) {
  int err;
  size_t i;
  TTF_Font* font;

  // Perform lookup in cache
  for(i = 0; i < txtrndr->num_cached_fonts; i++) {
    if(txtrndr->font_cache[i].size == size) {
      *ret = txtrndr->font_cache[i].font;
      return 0;
    }
  }

  // Fontsize not found, load it
  font = TTF_OpenFont(txtrndr->fontname, size);
  if(!font) {
    fprintf(stderr, "Failed to load font \"%s\", size %upt: %s\n", txtrndr->fontname, size, TTF_GetError());
    err = -1;
    goto fail;
  }

  // Store it in cache
  err = cache_add_entry(txtrndr, font, size);
  if(err) {
    fprintf(stderr, "Failed to store font \"%s\", size %upt: %s\n in cache, %s(%d)", txtrndr->fontname, size, strerror(-err), err);
    goto fail_font_alloc;
  }

fail_font_alloc:
  TTF_CloseFont(font);
fail:
  return err;
}

int textrender_alloc(struct textrender** ret, char* ttffont) {
  int err = 0;
  struct textrender* txtrndr;
  TTF_Font* font;

  if(!TTF_WasInit()) {
    err = TTF_Init();
    if(err != 0) {
      fprintf(stderr, "Failed to initialize SDL TTF renderer: %s\n", TTF_GetError());
      goto fail;
    }
  }

  txtrndr = calloc(1, sizeof(struct textrender));
  if(!txtrndr) {
    err = -ENOMEM;
    goto fail;
  }

  txtrndr->fontname = strdup(ttffont);
  if(!txtrndr->fontname) {
    err = -ENOMEM;
    goto fail_alloc;
  }

  err = textrender_get_font(&font, txtrndr, DEFAULT_FONT_SIZE);
  if(err) {
    fprintf(stderr, "Failed to load font %s at default size %u\n", ttffont, DEFAULT_FONT_SIZE);
    goto fail_namedup;
  }

  *ret = txtrndr;
  return 0;

  fail_namedup:
    free(txtrndr);
  fail_alloc:
    free(txtrndr);
  fail:
    return err;
}

void textrender_free(struct textrender* txtrndr) {
  while(txtrndr->num_cached_fonts--) {
    TTF_CloseFont(txtrndr->font_cache[i].font);
  }
  free(txtrndr->fontname);
  free(txtrndr->font_cache);
  free(txtrndr);
}

int textrender_draw_string(struct textrender* txtrndr, struct fb* fb, unsigned int x, unsigned int y, char* string, usinged int size) {

}
