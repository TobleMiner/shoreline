#ifndef _VNC_H_
#define _VNC_H_

#include <rfb/rfb.h>

#include "framebuffer.h"
#include "frontend.h"

#define VNC_FONT_HEIGHT 16

struct vnc {
	rfbScreenInfoPtr server;
	rfbFontDataPtr font;
	struct fb* fb;
	struct fb* fb_overlay;
	struct frontend front;
	pthread_mutex_t draw_lock;
};

/*
struct vnc_client {
	struct vnc* vnc;
	struct fb* fb;
};
*/

int vnc_alloc(struct frontend** ret, struct fb* fb, void* priv);
int vnc_start(struct frontend* front);
void vnc_free(struct frontend* front);
int vnc_update(struct frontend* front);
int vnc_draw_string(struct frontend* front, unsigned x, unsigned y, char* str);
int vnc_configure_port(struct frontend* front, char* value);
int vnc_configure_font(struct frontend* front, char* value);

#endif
