#include <errno.h>

#include "vnc.h"
#include "framebuffer.h"

static const struct frontend_ops fops = {
	.alloc = vnc_alloc,
	.free = vnc_free,
	.update = vnc_update,
};

DECLARE_FRONTEND_SIG(front_vnc, "VNC server frontend", fops);

int vnc_alloc(struct frontend** ret, struct fb* fb, void* priv) {
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

	*ret = &vnc->front;

	return 0;

fail_vnc:
	free(vnc);
fail:
	return err;
};

void vnc_free(struct frontend* front) {
	struct vnc* vnc = container_of(front, struct vnc, front);
	rfbShutdownServer(vnc->server, TRUE);
	rfbScreenCleanup(vnc->server);
	free(vnc);
}

int vnc_update(struct frontend* front) {
	struct vnc* vnc = container_of(front, struct vnc, front);
	rfbMarkRectAsModified(vnc->server, 0, 0, vnc->fb->size.width, vnc->fb->size.height);
	return !rfbIsActive(vnc->server);
}
