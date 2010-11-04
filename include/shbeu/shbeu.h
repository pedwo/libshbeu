/*
 * libshbeu: A library for controlling SH-Mobile BEU
 * Copyright (C) 2010 Renesas Electronics Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA
 */

/* Common information for SH video buffers */
#ifndef __SH_VIDEO_BUFFER_H__
#define __SH_VIDEO_BUFFER_H__

/** Surface formats */
typedef enum {
	SH_UNKNOWN,
	SH_NV12,    /**< YUV420: Y plane, packed CbCr plane, optional alpha plane */
	SH_NV16,    /**< YUV422: Y plane, packed CbCr plane, optional alpha plane */
	SH_RGB565,  /**< Packed RGB565 */
	SH_RGB24,   /**< Packed RGB888 */
	SH_BGR24,   /**< Packed BGR888 */
	SH_RGB32,   /**< Packed XRGB8888 (most significant byte ignored) */
	SH_ARGB32,  /**< Packed ARGB8888 */
} sh_vid_format_t;


/** Bounding rectange */
struct sh_vid_rect {
	int x;      /**< Offset from left in pixels */
	int y;      /**< Offset from top in pixels */
	int w;      /**< Width of surface in pixels */
	int h;      /**< Height of surface in pixels */
};

/** Surface */
struct sh_vid_surface {
	sh_vid_format_t format; /**< Surface format */
	int w;      /**< Width of surface in pixels */
	int h;      /**< Height of surface in pixels */
	void *py;   /**< Address of Y or RGB plane */
	void *pc;   /**< Address of CbCr plane (ignored for RGB) */
	void *pa;   /**< Address of Alpha plane */
};

struct format_info {
	sh_vid_format_t fmt;
	int y_bpp;
	int c_bpp_n;	/* numerator */
	int c_bpp_d;	/* denominator */
};

static const struct format_info fmts[] = {
	{ SH_UNKNOWN, 0, 0, 1 },
	{ SH_NV12,    1, 1, 2 },
	{ SH_NV16,    1, 1, 1 },
	{ SH_RGB565,  2, 0, 1 },
	{ SH_RGB24,   3, 0, 1 },
	{ SH_BGR24,   3, 0, 1 },
	{ SH_RGB32,   4, 0, 1 },
	{ SH_ARGB32,  4, 0, 1 },
};

static inline int is_ycbcr(sh_vid_format_t fmt)
{
	if (fmt >= SH_NV12 && fmt <= SH_NV16)
		return 1;
	return 0;
}

static inline int is_rgb(sh_vid_format_t fmt)
{
	if (fmt >= SH_RGB565 && fmt <= SH_ARGB32)
		return 1;
	return 0;
}

static inline int different_colorspace(sh_vid_format_t fmt1, sh_vid_format_t fmt2)
{
	if ((is_rgb(fmt1) && is_ycbcr(fmt2))
	    || (is_ycbcr(fmt1) && is_rgb(fmt2)))
		return 1;
	return 0;
}

static inline int size_y(sh_vid_format_t fmt, int nr_pixels)
{
	return (fmts[fmt].y_bpp * nr_pixels);
}

static inline int size_c(sh_vid_format_t fmt, int nr_pixels)
{
	return (fmts[fmt].c_bpp_n * nr_pixels) / fmts[fmt].c_bpp_d;
}

#endif /* __SH_VIDEO_BUFFER_H__ */


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
	unsigned long py;   /**< Physical address of Y or RGB plane */
	unsigned long pc;   /**< Physical address of CbCr plane (ignored for RGB) */
	unsigned long pa;   /**< Physical address of alpha plane (ignored for RGB or destination surface) */
	unsigned char alpha;/**< Fixed alpha value [0..255] for entire surface. Only used if a=0. 0=transparent, 255=opaque */
	int width;          /**< Width in pixels (ignored for destination surface) */
	int height;         /**< Height in pixels (ignored for destination surface) */
	int pitch;          /**< Line pitch in pixels */
	int x;              /**< Overlay position (horizontal) (ignored for destination surface) */
	int y;              /**< Overlay position (vertical) (ignored for destination surface) */
	sh_vid_format_t format; /**< Format */
};


/**
 * Open a BEU device.
 * \retval 0 Failure, otherwise BEU handle
 */
SHBEU *shbeu_open(void);

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

