#include <errno.h>
#include <stdio.h>

#include "vnc.h"
#include "framebuffer.h"

static const struct frontend_ops fops = {
	.alloc = vnc_alloc,
	.start = vnc_start,
	.free = vnc_free,
	.update = vnc_update,
	.draw_string = vnc_draw_string,
};

static const struct frontend_arg fargs[] = {
	{ .name = "port", .configure = vnc_configure_port },
	{ .name = "font", .configure = vnc_configure_font },
	{ .name = "", .configure = NULL },
};

DECLARE_FRONTEND_SIG_ARGS(front_vnc, "VNC server frontend", &fops, fargs);

/*
static rfbNewClientAction client_connect_cb(struct _rfbClientRec* client) {
	int err;
	struct fb* fb;
	struct vnc* vnc = client->screen->screenData;
	struct vnc_client* vnc_client = calloc(1, sizeof(struct vnc_client));
	if(!vnc_client) {
		err = ENOMEM;
		fprintf(stderr, "Failed to allocate vnc client: %s (%d)", strerror(err), err);
		goto fail;
	}
	
	vnc_client->vnc = vnc;
	if((err = fb_alloc(&fb, vnc->fb->size.widht, vnc->fb->size.height)) {
		fprintf(stderr, "Failed to allocate vnc client: %s (%d)", strerror(err), err);
		goto fail_client;
	}
	vnc_client->fb = fb;
	client->clientData = vnc_client;

	return RFB_CLIENT_ACCEPT;

fail_client:
	free(vnc_client);
fail:
	return RFB_CLIENT_REFUSE;
}
*/

static void pre_display_cb(struct _rfbClientRec* client) {
	struct vnc* vnc = client->screen->screenData;
	pthread_mutex_lock(&vnc->draw_lock);
	draw_overlays(vnc->fb_overlay);
}

static void post_display_cb(struct _rfbClientRec* client, int result) {
	struct vnc* vnc = client->screen->screenData;
	pthread_mutex_unlock(&vnc->draw_lock);
}

int vnc_alloc(struct frontend** ret, struct fb* fb, void* priv) {
	int err = 0;
	struct vnc* vnc = calloc(1, sizeof(struct vnc));
	struct fb_size* size;
	if(!vnc) {
		err = -ENOMEM;
		goto fail;
	}

	pthread_mutex_init(&vnc->draw_lock, NULL);
	vnc->fb = fb;
	size = fb_get_size(fb);

	if((err = fb_alloc(&vnc->fb_overlay, size->width, size->height))) {
		goto fail_vnc;
	}

	vnc->server = rfbGetScreen(NULL, NULL, size->width, size->height, 8, 3, 4);
	if(!vnc->server) {
		err = -ENOMEM;
		goto fail_fb;
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

	vnc->server->displayHook = pre_display_cb;
	vnc->server->displayFinishedHook = post_display_cb;
	vnc->server->screenData = vnc;
	vnc->server->frameBuffer = (char *)vnc->fb_overlay->pixels;
	vnc->server->desktopName = "shoreline";
	vnc->server->alwaysShared = TRUE;

	*ret = &vnc->front;

	return 0;

fail_fb:
	fb_free(vnc->fb_overlay);
fail_vnc:
	free(vnc);
fail:
	return err;
};

int vnc_start(struct frontend* front) {
	struct vnc* vnc = container_of(front, struct vnc, front);
	rfbInitServer(vnc->server);
	rfbRunEventLoop(vnc->server, -1, TRUE);
	return 0;
}

void vnc_free(struct frontend* front) {
	struct vnc* vnc = container_of(front, struct vnc, front);
	rfbShutdownServer(vnc->server, TRUE);
	rfbScreenCleanup(vnc->server);
	free(vnc);
}

int vnc_update(struct frontend* front) {
	struct vnc* vnc = container_of(front, struct vnc, front);
	pthread_mutex_lock(&vnc->draw_lock);
	fb_copy(vnc->fb_overlay, vnc->fb);
	pthread_mutex_unlock(&vnc->draw_lock);
	rfbMarkRectAsModified(vnc->server, 0, 0, vnc->fb->size.width, vnc->fb->size.height);
	return !rfbIsActive(vnc->server);
}

int vnc_draw_string(struct frontend* front, unsigned x, unsigned y, char* str) {
	int space, width;
	struct vnc* vnc = container_of(front, struct vnc, front);
	if(vnc->font) {
		space = rfbWidthOfString(vnc->font, " ");
		width = rfbWidthOfString(vnc->font, str);
		rfbFillRect(vnc->server, x, y, x + width + 2 * space, y + VNC_FONT_HEIGHT + 4, 0x00000000);
		rfbDrawString(vnc->server, vnc->font, x + space, y + VNC_FONT_HEIGHT + 2, str, 0xffffffff);
	}
	return 0;
}

int vnc_configure_port(struct frontend* front, char* value) {
	struct vnc* vnc = container_of(front, struct vnc, front);
	if(!value) {
		return -EINVAL;
	}

	int port = atoi(value);
	if(port < 0 || port > 65535) {
		return -EINVAL;
	}

	vnc->server->port = vnc->server->ipv6port = port;
	vnc->server->autoPort = FALSE;

	return 0;
}

int vnc_configure_font(struct frontend* front, char* value) {
	struct vnc* vnc = container_of(front, struct vnc, front);
	if(!value) {
		return -EINVAL;
	}

	vnc->font = rfbLoadConsoleFont(value);
	if(!vnc->font) {
		return -EINVAL;
	}

	return 0;
}
