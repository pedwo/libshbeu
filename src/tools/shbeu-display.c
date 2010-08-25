/*
 * Tool to demonstrate BEU hardware acceleration of raw image overlay.
 *
 * The RGB/YCbCr source images are read from files and displayed one on top of
 * another on the framebuffer. It uses an ncurses interface to allow the user
 * move the top most image and advance the image input.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_NCURSES
#ifdef HAVE_NCURSES_SUBDIR
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif
#endif
#include <uiomux/uiomux.h>
#include "shbeu/shbeu.h"
#include "display.h"

/* RGB565 colors */
#define BLACK 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F

#define U_SEC_PER_SEC 1000000
#define N_SEC_PER_SEC 1000000000

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
	printf ("Usage: %s [options] -i <input file> \n", progname);
	printf ("Overlays raw image data using the SH-Mobile BEU and displays on screen.\n");
	printf ("Options and input file can be specified for up to 3 inputs, e.g.\n");
	printf ("  %s -s vga -i vga.yuv -s qvga -i qvga.rgb -s qcif -i qcif.rgb\n", progname);
	printf ("\n");
	printf ("\nInput options\n");
	printf ("  -c, --input-colorspace (RGB565, RGBx888, NV12, YCbCr420, NV16, YCbCr422)\n");
	printf ("                         Specify input colorspace\n");
	printf ("  -s, --input-size       Set the input image size (qcif, cif, qvga, vga, d1, 720p)\n");
	printf ("\nControl keys\n");
	printf ("  Space key              Read next frame\n");
	printf ("  Cursor keys            Pan\n");
	printf ("  =                      Reset panning\n");
	printf ("  q                      Quit\n");
	printf ("\nMiscellaneous options\n");
	printf ("  -h, --help             Display this help and exit\n");
	printf ("  -v, --version          Output version information and exit\n");
	printf ("\nFile extensions are interpreted as follows unless otherwise specified:\n");
	printf ("  .yuv    YCbCr420\n");
	printf ("  .420    YCbCr420\n");
	printf ("  .422    YCbCr422\n");
	printf ("  .rgb    RGB565\n");
	printf ("  .565    RGB565\n");
	printf ("  .x888   RGBx888\n");
	printf ("\n");
	printf ("Please report bugs to <linux-sh@vger.kernel.org>\n");
}

struct sizes_t {
	const char *name;
	int w;
	int h;
};

static const struct sizes_t sizes[] = {
	{ "QCIF", 176,  144 },
	{ "CIF",  352,  288 },
	{ "QVGA", 320,  240 },
	{ "VGA",  640,  480 },
	{ "D1",   720,  480 },
	{ "720p", 1280, 720 },
};

static int set_size (char * arg, int * w, int * h)
{
	int nr_sizes = sizeof(sizes) / sizeof(sizes[0]);
	int i;

	if (!arg)
		return -1;

	for (i=0; i<nr_sizes; i++) {
		if (!strncasecmp (arg, sizes[i].name, strlen(sizes[i].name))) {
			*w = sizes[i].w;
			*h = sizes[i].h;
			return 0;
		}
	}

	return -1;
}

static const char * show_size (int w, int h)
{
	int nr_sizes = sizeof(sizes) / sizeof(sizes[0]);
	int i;

	for (i=0; i<nr_sizes; i++) {
		if (w == sizes[i].w && h == sizes[i].h)
			return sizes[i].name;
	}

	return "";
}

struct extensions_t {
	const char *ext;
	int fmt;
};

static const struct extensions_t exts[] = {
	{ "RGB565",   V4L2_PIX_FMT_RGB565 },
	{ "rgb",      V4L2_PIX_FMT_RGB565 },
	{ "RGBx888",  V4L2_PIX_FMT_RGB32 },
	{ "x888",     V4L2_PIX_FMT_RGB32 },
	{ "YCbCr420", V4L2_PIX_FMT_NV12 },
	{ "420",      V4L2_PIX_FMT_NV12 },
	{ "yuv",      V4L2_PIX_FMT_NV12 },
	{ "NV12",     V4L2_PIX_FMT_NV12 },
	{ "YCbCr422", V4L2_PIX_FMT_NV16 },
	{ "422",      V4L2_PIX_FMT_NV16 },
	{ "NV16",     V4L2_PIX_FMT_NV16 },
};

static int set_colorspace (char * arg, int * c)
{
	int nr_exts = sizeof(exts) / sizeof(exts[0]);
	int i;

	if (!arg)
		return -1;

	for (i=0; i<nr_exts; i++) {
		if (!strncasecmp (arg, exts[i].ext, strlen(exts[i].ext))) {
			*c = exts[i].fmt;
			return 0;
		}
	}

	return -1;
}

static const char * show_colorspace (int c)
{
	int nr_exts = sizeof(exts) / sizeof(exts[0]);
	int i;

	for (i=0; i<nr_exts; i++) {
		if (c == exts[i].fmt)
			return exts[i].ext;
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
	case V4L2_PIX_FMT_RGB32:
		/* 4 bytes per pixel */
		n=4; d=1;
		break;
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

	if (filename == NULL || !strcmp (filename, "-"))
		return -1;

	/* If the colorspace is already set (eg. explicitly by user args)
	 * then don't try to guess */
	if (*c != -1) return -1;

	ext = strrchr (filename, '.');
	if (ext == NULL) return -1;

	return set_colorspace(ext+1, c);
}

static int guess_size (char * filename, int colorspace, int * w, int * h)
{
	off_t size;

	if ((size = filesize (filename)) == -1) {
		return -1;
	}

	if (*w==-1 && *h==-1) {
		/* Image size unspecified */
		int nr_sizes = sizeof(sizes) / sizeof(sizes[0]);
		int i;

		for (i=0; i<nr_sizes; i++) {

			if (size == imgsize(colorspace, sizes[i].w, sizes[i].h)) {
				*w = sizes[i].w;
				*h = sizes[i].h;
				return 0;
			}
		}
	}

	return -1;
}


/* Total microseconds elapsed */
static long
elapsed_us (struct timespec * start)
{
	struct timespec curr;
	long secs, nsecs;
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &curr);
	if (ret == -1) return ret;

	secs = curr.tv_sec - start->tv_sec;
	nsecs = curr.tv_nsec - start->tv_nsec;
	if (nsecs < 0) {
		secs--;
		nsecs += N_SEC_PER_SEC;
	}

	return (secs*U_SEC_PER_SEC) + nsecs/1000;
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

static int nr_blends = 0;
static long time_total_us = 0;

static void blend(
	SHBEU *beu,
	DISPLAY *display,
	beu_surface_t **sources,
	int nr_inputs)
{
	unsigned char *bb_virt = display_get_back_buff_virt(display);
	unsigned long  bb_phys = display_get_back_buff_phys(display);
	int lcd_w = display_get_width(display);
	int lcd_h = display_get_height(display);
	beu_surface_t dst;
	int i;
	struct timespec start;

	/* Clear the back buffer */
	draw_rect_rgb565(bb_virt, BLACK, 0, 0, lcd_w, lcd_h, lcd_w);

	/* Destination surface info */
	dst.py = bb_phys;
	dst.pitch = lcd_w;
	dst.format = V4L2_PIX_FMT_RGB565;

	/* Limit the size of the images used in blend to the LCD */
	for (i=0; i<nr_inputs; i++) {
		if (sources[i]->width > lcd_w)
			sources[i]->width = lcd_w;
		if (sources[i]->height > lcd_h)
			sources[i]->height = lcd_h;
	}

	clock_gettime(CLOCK_MONOTONIC, &start);

	if (nr_inputs == 3)
		shbeu_blend(beu, sources[0], sources[1], sources[2], &dst);
	else if (nr_inputs == 2)
		shbeu_blend(beu, sources[0], sources[1], NULL, &dst);
	else if (nr_inputs == 1)
		shbeu_blend(beu, sources[0], NULL, NULL, &dst);

	time_total_us += elapsed_us(&start);
	nr_blends++;

	display_flip(display);
}

int setup_input_surface(char *progname, UIOMux *uiomux, int i, surface_t *s)
{
	printf ("[%d] Input colorspace:\t%s\n", i, show_colorspace (s->surface.format));
	printf ("[%d] Input size:      \t%dx%d %s\n", i, s->surface.width, s->surface.height,
		show_size (s->surface.width, s->surface.height));

	s->file = fopen (s->filename, "rb");
	if (s->file == NULL) {
		fprintf (stderr, "%s: unable to open input file %s\n",
			 progname, s->filename);
		return -1;
	}

	s->size = imgsize (s->surface.format, s->surface.width, s->surface.height);
	s->virt = uiomux_malloc (uiomux, UIOMUX_SH_BEU, s->size, 32);
	if (!s->virt) {
		perror("uiomux_malloc");
		return -1;
	}
	s->surface.pitch = s->surface.width;
	s->surface.py = uiomux_virt_to_phys (uiomux, UIOMUX_SH_BEU, s->virt);
	s->surface.pc = s->surface.py + (s->surface.width * s->surface.height);
	s->surface.pa = 0;
	s->surface.alpha = 255 - i*70;	/* 1st layer opaque, others semi-transparent */
	s->surface.x = 0;
	s->surface.y = 0;

	return 0;
}

void create_per_pixel_alpha(UIOMux *uiomux, surface_t *s)
{
	int y, alpha = 255;
	unsigned char *pA;

	/* Create alpha plane */
	pA = uiomux_malloc (uiomux, UIOMUX_SH_BEU, (s->surface.width * s->surface.height), 32);
	if (pA) {
		for (y=0; y<s->surface.height; y++) {
			alpha = (y << 8) / s->surface.height;
			memset(pA+y*s->surface.width, alpha, s->surface.width);
		}
	}
	s->surface.pa = uiomux_virt_to_phys (uiomux, UIOMUX_SH_BEU, pA);
}

int read_image_from_file(surface_t *s)
{
	int run = 1;

	if (s->filename) {
		/* Read input */
		if ((s->nread = fread (s->virt, 1, s->size, s->file)) != s->size) {
			if (s->nread == 0 && feof (s->file)) {
				run = 0;
			} else {
				fprintf (stderr, "error reading input file %s\n", s->filename);
			}
		}
	}

	return run;
}

int main (int argc, char * argv[])
{
	UIOMux *uiomux = NULL;
	SHBEU *beu = NULL;
	DISPLAY *display = NULL;
	surface_t in[3];
	beu_surface_t *beu_inputs[3];
	surface_t *current;
	int i, nr_inputs = 0;
	int ret;
	int read_image = 1;
	int key;
	int run = 1;
	long us;

	int show_version = 0;
	int show_help = 0;
	char * progname;
	int error = 0;

	int c;
	char * optstring = "hvc:s:i:";

#ifdef HAVE_GETOPT_LONG
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"input-colorspace", required_argument, 0, 'c'},
		{"input-size", required_argument, 0, 's'},
		{"input-file", required_argument, 0, 'i'},
		{NULL,0,0,0}
	};
#endif

	memset(&in, 0, sizeof(in));
	for (i=0; i<3; i++) {
		current = &in[i];
		current->surface.width = -1;
		current->surface.height = -1;
		current->surface.format = -1;
		beu_inputs[i] = &in[i].surface;
	}
	current = &in[nr_inputs];

	progname = argv[0];

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
		case 'i': /* input file */
			current->filename = optarg;
			/* Setup next input file */
			current = &in[++nr_inputs];
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

	for (i=0; i<nr_inputs; i++) {
		current = &in[i];

		if (current->filename == NULL) {
			fprintf (stderr, "ERROR: Input file unspecified\n");
			goto exit_err;
		}

		printf ("[%d] Input file:      \t%s\n", i, current->filename);

		guess_colorspace (current->filename, &current->surface.format);
		guess_size (current->filename, current->surface.format, &current->surface.width, &current->surface.height);

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

		if (setup_input_surface(progname, uiomux, i, current) < 0) 
			goto exit_err;
	}

#if TEST_PER_PIXEL_ALPHA
	/* Apply per-pixel alpha to top layer */
	create_per_pixel_alpha(uiomux, current);
#endif

#ifdef HAVE_NCURSES
	/* ncurses init */
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
#endif

	do
	{
		if (read_image) {
			/* Read the next image for each input. Stop if any file lacks further data */
			for (i=0; i<nr_inputs; i++)
				run = read_image_from_file(&in[i]);
#ifdef HAVE_NCURSES
			read_image = 0;
#endif
		}
		if (!run) break;

		/* Perform the blend */
		blend (beu, display, beu_inputs, nr_inputs);

#ifdef HAVE_NCURSES
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
#endif
	} while (run);


#ifdef HAVE_NCURSES
	/* ncurses close */
	clrtoeol();
	refresh();
	endwin();
#endif

	for (i=0; i<nr_inputs; i++)
		uiomux_free (uiomux, UIOMUX_SH_BEU, in[i].virt, in[i].size);

	display_close(display);
	shbeu_close(beu);
	uiomux_close (uiomux);

	us = time_total_us/nr_blends;
	printf("Average time for blend is %luus (%d pixel/us)\n", us, (in[0].surface.width * in[0].surface.height)/us);

exit_ok:
	exit (0);

exit_err:
	if (display) display_close(display);
	if (beu)     shbeu_close(beu);
	if (uiomux)  uiomux_close (uiomux);
	exit (1);
}
