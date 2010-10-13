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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>

#include <uiomux/uiomux.h>
#include "shbeu/shbeu.h"
#include "shbeu_regs.h"

/* #define DEBUG 2 */

#ifdef DEBUG
#define debug_info(s) fprintf(stderr, "%s: %s\n", __func__, s)
#else
#define debug_info(s)
#endif

struct uio_map {
	unsigned long address;
	unsigned long size;
	void *iomem;
};

struct SHBEU {
	UIOMux *uiomux;
	struct uio_map uio_mmio;
};


/* Helper functions for reading registers. */

static unsigned long read_reg(struct uio_map *ump, int reg_nr)
{
	volatile unsigned long *reg = ump->iomem + reg_nr;
	unsigned long value = *reg;

#if (DEBUG == 2)
	fprintf(stderr, " read_reg[0x%X] returned %lX\n", reg_nr, value);
#endif

	return value;
}

static void write_reg(struct uio_map *ump, unsigned long value, int reg_nr)
{
	volatile unsigned long *reg = ump->iomem + reg_nr;

#if (DEBUG == 2)
	fprintf(stderr, " write_reg[0x%X] = %lX\n", reg_nr, value);
#endif

	*reg = value;
}

SHBEU *shbeu_open(void)
{
	SHBEU *beu;
	int ret;

	beu = calloc(1, sizeof(*beu));
	if (!beu)
		goto err;

	beu->uiomux = uiomux_open();
	if (!beu->uiomux)
		goto err;

	ret = uiomux_get_mmio (beu->uiomux, UIOMUX_SH_BEU,
		&beu->uio_mmio.address,
		&beu->uio_mmio.size,
		&beu->uio_mmio.iomem);
	if (!ret)
		goto err;

#ifdef DEBUG
	fprintf(stderr, "BEU registers start at 0x%lX (virt: %p)\n", beu->uio_mmio.address, beu->uio_mmio.iomem);
#endif

	return beu;

err:
	shbeu_close(beu);
	return 0;
}

void shbeu_close(SHBEU *pvt)
{
	if (pvt) {
		if (pvt->uiomux)
			uiomux_close(pvt->uiomux);
		free(pvt);
	}
}

static int is_ycbcr(int fmt)
{
	if ((fmt == V4L2_PIX_FMT_NV12) || (fmt == V4L2_PIX_FMT_NV16))
		return 1;
	return 0;
}

static int is_rgb(int fmt)
{
	if ((fmt == V4L2_PIX_FMT_RGB565) || (fmt == V4L2_PIX_FMT_RGB32))
		return 1;
	return 0;
}

static int different_colorspace(int fmt1, int fmt2)
{
	if ((is_rgb(fmt1) && is_ycbcr(fmt2))
	   || (is_ycbcr(fmt1) && is_rgb(fmt2)))
		return 1;
	return 0;
}

/* Setup input surface */
static int
setup_src_surface(struct uio_map *ump, int index, beu_surface_t *surface)
{
	const int offsets[] = {SRC1_BASE, SRC2_BASE, SRC3_BASE};
	int offset = offsets[index];
	unsigned long tmp;
	unsigned long fmt_reg;
	unsigned long width, height, pitch;

	if (!surface)
		return 0;

#ifdef DEBUG
	fprintf(stderr, "\nsrc%d: fmt=%d: width=%lu, height=%lu pitch=%lu\n",
		index+1, surface->format, surface->width, surface->height, surface->pitch);
	fprintf(stderr, "\tY/RGB (0x%lX), C (0x%lX), alpha (0x%lX)\n", surface->py, surface->pc, surface->pa);
	fprintf(stderr, "\toffset=(%lu,%lu), alternative alpha =%lu\n", surface->x, surface->y, surface->alpha);
#endif

	if (!surface->py)
		return -1;

	if ((surface->width % 4) || (surface->pitch % 4) || (surface->height % 4))
		return -1;

	if ((surface->width > 4092) || (surface->pitch > 4092) || (surface->height > 4092))
		return -1;

	if (is_rgb(surface->format) && surface->pa) {
		/* allow RGB32 when the alpha plane has the same physical address as py. 
		   this is how we specify ARGB, because there is no V4L2_PIX_FMT for ARGB */
		if (!((surface->format == V4L2_PIX_FMT_RGB32) && (surface->pa == surface->py)))
			return -1;
	}

	width = surface->width;
	height = surface->height;
	pitch = surface->pitch;

	/* BSMWR (pitch) is in bytes */
	if (surface->format == V4L2_PIX_FMT_RGB565)
		pitch *= 2;
	else if (surface->format == V4L2_PIX_FMT_RGB32)
		pitch *= 4;
	write_reg(ump, pitch, BSMWR + offset);

	write_reg(ump, (height << 16) | width, BSSZR + offset);
	write_reg(ump, surface->py, BSAYR + offset);
	write_reg(ump, surface->pc, BSACR + offset);
	write_reg(ump, surface->pa, BSAAR + offset);

	/* Surface format */
	if ((surface->format == V4L2_PIX_FMT_NV12) && !surface->pa)
		fmt_reg = CHRR_YCBCR_420;
	else if ((surface->format == V4L2_PIX_FMT_NV12) && surface->pa)
		fmt_reg = CHRR_aYCBCR_420;
	else if ((surface->format == V4L2_PIX_FMT_NV16) && !surface->pa)
		fmt_reg = CHRR_YCBCR_422;
	else if ((surface->format == V4L2_PIX_FMT_NV16) && surface->pa)
		fmt_reg = CHRR_aYCBCR_422;
	else if (surface->format == V4L2_PIX_FMT_RGB565)
		fmt_reg = RPKF_RGB16;
	else if (surface->format == V4L2_PIX_FMT_RGB32)
		fmt_reg = RPKF_RGB32;
	else
		return -1;

	write_reg(ump, fmt_reg, BSIFR + offset);

	/* Position of overlay */
	tmp = (surface->y << 16) | surface->x;
	write_reg(ump, tmp, BLOCR1 + index*4);

	/* byte/word swapping */
	tmp = read_reg(ump, BSWPR);
	tmp |= BSWPR_MODSEL;
	if (surface->format == V4L2_PIX_FMT_RGB32)
		tmp |= ((0x4 << index*8));
	else if (surface->format == V4L2_PIX_FMT_RGB565)
		tmp |= ((0x6 << index*8));
	else
		tmp |= ((0x7 << index*8));
	write_reg(ump, tmp, BSWPR);

	/* Set alpha value for entire plane, if no alpha data */
	tmp = read_reg(ump, BBLCR0);
	if (!surface->pa)
		tmp |= ((surface->alpha & 0xFF) << index*8);
	else
		tmp |= (1 << index+28);
	write_reg(ump, tmp, BBLCR0);

	return 0;
}

/* Setup output surface */
/* The dest size is defined by input surface 1. The output can be on a larger
   canvas by setting the pitch */
static int
setup_dst_surface(struct uio_map *ump, beu_surface_t *dest)
{
	unsigned long tmp;
	unsigned long fmt_reg;

	if (!dest)
		return -1;

#ifdef DEBUG
	fprintf(stderr, "\ndest: fmt=%d: pitch=%lu\n", dest->format, dest->pitch);
	fprintf(stderr, "\tY/RGB (0x%lX), C (0x%lX)\n", dest->py, dest->pc);
#endif

	if ((dest->pitch % 4) || (dest->pitch > 4092))
		return -1;

	/* BDMWR (pitch) is in bytes */
	tmp = dest->pitch;
	if (dest->format == V4L2_PIX_FMT_RGB565)
		tmp *= 2;
	else if (dest->format == V4L2_PIX_FMT_RGB32)
		tmp *= 4;
	write_reg(ump, tmp, BDMWR);

	write_reg(ump, dest->py, BDAYR);
	write_reg(ump, dest->pc, BDACR);
	write_reg(ump, 0, BAFXR);

	/* Surface format */
	if (dest->format == V4L2_PIX_FMT_NV12)
		fmt_reg = CHRR_YCBCR_420;
	else if (dest->format == V4L2_PIX_FMT_NV16)
		fmt_reg = CHRR_YCBCR_422;
	else if (dest->format == V4L2_PIX_FMT_RGB565)
		fmt_reg = WPCK_RGB16;
	else if (dest->format == V4L2_PIX_FMT_RGB32)
		fmt_reg = WPCK_RGB32;
	else
		return -1;
	write_reg(ump, fmt_reg, BPKFR);

	/* byte/word swapping */
	tmp = read_reg(ump, BSWPR);
	if (dest->format == V4L2_PIX_FMT_RGB32)
		tmp |= 0x40;
	else if (dest->format == V4L2_PIX_FMT_RGB565)
		tmp |= 0x60;
	else
		tmp |= 0x70;
	write_reg(ump, tmp, BSWPR);

#ifdef DEBUG
	fprintf(stderr, "\n");
#endif

	return 0;
}

int
shbeu_start_blend(
	SHBEU *pvt,
	beu_surface_t *src1,
	beu_surface_t *src2,
	beu_surface_t *src3,
	beu_surface_t *dest)
{
	struct uio_map *ump = &pvt->uio_mmio;
	unsigned long start_reg;
	unsigned long control_reg;
	beu_surface_t *src_check = src1;
	unsigned long bblcr1 = 0;
	unsigned long bblcr0 = 0;

	debug_info("in");

	/* Check we have been passed at least an input and an output */
	if (!pvt || !src1 || !dest)
		return -1;

	/* Ensure src2 and src3 formats are the same type (only input 1 on the
	   hardware has colorspace conversion */
	if (src2 && src3) {
		if (different_colorspace(src2->format, src3->format)) {
			if (different_colorspace(src1->format, src2->format)) {
				/* src2 is the odd one out, swap 1 and 2 */
				beu_surface_t *tmp = src2;
				src2 = src1;
				src1 = tmp;
				bblcr1 = (1 << 24);
				bblcr0 = (2 << 24);
			} else {
				/* src3 is the odd one out, swap 1 and 3 */
				beu_surface_t *tmp = src3;
				src3 = src1;
				src1 = tmp;
				bblcr1 = (2 << 24);
				bblcr0 = (5 << 24);
			}
		}
	}

	uiomux_lock (pvt->uiomux, UIOMUX_SH_BEU);

	if (read_reg(ump, BSTAR)) {
		debug_info("BEU appears to be running already...");
	}

	/* Reset */
	write_reg(ump, 1, BBRSTR);

	/* Wait for BEU to stop */
	while (read_reg(ump, BSTAR) & 1)
		;

	/* Turn off register bank/plane access, access regs via Plane A */
	write_reg(ump, 0, BRCNTR);
	write_reg(ump, 0, BRCHR);

	/* Default location of surfaces is (0,0) */
	write_reg(ump, 0, BLOCR1);

	/* Default to no byte swapping for all surfaces (YCbCr) */
	write_reg(ump, 0, BSWPR);

	/* Turn off transparent color comparison */
	write_reg(ump, 0, BPCCR0);

	/* Turn on blending */
	write_reg(ump, 0, BPROCR);

	/* Not using "multi-window" capability */
	write_reg(ump, 0, BMWCR0);

	/* Set parent surface; output to memory */
	write_reg(ump, bblcr1 | BBLCR1_OUTPUT_MEM, BBLCR1);

	/* Set surface order */
	write_reg(ump, bblcr0, BBLCR0);

	if (setup_src_surface(ump, 0, src1) < 0)
		goto err;
	if (setup_src_surface(ump, 1, src2) < 0)
		goto err;
	if (setup_src_surface(ump, 2, src3) < 0)
		goto err;
	if (setup_dst_surface(ump, dest) < 0)
		goto err;

	if (src2) {
		if (different_colorspace(src1->format, src2->format)) {
			unsigned long bsifr = read_reg(ump, BSIFR + SRC1_BASE);
			debug_info("Setting BSIFR1 IN1TE bit");
			bsifr  |= (BSIFR1_IN1TE | BSIFR1_IN1TM);
			write_reg(ump, bsifr, BSIFR + SRC1_BASE);
		}

		src_check = src2;
	}

	/* Is input 1 colourspace (after the colorspace convertor) RGB? */
	if (is_rgb(src_check->format)) {
		unsigned long bpkfr = read_reg(ump, BPKFR);
		debug_info("Setting BPKFR RY bit");
		bpkfr |= BPKFR_RY;
		write_reg(ump, bpkfr, BPKFR);
	}

	/* Is the output colourspace different to input? */
	if (different_colorspace(dest->format, src_check->format)) {
		unsigned long bpkfr = read_reg(ump, BPKFR);
		debug_info("Setting BPKFR TE bit");
		bpkfr |= (BPKFR_TM2 | BPKFR_TM | BPKFR_DITH1 | BPKFR_TE);
		write_reg(ump, bpkfr, BPKFR);
	}

	/* enable interrupt */
	write_reg(ump, 1, BEIER);

	/* start operation */
	start_reg = BESTR_BEIVK;
	if (src1) start_reg |= BESTR_CHON1;
	if (src2) start_reg |= BESTR_CHON2;
	if (src3) start_reg |= BESTR_CHON3;
	write_reg(ump, start_reg, BESTR);

	debug_info("out");

	return 0;

err:
	uiomux_unlock(pvt->uiomux, UIOMUX_SH_BEU);
	return -1;
}

void
shbeu_wait(SHBEU *pvt)
{
	debug_info("in");

	uiomux_sleep(pvt->uiomux, UIOMUX_SH_BEU);

	/* Acknowledge interrupt, write 0 to bit 0 */
	write_reg(&pvt->uio_mmio, 0x100, BEVTR);

	/* Wait for BEU to stop */
	while (read_reg(&pvt->uio_mmio, BSTAR) & 1)
		;

	uiomux_unlock(pvt->uiomux, UIOMUX_SH_BEU);

	debug_info("out");
}


int
shbeu_blend(
	SHBEU *pvt,
	beu_surface_t *src1,
	beu_surface_t *src2,
	beu_surface_t *src3,
	beu_surface_t *dest)
{
	int ret = 0;

	ret = shbeu_start_blend(pvt, src1, src2, src3, dest);

	if (ret == 0)
		shbeu_wait(pvt);

	return ret;
}

