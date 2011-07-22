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

#ifndef __SHBEU_REGS_H__
#define __SHBEU_REGS_H__

/* All addresses are for Plane A. A lot of the registers have Plane A, Plane B
 * and Mirror addresses. See the hardware manual...
 */
#define PLANE_A 0x0000
#define PLANE_B 0x1000
#define MIRROR  0x2000


#define BESTR  0x00		/* Start register */

/*
 * Image source registers
 */
#define SRC1_BASE 0x10
#define SRC2_BASE 0x28
#define SRC3_BASE 0x40
/* All 3 blend inputs have these registers: */
#define BSMWR 0x00		/* src: line length */
#define BSSZR 0x04		/* src: image size */
#define BSAYR 0x08		/* src: y/rgb plane address */
#define BSACR 0x0c		/* src: c plane address */
#define BSAAR 0x10		/* src: alpha plane address */
#define BSIFR 0x14		/* src: format */

#define BTPSR 0x58		/* tile pattern size */

/*
 * Multidisplay registers
 */
#define MD_SRC1_BASE 0x70
#define MD_SRC2_BASE 0x80
#define MD_SRC3_BASE 0x90
#define MD_SRC4_BASE 0xA0
/* All 4 Multidisplay inputs have these registers: */
#define BMSMWR 0x00		/* src: line length */
#define BMSSZR 0x04		/* src: image size */
#define BMSAYR 0x08		/* src: y/rgb plane address */
#define BMSACR 0x0c		/* src: c plane address */

#define BMSIFR 0xf0		/* src: format */

/*
 * Other registers
 */
#define BBLCR0 0x100	/* blend control 0 */
#define BBLCR1 0x104	/* blend control 1 */
#define BPROCR 0x108	/* process control */
#define BMWCR0 0x10c	/* multiwindow control 0 */
#define BLOCR1 0x114	/* Blend location 1 */
#define BLOCR2 0x118	/* Blend location 2 */
#define BLOCR3 0x11c	/* Blend location 3 */

/*
 * Multidisplay registers
 */
#define BMLOCR1 0x120	/* Multidisplay location 1 */
#define BMLOCR2 0x124	/* Multidisplay location 2 */
#define BMLOCR3 0x128	/* Multidisplay location 3 */
#define BMLOCR4 0x12c	/* Multidisplay location 4 */
#define BMPCCR1 0x130	/* Multidisplay transparent control 1 */
#define BMPCCR2 0x134	/* Multidisplay transparent control 2 */

#define BPKFR   0x140	/* Blend pack form */

/*
 * Transparent control
 */
#define BPCCR0  0x144
#define BPCCR11 0x148
#define BPCCR12 0x14c
#define BPCCR21 0x150
#define BPCCR22 0x154
#define BPCCR31 0x158
#define BPCCR32 0x15c

/*
 * Image destination registers
 */
#define BDMWR   0x160	/* dst: line length */
#define BDAYR   0x164	/* dst: y/rgb plane address */
#define BDACR   0x168	/* dst: c plane address */

#define BAFXR   0x180	/* address fixed */
#define BSWPR   0x184	/* swapping */
#define BEIER   0x188	/* event interrupt enable */
#define BEVTR   0x18c	/* event */
#define BRCNTR  0x194	/* register control */
#define BSTAR   0x198	/* status */
#define BBRSTR  0x19c	/* reset */
#define BRCHR   0x1a0	/* register-plane forcible setting */

#define CLUT_BASE 0x3000	/* Color Lookup Table */


/*
 * Bits within registers
 */

/* BESTR */
#define BESTR_BEIVK 1
#define BESTR_CHON1 (1 << 8)
#define BESTR_CHON2 (2 << 8)
#define BESTR_CHON3 (4 << 8)

/* BSIFRx */
#define CHRR_YCBCR_422   (1 << 8)
#define CHRR_YCBCR_420   (2 << 8)
#define CHRR_YCBCR_ALPHA (3 << 8)
#define RPKF_RGB32       0
#define RPKF_RGB24       2
#define RPKF_BGR24       11
#define RPKF_RGB16       3

/* BPKFR */
#define BPKFR_TM2		(1 << 21)
#define BPKFR_TM		(1 << 20)
#define BPKFR_DITH2		(1 << 16)
#define BPKFR_DITH1		(1 << 12)
#define BPKFR_RY 		(1 << 11)
#define BPKFR_TE 		(1 << 10)
#define WPCK_RGB16       6
#define WPCK_RGB24       0x15
#define WPCK_RGB32       0x13

/* Others */
#define BSIFR1_IN1TM	(1 << 13)
#define BSIFR1_IN1TE 	(1 << 12)
#define BSWPR_MODSEL	(1UL << 31)
#define BBLCR1_OUTPUT_MEM (1 << 16)

#endif /* __SHBEU_REGS_H__ */
