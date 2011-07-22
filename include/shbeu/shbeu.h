/*
 * libshbeu: A library for controlling SH-Mobile BEU
 * Copyright (C) 2010 Renesas Electronics Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Common information for Renesas video buffers */
#ifndef __REN_VIDEO_BUFFER_H__
#define __REN_VIDEO_BUFFER_H__

/* Notes on YUV/YCbCr:
 * YUV historically refers to analogue color space, and YCbCr to digital.
 * The formula used to convert to/from RGB is BT.601 or BT.709. HDTV specifies
 * BT.709, everything else BT.601.
 * MPEG standards use 'clamped' data with Y[16,235], CbCr[16,240]. JFIF file
 * format for JPEG specifies full-range data.
 * All YCbCr formats here are BT.601, Y[16,235], CbCr[16,240].
 */

/** Surface formats */
typedef enum {
	REN_UNKNOWN,
	REN_NV12,    /**< YCbCr420: Y plane, packed CbCr plane, optional alpha plane */
	REN_NV16,    /**< YCbCr422: Y plane, packed CbCr plane, optional alpha plane */
	REN_RGB565,  /**< Packed RGB565 */
	REN_RGB24,   /**< Packed RGB888 */
	REN_BGR24,   /**< Packed BGR888 */
	REN_RGB32,   /**< Packed XRGB8888 (most significant byte ignored) */
	REN_ARGB32,  /**< Packed ARGB8888 */
} ren_vid_format_t;


/** Bounding rectange */
struct ren_vid_rect {
	int x;      /**< Offset from left in pixels */
	int y;      /**< Offset from top in pixels */
	int w;      /**< Width of rectange in pixels */
	int h;      /**< Height of rectange in pixels */
};

/** Surface */
struct ren_vid_surface {
	ren_vid_format_t format; /**< Surface format */
	int w;      /**< Width of active surface in pixels */
	int h;      /**< Height of active surface in pixels */
	int pitch;  /**< Width of surface in pixels */
	void *py;   /**< Address of Y or RGB plane */
	void *pc;   /**< Address of CbCr plane (ignored for RGB) */
	void *pa;   /**< Address of Alpha plane (ignored) */
};

struct format_info {
	ren_vid_format_t fmt;    /**< surface format */
	int y_bpp;      /**< Luma numerator */
	int c_bpp;      /**< Chroma numerator */
	int c_bpp_n;    /**< Chroma numerator */
	int c_bpp_d;    /**< Chroma denominator */
	int c_ss_horz;  /**< Chroma horizontal sub-sampling */
	int c_ss_vert;  /**< Chroma vertical sub-sampling */
};

static const struct format_info fmts[] = {
	{ REN_UNKNOWN, 0, 0, 0, 1, 1, 1 },
	{ REN_NV12,    1, 2, 1, 2, 2, 2 },
	{ REN_NV16,    1, 2, 1, 1, 2, 1 },
	{ REN_RGB565,  2, 0, 0, 1, 1, 1 },
	{ REN_RGB24,   3, 0, 0, 1, 1, 1 },
	{ REN_BGR24,   3, 0, 0, 1, 1, 1 },
	{ REN_RGB32,   4, 0, 0, 1, 1, 1 },
	{ REN_ARGB32,  4, 0, 0, 1, 1, 1 },
};

static inline int is_ycbcr(ren_vid_format_t fmt)
{
	if (fmt >= REN_NV12 && fmt <= REN_NV16)
		return 1;
	return 0;
}

static inline int is_rgb(ren_vid_format_t fmt)
{
	if (fmt >= REN_RGB565 && fmt <= REN_ARGB32)
		return 1;
	return 0;
}

static inline int different_colorspace(ren_vid_format_t fmt1, ren_vid_format_t fmt2)
{
	if ((is_rgb(fmt1) && is_ycbcr(fmt2))
	    || (is_ycbcr(fmt1) && is_rgb(fmt2)))
		return 1;
	return 0;
}

static inline size_t size_y(ren_vid_format_t format, int nr_pixels)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->y_bpp * nr_pixels);
}

static inline size_t size_c(ren_vid_format_t format, int nr_pixels)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->c_bpp_n * nr_pixels) / fmt->c_bpp_d;
}

static inline size_t size_a(ren_vid_format_t format, int nr_pixels)
{
	/* Assume 1 byte per alpha pixel */
	return nr_pixels;
}

static inline size_t offset_y(ren_vid_format_t format, int w, int h, int pitch)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->y_bpp * ((h * pitch) + w));
}

static inline size_t offset_c(ren_vid_format_t format, int w, int h, int pitch)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->c_bpp * (((h/fmt->c_ss_vert) * pitch/fmt->c_ss_horz) + w/fmt->c_ss_horz));
}

static inline size_t offset_a(ren_vid_format_t format, int w, int h, int pitch)
{
	/* Assume 1 byte per alpha pixel */
	return ((h * pitch) + w);
}

static int horz_increment(ren_vid_format_t format)
{
	/* Only restriction is caused by chroma sub-sampling */
	return fmts[format].c_ss_horz;
}

static int vert_increment(ren_vid_format_t format)
{
	/* Only restriction is caused by chroma sub-sampling */
	return fmts[format].c_ss_vert;
}

/* Get a new surface descriptor based on a selection */
static inline void get_sel_surface(
	struct ren_vid_surface *out,
	const struct ren_vid_surface *in,
	const struct ren_vid_rect *sel)
{
	int x = sel->x & ~horz_increment(in->format);
	int y = sel->y & ~vert_increment(in->format);

	*out = *in;
	out->w = sel->w & ~horz_increment(in->format);
	out->h = sel->h & ~vert_increment(in->format);

	if (in->py) out->py += offset_y(in->format, x, y, in->pitch);
	if (in->pc) out->pc += offset_c(in->format, x, y, in->pitch);
	if (in->pa) out->pa += offset_a(in->format, x, y, in->pitch);
}

#endif /* __REN_VIDEO_BUFFER_H__ */


#ifndef __SHBEU_H__
#define __SHBEU_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \mainpage
 *
 * \section intro SHBEU: A library for accessing the BEU.
 *
 * This is the documentation for the SHBEU C API. Please read the associated
 * README, COPYING and TODO files.
 *
 * Features:
 *  - Simple interface to blend images
 *
 * \subsection contents Contents
 *
 * - \link shbeu.h shbeu.h \endlink:
 * Documentation of the SHBEU C API
 *
 * - \link configuration Configuration \endlink:
 * Customizing libshbeu
 *
 * - \link building Building \endlink:
 *
 */

/** \defgroup configuration Configuration
 * \section configure ./configure
 *
 * It is possible to customize the functionality of libshbeu
 * by using various ./configure flags when building it from source.
 *
 * For general information about using ./configure, see the file
 * \link install INSTALL \endlink
 *
 */

/** \defgroup install Installation
 * \section install INSTALL
 *
 * \include INSTALL
 */

/** \defgroup building Building against libshbeu
 *
 *
 * \section autoconf Using GNU autoconf
 *
 * If you are using GNU autoconf, you do not need to call pkg-config
 * directly. Use the following macro to determine if libshbeu is
 * available:
 *
 <pre>
 PKG_CHECK_MODULES(SHBEU, shbeu >= 0.0.1,
                   HAVE_SHBEU="yes", HAVE_SHBEU="no")
 if test "x$HAVE_SHBEU" = "xyes" ; then
   AC_SUBST(SHBEU_CFLAGS)
   AC_SUBST(SHBEU_LIBS)
 fi
 </pre>
 *
 * If libshbeu is found, HAVE_SHBEU will be set to "yes", and
 * the autoconf variables SHBEU_CFLAGS and SHBEU_LIBS will
 * be set appropriately.
 *
 * \section pkg-config Determining compiler options with pkg-config
 *
 * If you are not using GNU autoconf in your project, you can use the
 * pkg-config tool directly to determine the correct compiler options.
 *
 <pre>
 SHBEU_CFLAGS=`pkg-config --cflags shbeu`

 SHBEU_LIBS=`pkg-config --libs shbeu`
 </pre>
 *
 */

/** \file
 * The libshbeu C API.
 *
 */

/**
 * An opaque handle to the BEU.
 */
struct SHBEU;
typedef struct SHBEU SHBEU;

/**
 * Surface specification.
 */
struct shbeu_surface {
	struct ren_vid_surface s; /**< surface */
	unsigned char alpha;/**< Fixed alpha value [0..255] for entire surface. Only used if pa=0. 0=transparent, 255=opaque */
	int x;              /**< Overlay position (horizontal) (ignored for destination surface) */
	int y;              /**< Overlay position (vertical) (ignored for destination surface) */
};


/**
 * Open a BEU device.
 * \retval 0 Failure, otherwise BEU handle
 */
SHBEU *shbeu_open(void);

/**
 * Open a BEU device with the specified name.
 * If more than one BEU is available on the platform, each BEU
 * has a name such as 'BEU0', 'BEU1', and so on. This API will allow
 * to open a specific BEU by shbeu_open_named("BEU0") for instance.
 * \retval 0 Failure, otherwise BEU handle.
 */
SHBEU *shbeu_open_named(const char *name);

/**
 * Close a BEU device.
 * \param beu BEU handle
 */
void shbeu_close(SHBEU *beu);

/** Start a surface blend
 * \param beu BEU handle
 * \param src1 Parent surface. The output will be this size.
 * \param src2 Overlay surface. Can be NULL, if no overlay required.
 * \param src3 Overlay surface. Can be NULL, if no overlay required.
 * \param dest Output surface.
 * \retval 0 Success
 * \retval -1 Error
 */
int
shbeu_start_blend(
	SHBEU *beu,
	const struct shbeu_surface *src1,
	const struct shbeu_surface *src2,
	const struct shbeu_surface *src3,
	const struct shbeu_surface *dest);

/** Wait for a BEU operation to complete. The operation is started by a call to shbeu_start_blend.
 * \param beu BEU handle
 */
void
shbeu_wait(SHBEU *beu);

/** Perform a surface blend.
 * See shbeu_start_blend for parameter definitions.
 */
int
shbeu_blend(
	SHBEU *beu,
	const struct shbeu_surface *src1,
	const struct shbeu_surface *src2,
	const struct shbeu_surface *src3,
	const struct shbeu_surface *dest);

#ifdef __cplusplus
}
#endif

#endif /* __SHBEU_H__ */
