#ifndef _VNC_H_
#define _VNC_H_

#include <rfb/rfb.h>

#include "framebuffer.h"
#include "frontend.h"

struct vnc {
	rfbScreenInfoPtr server;
	struct fb* fb;
	struct frontend front;
};

int vnc_alloc(struct frontend** ret, struct fb* fb, void* priv);
void vnc_free(struct frontend* front);
int vnc_update(struct frontend* front);

#endif
