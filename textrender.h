#ifndef _TEXTRENDER_H_
#define _TEXTRENDER_H_

#include <sys/types.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct textrender_font {
  TTF_Font* font;
  unsigned int size;
};

struct textrender {
  char* fontname;
  struct textrender_font* font_cache;
  size_t font_cache_size;
  size_t num_cached_fonts;
};

#endif
