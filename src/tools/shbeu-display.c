/*
 * Tool to overlay an image onto the display
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_NCURSES_SUBDIR
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif
#include <uiomux/uiomux.h>
#include "shbeu/shbeu.h"
#include "display.h"

/* RGB565 colors */
#define BLACK 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F


typedef struct {
	char * filename;
	FILE * file;
	size_t nread;
	size_t size;
	unsigned char *virt;
	beu_surface_t surface;
} surface_t;



static void
usage (const char * progname)
{
	printf ("Usage: %s [options] [input-filename]\n", progname);
	printf ("Overlays raw image data using the SH-Mobile BEU and displays on screen.\n");
	printf ("\n");
	printf ("If no input filename is specified, a simple image will be created.\n");
	printf ("\nInput options\n");
	printf ("  -c, --input-colorspace (RGB565, NV12, YCbCr420, YCbCr422)\n");
	printf ("                         Specify input colorspace\n");
	printf ("  -s, --input-size       Set the input image size (qcif, cif, qvga, vga, d1, 720p)\n");
	printf ("\nControl keys\n");
	printf ("  Cursor keys            Pan\n");
	printf ("  =                      Reset panning\n");
	printf ("  q                      Quit\n");
	printf ("\nMiscellaneous options\n");
	printf ("  -h, --help             Display this help and exit\n");
	printf ("  -v, --version          Output version information and exit\n");
	printf ("\nFile extensions are interpreted as follows unless otherwise specified:\n");
	printf ("  .yuv    YCbCr420\n");
	printf ("  .rgb    RGB565\n");
	printf ("\n");
	printf ("Please report bugs to <linux-sh@vger.kernel.org>\n");
}

void
print_short_options (char * optstring)
{
	char *c;

	for (c=optstring; *c; c++) {
		if (*c != ':') printf ("-%c ", *c);
	}

	printf ("\n");
}

#ifdef HAVE_GETOPT_LONG
void
print_options (struct option long_options[], char * optstring)
{
	int i;
	for (i=0; long_options[i].name != NULL; i++)  {
		printf ("--%s ", long_options[i].name);
	}

	print_short_options (optstring);
}
#endif

static int set_size (char * arg, int * w, int * h)
{
	if (arg) {
		if (!strncasecmp (arg, "qcif", 4)) {
			*w = 176;
			*h = 144;
		} else if (!strncasecmp (arg, "cif", 3)) {
			*w = 352;
			*h = 288;
		} else if (!strncasecmp (arg, "qvga", 4)) {
			*w = 320;
			*h = 240;
		} else if (!strncasecmp (arg, "vga", 3)) {
			*w = 640;
			*h = 480;
                } else if (!strncasecmp (arg, "d1", 2)) {
                        *w = 720;
                        *h = 480;
		} else if (!strncasecmp (arg, "720p", 4)) {
			*w = 1280;
			*h = 720;
		} else {
			return -1;
		}

		return 0;
	}

	return -1;
}

static const char * show_size (int w, int h)
{
	if (w == -1 && h == -1) {
		return "<Unknown size>";
	} else if (w == 176 && h == 144) {
		return "QCIF";
	} else if (w == 352 && h == 288) {
		return "CIF";
	} else if (w == 320 && h == 240) {
		return "QVGA";
	} else if (w == 640 && h == 480) {
		return "VGA";
	} else if (w == 720 && h == 480) {
		return "D1";
	} else if (w == 1280 && h == 720) {
		return "720p";
	}

	return "";
}

static int set_colorspace (char * arg, int * c)
{
	if (arg) {
		if (!strncasecmp (arg, "rgb565", 6) ||
		    !strncasecmp (arg, "rgb", 3)) {
			*c = V4L2_PIX_FMT_RGB565;
		} else if (!strncasecmp (arg, "YCbCr420", 8) ||
			   !strncasecmp (arg, "420", 3) ||
			   !strncasecmp (arg, "NV12", 4)) {
			*c = V4L2_PIX_FMT_NV12;
		} else if (!strncasecmp (arg, "YCbCr422", 8) ||
			   !strncasecmp (arg, "422", 3)) {
			*c = V4L2_PIX_FMT_NV16;
		} else {
			return -1;
		}

		return 0;
	}

	return -1;
}

static char * show_colorspace (int c)
{
	switch (c) {
	case V4L2_PIX_FMT_RGB565:
		return "RGB565";
	case V4L2_PIX_FMT_NV12:
		return "YCbCr420";
	case V4L2_PIX_FMT_NV16:
		return "YCbCr422";
	}

	return "<Unknown colorspace>";
}

static off_t filesize (char * filename)
{
	struct stat statbuf;

	if (filename == NULL || !strcmp (filename, "-"))
		return -1;

	if (stat (filename, &statbuf) == -1) {
		perror (filename);
		return -1;
	}

	return statbuf.st_size;
}

static off_t imgsize (int colorspace, int w, int h)
{
	int n=0, d=1;

	switch (colorspace) {
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_NV16:
		/* 2 bytes per pixel */
		n=2; d=1;
	       	break;
       case V4L2_PIX_FMT_NV12:
		/* 3/2 bytes per pixel */
		n=3; d=2;
		break;
       default:
		return -1;
	}

	return (off_t)(w*h*n/d);
}

static int guess_colorspace (char * filename, int * c)
{
	char * ext;
	off_t size;

	if (filename == NULL || !strcmp (filename, "-"))
		return -1;

	/* If the colorspace is already set (eg. explicitly by user args)
	 * then don't try to guess */
	if (*c != -1) return -1;

	ext = strrchr (filename, '.');
	if (ext == NULL) return -1;

	if (!strncasecmp (ext, ".yuv", 4)) {
		*c = V4L2_PIX_FMT_NV12;
		return 0;
	} else if (!strncasecmp (ext, ".rgb", 4)) {
		*c = V4L2_PIX_FMT_RGB565;
		return 0;
	}

	return -1;
}

static int guess_size (char * filename, int colorspace, int * w, int * h)
{
	off_t size;
	int n=0, d=1;

	/* If the size is already set (eg. explicitly by user args)
	 * then don't try to guess */
	if (*w != -1 && *h != -1) return -1;

	if ((size = filesize (filename)) == -1) {
		return -1;
	}

	switch (colorspace) {
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_NV16:
		/* 2 bytes per pixel */
		n=2; d=1;
	       	break;
       case V4L2_PIX_FMT_NV12:
		/* 3/2 bytes per pixel */
		n=3; d=2;
		break;
       default:
		return -1;
	}

	if (*w==-1 && *h==-1) {
		/* Image size unspecified */
		if (size == 176*144*n/d) {
			*w = 176; *h = 144;
		} else if (size == 352*288*n/d) {
			*w = 352; *h = 288;
		} else if (size == 320*240*n/d) {
			*w = 320; *h = 240;
		} else if (size == 640*480*n/d) {
			*w = 640; *h = 480;
		} else if (size == 720*480*n/d) {
			*w = 720; *h = 480;
		} else if (size == 1280*720*n/d) {
			*w = 1280; *h = 720;
		} else {
			return -1;
		}
	} else if (*h != -1) {
		/* Height has been specified */
		*w = size * d / (*h * n);
	} else if (*w != -1) {
		/* Width has been specified */
		*h = size * d / (*w * n);
	}

	return 0;
}


static void draw_rect_rgb565(void *surface, uint16_t color, int x, int y, int w, int h, int span)
{
	uint16_t *pix = (uint16_t *)surface + y*span + x;
	int xi, yi;

	for (yi=0; yi<h; yi++) {
		for (xi=0; xi<w; xi++) {
			*pix++ = color;
		}
		pix += (span-w);
	}
}

static void draw_simple_pic(surface_t *s)
{
	int w = (int)s->surface.width;
	int h = (int)s->surface.height;

	if (s->surface.format == V4L2_PIX_FMT_RGB565) {
		draw_rect_rgb565(s->virt, BLACK,   0,   0,   w,   h, w);
		draw_rect_rgb565(s->virt, BLUE,  w/4, h/4, w/4, h/2, w);
		draw_rect_rgb565(s->virt, RED,   w/2, h/4, w/4, h/2, w);
	}
}

static void blend(
	SHBEU *beu,
	DISPLAY *display,
	surface_t *parent,
	surface_t *overlay)
{
	unsigned char *bb_virt = display_get_back_buff_virt(display);
	unsigned long  bb_phys = display_get_back_buff_phys(display);
	int lcd_w = display_get_width(display);
	int lcd_h = display_get_height(display);
	beu_surface_t dst;

	/* Clear the back buffer */
	draw_rect_rgb565(bb_virt, BLACK, 0, 0, lcd_w, lcd_h, lcd_w);

	/* Destination surface info */
	dst.py = bb_phys;
	dst.pitch = lcd_w;
	dst.format = V4L2_PIX_FMT_RGB565;

	shbeu_blend(beu, &parent->surface, &overlay->surface, NULL, &dst);

	display_flip(display);
}


int main (int argc, char * argv[])
{
	UIOMux *uiomux = NULL;
	SHBEU *beu = NULL;
	DISPLAY *display = NULL;
	surface_t *current;
	surface_t *parent;
	int ret;
	int read_image = 1;
	int key;
	int run = 0;

	int show_version = 0;
	int show_help = 0;
	char * progname;
	int error = 0;

	int c;
	char * optstring = "hvc:s:";

#ifdef HAVE_GETOPT_LONG
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"input-colorspace", required_argument, 0, 'c'},
		{"input-size", required_argument, 0, 's'},
		{NULL,0,0,0}
	};
#endif

	parent = calloc(1, sizeof(surface_t));
	current = calloc(1, sizeof(surface_t));
	current->surface.width = -1;
	current->surface.height = -1;
	current->surface.format = -1;

	progname = argv[0];

	if ((argc == 2) && !strncmp (argv[1], "-?", 2)) {
#ifdef HAVE_GETOPT_LONG
		print_options (long_options, optstring);
#else
		print_short_options (optstring);
#endif
		exit (0);
	}

	while (1) {
#ifdef HAVE_GETOPT_LONG
		c = getopt_long (argc, argv, optstring, long_options, NULL);
#else
		c = getopt (argc, argv, optstring);
#endif
		if (c == -1) break;
		if (c == ':') {
			usage (progname);
			goto exit_err;
		}

		switch (c) {
		case 'h': /* help */
			show_help = 1;
			break;
		case 'v': /* version */
			show_version = 1;
			break;
		case 'c': /* input colorspace */
			set_colorspace (optarg, &current->surface.format);
			break;
		case 's': /* input size */
			set_size (optarg, &current->surface.width, &current->surface.height);
			break;
		default:
			break;
		}
	}

	if (show_version) {
		printf ("%s version " VERSION "\n", progname);
	}
      
	if (show_help) {
		usage (progname);
	}
      
	if (show_version || show_help) {
		goto exit_ok;
	}

	if (optind > argc) {
		usage (progname);
		goto exit_err;
	}

	if (optind < argc) {
		current->filename = argv[optind++];
		printf ("Input file: %s\n", current->filename);

		guess_colorspace (current->filename, &current->surface.format);
		guess_size (current->filename, current->surface.format, &current->surface.width, &current->surface.height);
	} else {
		printf ("No input file specified, drawing simple image\n");
		if (current->surface.width == -1) {
			current->surface.width = 320;
			current->surface.height = 240;
		}
		current->surface.format = V4L2_PIX_FMT_RGB565;
	}

	/* Check that all parameters are set */
	if (current->surface.format == -1) {
		fprintf (stderr, "ERROR: Input colorspace unspecified\n");
		error = 1;
	}
	if (current->surface.width == -1) {
		fprintf (stderr, "ERROR: Input width unspecified\n");
		error = 1;
	}
	if (current->surface.height == -1) {
		fprintf (stderr, "ERROR: Input height unspecified\n");
		error = 1;
	}

	if (error) goto exit_err;

	printf ("Input colorspace:\t%s\n", show_colorspace (current->surface.format));
	printf ("Input size:\t\t%dx%d %s\n", current->surface.width, current->surface.height,
		show_size (current->surface.width, current->surface.height));

	current->size = imgsize (current->surface.format, current->surface.width, current->surface.height);

	if (current->filename != NULL) {
		current->file = fopen (current->filename, "rb");
		if (current->file == NULL) {
			fprintf (stderr, "%s: unable to open input file %s\n",
				 progname, current->filename);
			goto exit_err;
		}
	}

	if ((uiomux = uiomux_open()) == 0) {
		fprintf (stderr, "Error opening UIOmux\n");
		goto exit_err;
	}

	if ((beu = shbeu_open()) == 0) {
		fprintf (stderr, "Error opening BEU\n");
		goto exit_err;
	}

	if ((display = display_open()) == 0) {
		fprintf (stderr, "Error opening display\n");
		goto exit_err;
	}


	/* Surface 1: static RGB image - same size as display */
	parent->surface.width = display_get_width(display);
	parent->surface.height = display_get_height(display);
	parent->surface.pitch = parent->surface.width;
	parent->size = 2 * parent->surface.width * parent->surface.height;
	parent->virt = uiomux_malloc (uiomux, UIOMUX_SH_BEU, parent->size, 32);
	if (!parent->virt) {
		perror("uiomux_malloc");
		goto exit_err;
	}
	parent->surface.py = uiomux_virt_to_phys (uiomux, UIOMUX_SH_BEU, parent->virt);
	parent->surface.pc = 0;
	parent->surface.pa = 0;
	parent->surface.alpha = 255;	/* Opaque */
	parent->surface.x = 0;
	parent->surface.y = 0;
	parent->surface.format = V4L2_PIX_FMT_RGB565;
	draw_simple_pic(parent);


	/* Surface 2: File input or static RGB image */
	current->virt = uiomux_malloc (uiomux, UIOMUX_SH_BEU, current->size, 32);
	if (!current->virt) {
		perror("uiomux_malloc");
		goto exit_err;
	}
	current->surface.pitch = current->surface.width;
	current->surface.py = uiomux_virt_to_phys (uiomux, UIOMUX_SH_BEU, current->virt);
	current->surface.pc = current->surface.py + (current->surface.width * current->surface.height);
	current->surface.pa = 0;
	current->surface.alpha = 200;	/* Semi-transparent */
	current->surface.x = 0;
	current->surface.y = 0;

	if (!current->filename) {
		draw_simple_pic(current);
	}

	/* ncurses init *
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);*/

	do
	{
		if (current->filename && read_image) {
			read_image = 0;
			/* Read input */
			if ((current->nread = fread (current->virt, 1, current->size, current->file)) != current->size) {
				if (current->nread == 0 && feof (current->file)) {
					break;
				} else {
					fprintf (stderr, "%s: error reading input file %s\n",
						 progname, current->filename);
				}
			}
		}

		blend (beu, display, parent, current);

		key = getch();
		switch (key)
		{
		case '=':
			current->surface.x = 0;
			current->surface.y = 0;
			break;
		case KEY_UP:
			current->surface.y -= 2;
			break;
		case KEY_DOWN:
			current->surface.y += 2;
			break;
		case KEY_LEFT:
			current->surface.x -= 2;
			break;
		case KEY_RIGHT:
			current->surface.x += 2;
			break;
		case ' ':
			read_image = 1;
			break;
		case 'q':
			run = 0;
			break;
		}
	} while (run);


	/* ncurses close *
	clrtoeol();
	refresh();
	endwin();*/

	display_close(display);
	shbeu_close(beu);
	uiomux_free (uiomux, UIOMUX_SH_BEU, parent->virt, parent->size);
	uiomux_free (uiomux, UIOMUX_SH_BEU, current->virt, current->size);
	uiomux_close (uiomux);


exit_ok:
	exit (0);

exit_err:
	if (display) display_close(display);
	if (beu)     shbeu_close(beu);
	if (uiomux)  uiomux_close (uiomux);
	exit (1);
}
