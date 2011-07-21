/**
 * SH display. This file implements helper functions to access the framebuffer.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <linux/fb.h>
#include <uiomux/uiomux.h>

#include "display.h"

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif

#define min(a, b) ((a) < (b) ? (a) : (b))

#define HW_ALIGN 2
#define RGB_BPP 2

struct DISPLAY {
	int fb_handle;
	struct fb_fix_screeninfo fb_fix;
	struct fb_var_screeninfo fb_var;
	unsigned long fb_base;
	unsigned long back_buf_phys;
	unsigned char *iomem;
	int fb_size;
	int fb_index;
	int lcd_w;
	int lcd_h;
};


DISPLAY *display_open(void)
{
	const char *device;
	DISPLAY *disp;
	int size;

	disp = calloc(1, sizeof(*disp));
	if (!disp)
		return NULL;

	/* Initialize display */
	device = getenv("FRAMEBUFFER");
	if (!device) {
		if (access("/dev/.devfsd", F_OK) == 0) {
			device = "/dev/fb/0";
		} else {
			device = "/dev/fb0";
		}
	}

	if ((disp->fb_handle = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Open %s: %s.\n", device, strerror(errno));
		free(disp);
		return 0;
	}
	if (ioctl(disp->fb_handle, FBIOGET_FSCREENINFO, &disp->fb_fix) < 0) {
		fprintf(stderr, "Ioctl FBIOGET_FSCREENINFO error.\n");
		free(disp);
		return 0;
	}
	if (ioctl(disp->fb_handle, FBIOGET_VSCREENINFO, &disp->fb_var) < 0) {
		fprintf(stderr, "Ioctl FBIOGET_VSCREENINFO error.\n");
		free(disp);
		return 0;
	}
	if (disp->fb_fix.type != FB_TYPE_PACKED_PIXELS) {
		fprintf(stderr, "Frame buffer isn't packed pixel.\n");
		free(disp);
		return 0;
	}

	/* clear framebuffer and back buffer */
	disp->fb_size = (RGB_BPP * disp->fb_var.xres * disp->fb_var.yres * disp->fb_var.bits_per_pixel) / 8;
	disp->iomem = mmap(0, disp->fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, disp->fb_handle, 0);
	if (disp->iomem != MAP_FAILED) {
		memset(disp->iomem, 0, disp->fb_size);
	}

	/* Register the framebuffer with UIOMux */
	uiomux_register (disp->iomem, disp->fb_fix.smem_start, disp->fb_size);

	disp->lcd_w = disp->fb_var.xres;
	disp->lcd_h = disp->fb_var.yres;

	disp->back_buf_phys = disp->fb_base = disp->fb_fix.smem_start;
	disp->fb_index = 0;
	display_flip(disp);

	return disp;
}

void display_close(DISPLAY *disp)
{
	disp->fb_var.xoffset = 0;
	disp->fb_var.yoffset = 0;

	uiomux_unregister(disp->iomem);
	munmap(disp->iomem, disp->fb_size);

	/* Restore the framebuffer to the front buffer */
	ioctl(disp->fb_handle, FBIOPAN_DISPLAY, &disp->fb_var);

	close(disp->fb_handle);
	free(disp);
}

int display_get_width(DISPLAY *disp)
{
	return disp->lcd_w;
}

int display_get_height(DISPLAY *disp)
{
	return disp->lcd_h;
}

unsigned char *display_get_back_buff_virt(DISPLAY *disp)
{
	int frame_offset = RGB_BPP * (1-disp->fb_index) * disp->lcd_w * disp->lcd_h;
	return (disp->iomem + frame_offset);
}

unsigned long display_get_back_buff_phys(DISPLAY *disp)
{
	return disp->back_buf_phys;
}

int display_flip(DISPLAY *disp)
{
	struct fb_var_screeninfo fb_screen = disp->fb_var;
	unsigned long crt = 0;

	fb_screen.xoffset = 0;
	fb_screen.yoffset = 0;
	if (disp->fb_index==0)
		fb_screen.yoffset = disp->fb_var.yres;
	if (-1 == ioctl(disp->fb_handle, FBIOPAN_DISPLAY, &fb_screen))
		return 0;

	/* Point to the back buffer */
	disp->back_buf_phys = disp->fb_base;
	if (disp->fb_index!=0)
		disp->back_buf_phys += disp->fb_fix.line_length * disp->fb_var.yres;

	disp->fb_index = (disp->fb_index+1) & 1;

	/* wait for vsync interrupt */
	ioctl(disp->fb_handle, FBIO_WAITFORVSYNC, &crt);

	return 1;
}

