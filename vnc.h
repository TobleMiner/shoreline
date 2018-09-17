#ifndef _VNC_H_
#define _VNC_H_

#include <rfb/rfb.h>

#include "framebuffer.h"
#include "frontend.h"

struct vnc {
	rfbScreenInfoPtr server;
	rfbFontDataPtr font;
	struct fb* fb;
	struct frontend front;
};

int vnc_alloc(struct frontend** ret, struct fb* fb, void* priv);
int vnc_start(struct frontend* front);
void vnc_free(struct frontend* front);
int vnc_update(struct frontend* front);
int vnc_draw_string(struct frontend* front, unsigned x, unsigned y, char* str);
int vnc_configure_port(struct frontend* front, char* value);
int vnc_configure_font(struct frontend* front, char* value);

#endif
