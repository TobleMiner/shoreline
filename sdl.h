#ifndef _SDL_H_
#define _SDL_H_

#include <SDL.h>

#include "framebuffer.h"
#include "frontend.h"

struct sdl;

typedef int (*sdl_cb_resize)(struct sdl* sdl, unsigned int width, unsigned int height);

struct sdl {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	struct fb* fb;

	sdl_cb_resize resize_cb;
	void* cb_private;
	struct frontend front;
};


struct sdl_param {
	void* cb_private;
	sdl_cb_resize resize_cb;
};

int sdl_alloc(struct frontend** ret, struct fb* fb, void* c);
void sdl_free(struct frontend* front);
int sdl_update(struct frontend* front);

#endif
