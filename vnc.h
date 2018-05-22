#ifndef _VNC_H_
#define _VNC_H_

#include <rfb/rfb.h>

#include "framebuffer.h"

struct vnc {
	rfbScreenInfoPtr server;
	struct fb* fb;
};

int vnc_alloc(struct vnc** ret, struct fb* fb);
void vnc_free(struct vnc* vnc);

#endif
