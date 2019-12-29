#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <SDL.h>

#include "sdl.h"
#include "framebuffer.h"
#include "frontend.h"
#include "util.h"

static const struct frontend_ops fops = {
	.alloc = sdl_alloc,
	.free = sdl_free,
	.update = sdl_update,
};

DECLARE_FRONTEND_NOSIG(front_sdl, "SDL2 Frontend", &fops);

int sdl_alloc(struct frontend** ret, struct fb* fb, void* priv) {
	int err = 0;
	struct sdl_param* params = priv;
	struct sdl* sdl = calloc(1, sizeof(struct sdl));
	struct fb_size* size;
	if(!sdl) {
		err = -ENOMEM;
		goto fail;
	}

	sdl->fb = fb;
	size = fb_get_size(fb);

	sdl->cb_private = params->cb_private;
	sdl->resize_cb = params->resize_cb;

	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

	if(SDL_Init(SDL_INIT_VIDEO)) {
		err = -1;
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		goto fail_sdl;
	}

	SDL_ShowCursor(0);

	sdl->window = SDL_CreateWindow("shoreline",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size->width,
		size->height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

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

	sdl->texture = SDL_CreateTexture(sdl->renderer, SDL_PXFMT,
		SDL_TEXTUREACCESS_STREAMING, size->width, size->height);

	if(!sdl->texture) {
		err = -1;
		fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());
		goto fail_sdl_renderer;
	}

	*ret = &sdl->front;

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

void sdl_free(struct frontend* front) {
	struct sdl* sdl = container_of(front, struct sdl, front);
	SDL_DestroyTexture(sdl->texture);
	SDL_DestroyRenderer(sdl->renderer);
	SDL_DestroyWindow(sdl->window);
	SDL_Quit();
	free(sdl);
}


int sdl_update(struct frontend* front) {
	struct sdl* sdl = container_of(front, struct sdl, front);
	struct fb_size* size = fb_get_size(sdl->fb);

	int width, height, err;
	SDL_Window* window;
	SDL_Texture* texture;
	SDL_Event event;

	while(SDL_PollEvent(&event)) {
		if(event.type == SDL_WINDOWEVENT) {
			if(event.window.event == SDL_WINDOWEVENT_RESIZED) {
				window = SDL_GetWindowFromID(event.window.windowID);
				assert(window == sdl->window);

				SDL_GetWindowSize(window, &width, &height);
				assert(width >= 0);
				assert(height >= 0);
				printf("Resizing to %dx%d px\n", width, height);

				if(sdl->resize_cb) {
					if((err = sdl->resize_cb(sdl, width, height))) {
						return err;
					}
				}

				texture = SDL_CreateTexture(sdl->renderer, SDL_PXFMT,
					SDL_TEXTUREACCESS_STREAMING, width, height);
				if(!texture) {
					fprintf(stderr, "Failed to allocate new texture\n");
					continue;
				}
				SDL_DestroyTexture(sdl->texture);
				sdl->texture = texture;
				fb_resize(sdl->fb, width, height);
			}
		} else if(event.type == SDL_QUIT) {
			return 1;
		}
	}

	SDL_UpdateTexture(sdl->texture, NULL, sdl->fb->pixels, size->width * sizeof(union fb_pixel));
	SDL_RenderCopy(sdl->renderer, sdl->texture, NULL, NULL);
	SDL_RenderPresent(sdl->renderer);

	return 0;
}
