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
#include <linux/videodev2.h>	/* For pixel formats */

/**
 * An opaque handle to the BEU.
 */
struct SHBEU;
typedef struct SHBEU SHBEU;

/**
 * Surface specification.
 */
typedef struct {
	unsigned long py;   /**< Physical address of Y or RGB plane */
	unsigned long pc;   /**< Physical address of CbCr plane (ignored for RGB) */
	unsigned long pa;   /**< Physical address of alpha plane (ignored for RGB or destination surface) */
	unsigned char alpha;/**< Fixed alpha value [0..255] for entire surface. Only used if a=0. 0=transparent, 255=opaque */
	int width;          /**< Width in pixels (ignored for destination surface) */
	int height;         /**< Height in pixels (ignored for destination surface) */
	int pitch;          /**< Line pitch in pixels */
	int x;              /**< Format (V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV16, V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_RGB32) */
	int y;              /**< Overlay position (horizontal) (ignored for destination surface) */
	int format;         /**< Overlay position (vertical) (ignored for destination surface) */
} beu_surface_t;


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
	beu_surface_t *src1,
	beu_surface_t *src2,
	beu_surface_t *src3,
	beu_surface_t *dest);

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
	beu_surface_t *src1,
	beu_surface_t *src2,
	beu_surface_t *src3,
	beu_surface_t *dest);

#ifdef __cplusplus
}
#endif

#endif /* __SHBEU_H__ */

