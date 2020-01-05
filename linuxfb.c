#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/fb.h>

#include "linuxfb.h"

char* default_fbdev = "/dev/fb0";

int linuxfb_alloc(struct frontend** ret, struct fb* fb, void* priv) {
	int err;
	struct linuxfb* linuxfb = calloc(1, sizeof(struct linuxfb));
	if(!linuxfb) {
		fprintf(stderr, "Failed to allocate linuxfb frontend, out of memory\n");
		err = -ENOMEM;
		goto fail;
	}

	linuxfb->fb = fb;
	linuxfb->fbdev = default_fbdev;
	linuxfb->fd = -1;

	*ret = &linuxfb->front;
	return 0;

fail:
	return err;
}

static int linuxfb_start(struct frontend* front) {
	struct linuxfb* linuxfb = container_of(front, struct linuxfb, front);
	char* fbmem;
	int err;
	int fd = open(linuxfb->fbdev, O_RDWR);
	if(fd < 0) {
		fprintf(stderr, "Failed to open fbdev '%s': %s(%d)\n", linuxfb->fbdev, strerror(errno), errno);
		err = -errno;
		goto fail;
	}

	if((err = ioctl(fd, FBIOGET_VSCREENINFO, &linuxfb->vscreen) < 0)) {
		fprintf(stderr, "Failed to get var screeninfo: %s(%d)\n", strerror(errno), errno);
		err = -errno;
		goto fail_fd;
	}

	printf("vscreen offsets:\n");
	printf("  red:   %u.%u\n", linuxfb->vscreen.red.offset, linuxfb->vscreen.red.length);
	printf("  green: %u.%u\n", linuxfb->vscreen.green.offset, linuxfb->vscreen.green.length);
	printf("  blue:  %u.%u\n", linuxfb->vscreen.blue.offset, linuxfb->vscreen.blue.length);

	fbmem = calloc(2, linuxfb->fb->size.width * linuxfb->fb->size.height);
	if(!fbmem) {
		fprintf(stderr, "Failed to allocate buffer for fb target format, out of memory\n");
		err = -ENOMEM;
		goto fail_fd;
	}

	linuxfb->fd = fd;
	linuxfb->fbmem = fbmem;

	return 0;

fail_fd:
	close(fd);
fail:
	return err;
}

int linuxfb_update(struct frontend* front) {
	struct linuxfb* linuxfb = container_of(front, struct linuxfb, front);
	union fb_pixel px;
	unsigned int x, y;
	ssize_t write_len;
	size_t len;
	char* fbmem;
	// Convert fb data to fbdev format
	unsigned int px_index = 0;
	for(y = 0; y < linuxfb->fb->size.height; y++) {
		for(x = 0; x < linuxfb->fb->size.width; x++) {
			px = fb_get_pixel(linuxfb->fb, x, y);
			// 16 bit -> 565
			linuxfb->fbmem[px_index++] = (px.color.color_bgr.red >> 3) | (((px.color.color_bgr.green >> 2) & 0x07) << 5);
			linuxfb->fbmem[px_index++] = (((px.color.color_bgr.green >> 2) & 0x38) >> 3) | ((px.color.color_bgr.blue >> 3) << 1);
		}
	}

	fbmem = linuxfb->fbmem;
	len = linuxfb->fb->size.width * linuxfb->fb->size.height * 2;
	lseek(linuxfb->fd, 0, SEEK_SET);
	while(len > 0) {
		if((write_len = write(linuxfb->fd, fbmem, len)) < 0) {
			break;
		}
		len -= write_len;
		fbmem += write_len;
	}
	if(write_len < 0) {
		fprintf(stderr, "Write to fbdev failed: %s(%d)\n", strerror(errno), errno);
		return -errno;
	}

	return 0;
}

static int configure_fbdev(struct frontend* front, char* value) {
	struct linuxfb* linuxfb = container_of(front, struct linuxfb, front);
	int err;
	char* fbdev = strdup(value);
	if(!fbdev) {
		fprintf(stderr, "Failed to allocate space for fb device path, out of memory\n");
		err = -ENOMEM;
		goto fail;
	}

	linuxfb->fbdev = fbdev;
	return 0;

fail:
	return err;
}

void linuxfb_free(struct frontend* front) {
	struct linuxfb* linuxfb = container_of(front, struct linuxfb, front);
	if(linuxfb->fbdev != default_fbdev) {
		free(linuxfb->fbdev);
	}
	if(linuxfb->fd > 0) {
		close(linuxfb->fd);
	}
	free(linuxfb);
}

static const struct frontend_ops fops = {
	.alloc = linuxfb_alloc,
	.start = linuxfb_start,
	.free = linuxfb_free,
	.update = linuxfb_update,
};

static const struct frontend_arg fargs[] = {
	{ .name = "fb", .configure = configure_fbdev },
	{ .name = "", .configure = NULL },
};

DECLARE_FRONTEND_SIG_ARGS(front_linuxfb, "Linux framebuffer frontend", &fops, fargs);
