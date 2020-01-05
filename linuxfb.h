#ifndef _LINUXFB_H_
#define _LINUXFB_H_

#include "framebuffer.h"
#include "frontend.h"

struct linuxfb {
	struct frontend front;
	struct fb* fb;
	char* fbdev;
	int fd;
	char* fbmem;
	struct fb_var_screeninfo vscreen;
};

#endif
