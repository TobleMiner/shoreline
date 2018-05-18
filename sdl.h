#ifndef _SDL_H_
#define _SDL_H_

#include <SDL.h>

#include "framebuffer.h"

struct sdl {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	struct fb* fb;
	void* cb_private;
};

typedef int (*sdl_cb_resize)(struct sdl* sdl, unsigned int width, unsigned int height);


int sdl_alloc(struct sdl** ret, struct fb* fb, void* cb_private);
void sdl_free(struct sdl* sdl);

int sdl_update(struct sdl* sdl, sdl_cb_resize resize_cb);


#endif
