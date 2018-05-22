#include <errno.h>

#include "vnc.h"
#include "framebuffer.h"

#define SL_PXFMT SDL_PIXELFORMAT_RGBA8888

int vnc_alloc(struct vnc** ret, struct fb* fb) {
	int err = 0;
	struct vnc* vnc = malloc(sizeof(struct vnc));
	struct fb_size* size;
	if(!vnc) {
		err = -ENOMEM;
		goto fail;
	}

	vnc->fb = fb;
	size = fb_get_size(fb);

	vnc->server = rfbGetScreen(NULL, NULL, size->width, size->height, 8, 3, 4);
	if(!vnc->server) {
			err = -ENOMEM;
		goto fail_vnc;
	}

	vnc->server->bitsPerPixel = 32;
	vnc->server->depth = 24;
	rfbPixelFormat* format = &vnc->server->serverFormat;
	format->depth = 24;
	format->redMax = 0xFF;
	format->greenMax = 0xFF;
	format->blueMax = 0xFF;
	format->redShift = 24;
	format->greenShift = 16;
	format->blueShift = 8;

	vnc->server->frameBuffer = (char *)fb->pixels;
	vnc->server->alwaysShared = TRUE;

	rfbInitServer(vnc->server);
	rfbRunEventLoop(vnc->server, -1, TRUE);

	*ret = vnc;

	return 0;

fail_vnc:
	free(vnc);
fail:
	return err;
};

void vnc_free(struct vnc* vnc) {
	rfbShutdownServer(vnc->server, TRUE);
	rfbScreenCleanup(vnc->server);
	free(vnc);
}
