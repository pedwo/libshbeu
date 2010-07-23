
#ifndef __SHBEU_H__
#define __SHBEU_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \mainpage
 *
 * \section intro SHBEU: A library for accessing the BEU.
 *
 * This is the documentation for the SHBEU C API.
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
 PKG_CHECK_MODULES(SHBEU, shbeu >= 0.5.0,
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

/** Surface specification
 * \param py     Physical address of Y or RGB plane
 * \param pc     Physical address of CbCr plane (ignored for RGB)
 * \param pa     Physical address of alpha plane (ignored for RGB/dst)
 * \param alpha  Fixed alpha value [0..255] for entire surface. Only used if a=0
 * \param width  Width in pixels
 * \param height Height in pixels
 * \param pitch  Line pitch
 * \param format Format (V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV16, V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_RGB32)
 * \param x      Overlay position (horizontal)
 * \param y      Overlay position (vertical)
 */
typedef struct {
	unsigned long py;
	unsigned long pc;
	unsigned long pa;
	unsigned long alpha;
	unsigned long width;
	unsigned long height;
	unsigned long pitch;
	unsigned long x;
	unsigned long y;
	int format;
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

/** Perform a surface blend
 * \param beu BEU handle
 * \retval 0 Success
 * \retval -1 Error: Unsupported parameters
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

