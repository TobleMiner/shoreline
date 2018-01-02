#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <SDL.h>

#include "sdl.h"
#include "framebuffer.h"

int sdl_alloc(struct sdl** ret, struct fb* fb) {
	int err = 0;
	struct sdl* sdl = malloc(sizeof(struct sdl));
	struct fb_size size;
	if(!sdl) {
		err = -ENOMEM;
		goto fail;
	}

	sdl->fb = fb;
	size = fb_get_size(fb);

	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

	if(SDL_Init(SDL_INIT_VIDEO)) {
		err = -1;
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		goto fail_sdl;
	}

	SDL_ShowCursor(0);

	sdl->window = SDL_CreateWindow("shoreline",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size.width,
		size.height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if(!sdl->window) {
		err = -1;
		fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
		goto fail_sdl_init;
	}

	sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED
		| SDL_RENDERER_PRESENTVSYNC);

	if(!sdl->renderer) {
		err = -1;
		fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
		goto fail_sdl_window;
	}
	SDL_RenderClear(sdl->renderer);

	sdl->texture = SDL_CreateTexture(sdl->renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, size.width, size.height);

	if(!sdl->texture) {
		err = -1;
		fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());
		goto fail_sdl_renderer;
	}

	*ret = sdl;

	return 0;

fail_sdl_renderer:
	SDL_DestroyRenderer(sdl->renderer);
fail_sdl_window:
	SDL_DestroyWindow(sdl->window);
fail_sdl_init:
	SDL_Quit();
fail_sdl:
	free(sdl);
fail:
	return err;
};

void sdl_free(struct sdl* sdl) {
	SDL_DestroyTexture(sdl->texture);
	SDL_DestroyRenderer(sdl->renderer);
	SDL_DestroyWindow(sdl->window);
	SDL_Quit();
	free(sdl);
}


void sdl_update(struct sdl* sdl) {
	struct fb_size size = fb_get_size(sdl->fb);
	SDL_UpdateTexture(sdl->texture, NULL, sdl->fb->pixels, size.width * sizeof(union fb_pixel));
	SDL_RenderCopy(sdl->renderer, sdl->texture, NULL, NULL);
	SDL_RenderPresent(sdl->renderer);
}
