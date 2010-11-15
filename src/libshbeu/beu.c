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

#include <string.h>
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

struct phys_buf {
	void *virt;
	size_t len;
};

struct beu_allocated_bufs {
	struct phys_buf Y;
	struct phys_buf C;
	struct phys_buf A;
};

struct beu_format_info {
	ren_vid_format_t fmt;
	unsigned long bpXfr;
	unsigned long bswpr;
};

static const struct beu_format_info beu_src_fmts[] = {
	{ REN_NV12,   CHRR_YCBCR_420, 7 },
	{ REN_NV16,   CHRR_YCBCR_422, 7 },
	{ REN_RGB565, RPKF_RGB16,     6 },
	{ REN_RGB24,  RPKF_RGB24,     7 },
	{ REN_BGR24,  RPKF_BGR24,     7 },
	{ REN_RGB32,  RPKF_RGB32,     4 },
	{ REN_ARGB32, RPKF_RGB32,     4 },
};

static const struct beu_format_info beu_dst_fmts[] = {
	{ REN_NV12,   CHRR_YCBCR_420, 7 },
	{ REN_NV16,   CHRR_YCBCR_422, 7 },
	{ REN_RGB565, WPCK_RGB16,     6 },
	{ REN_RGB24,  WPCK_RGB24,     7 },
	{ REN_RGB32,  WPCK_RGB32,     4 },
};


struct uio_map {
	unsigned long address;
	unsigned long size;
	void *iomem;
};

struct SHBEU {
	UIOMux *uiomux;
	struct uio_map uio_mmio;
	struct beu_allocated_bufs src[3];
	struct beu_allocated_bufs dest;
};


static const struct beu_format_info *src_fmt_info(ren_vid_format_t format)
{
	int i, nr_fmts;

	nr_fmts = sizeof(beu_src_fmts) / sizeof(beu_src_fmts[0]);
	for (i=0; i<nr_fmts; i++) {
		if (beu_src_fmts[i].fmt == format)
			return &beu_src_fmts[i];
	}
	return NULL;
}

static const struct beu_format_info *dst_fmt_info(ren_vid_format_t format)
{
	int i, nr_fmts;

	nr_fmts = sizeof(beu_dst_fmts) / sizeof(beu_dst_fmts[0]);
	for (i=0; i<nr_fmts; i++) {
		if (beu_dst_fmts[i].fmt == format)
			return &beu_dst_fmts[i];
	}
	return NULL;
}

/* Tests a virtual buffer to see if it can be used with the HW (i.e. physically contiguous)
 and if not, allocate a new buffer that can be used and optionally copy the data over */
static int into_physical_buf(SHBEU *beu, void *virt, size_t len, unsigned long *phys, struct phys_buf *new_buf, int copy)
{
	int ret=0;
	*phys = 0;
	if (virt) {
		*phys = uiomux_all_virt_to_phys(virt);
		if (!*phys) {
			/* Supplied buffer is not usable by BEU! */
			new_buf->virt = uiomux_malloc(beu->uiomux, UIOMUX_SH_BEU, len, 32);
			if (!new_buf->virt) {
				return -1;
			}
			new_buf->len = len;
			*phys = uiomux_all_virt_to_phys(new_buf->virt);
			if (copy) {
				memcpy(new_buf->virt, virt, len);
			}
		}
	}
	return ret;
}

static void free_temp_buf(SHBEU *beu, struct beu_allocated_bufs *buf)
{
	if (buf->Y.virt)
		uiomux_free(beu->uiomux, UIOMUX_SH_BEU, buf->Y.virt, buf->Y.len);
	if (buf->C.virt)
		uiomux_free(beu->uiomux, UIOMUX_SH_BEU, buf->C.virt, buf->C.len);
	if (buf->A.virt)
		uiomux_free(beu->uiomux, UIOMUX_SH_BEU, buf->A.virt, buf->A.len);
}


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

	beu->src[0].Y.virt = 0;
	beu->src[0].C.virt = 0;
	beu->src[0].A.virt = 0;
	beu->src[1].Y.virt = 0;
	beu->src[1].C.virt = 0;
	beu->src[1].A.virt = 0;
	beu->src[2].Y.virt = 0;
	beu->src[2].C.virt = 0;
	beu->src[2].A.virt = 0;
	beu->dest.Y.virt = 0;
	beu->dest.C.virt = 0;
	beu->dest.A.virt = 0;

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

/* Setup input surface */
static int
setup_src_surface(SHBEU *beu, struct uio_map *ump, int index, const struct shbeu_surface *surface)
{
	const int offsets[] = {SRC1_BASE, SRC2_BASE, SRC3_BASE};
	int offset = offsets[index];
	unsigned long tmp;
	const struct beu_format_info *info;
	unsigned long Y, C, A;

	/* Not having an overlay surface is valid */
	if (!surface)
		return 0;

	info = src_fmt_info(surface->format);
	if (!info)
		return -1;

	/* if supplied buffers are not usable by the BEU, copy them to ones that are */
	if (into_physical_buf(beu, surface->py, size_y(surface->format, surface->height * surface->pitch), &Y, &beu->src[index].Y, 1) < 0)
		return -1;
	if (into_physical_buf(beu, surface->pc, size_c(surface->format, surface->height * surface->pitch), &C, &beu->src[index].C, 1) < 0)
		return -1;
	if (into_physical_buf(beu, surface->pa, surface->height * surface->pitch,        &A, &beu->src[index].A, 1) < 0)
		return -1;

#ifdef DEBUG
	fprintf(stderr, "\nsrc%d: fmt=%d: width=%lu, height=%lu pitch=%lu\n",
		index+1, surface->format, surface->width, surface->height, surface->pitch);
	fprintf(stderr, "\tY/RGB (0x%lX), C (0x%lX), alpha (0x%lX)\n", Y, C, A);
	fprintf(stderr, "\toffset=(%lu,%lu), alternative alpha =%lu\n", surface->x, surface->y, surface->alpha);
#endif

	if (!Y)
		return -1;

	if ((surface->width % 4) || (surface->pitch % 4) || (surface->height % 4))
		return -1;

	if ((surface->width > 4092) || (surface->pitch > 4092) || (surface->height > 4092))
		return -1;

	if (is_rgb(surface->format) && surface->pa)
		return -1;

	/* Surface pitch */
	tmp = size_y(surface->format, surface->pitch);
	write_reg(ump, tmp, BSMWR + offset);

	write_reg(ump, (surface->height << 16) | surface->width, BSSZR + offset);
	write_reg(ump, Y, BSAYR + offset);
	write_reg(ump, C, BSACR + offset);
	write_reg(ump, A, BSAAR + offset);

	/* Surface format */
	tmp = info->bpXfr;
	if (is_ycbcr(surface->format) && surface->pa)
		tmp += CHRR_YCBCR_ALPHA;
	write_reg(ump, tmp, BSIFR + offset);

	/* Position of overlay */
	tmp = (surface->y << 16) | surface->x;
	write_reg(ump, tmp, BLOCR1 + index*4);

#ifdef __LITTLE_ENDIAN__
	/* byte/word swapping */
	tmp = read_reg(ump, BSWPR);
	tmp |= BSWPR_MODSEL;
	tmp |= (info->bswpr << index*8);
	write_reg(ump, tmp, BSWPR);
#endif

	/* Set alpha value for entire plane, if no alpha data */
	tmp = read_reg(ump, BBLCR0);
	if (surface->pa || surface->format == REN_ARGB32)
		tmp |= (1 << (index+28));
	else
		tmp |= ((surface->alpha & 0xFF) << index*8);
	write_reg(ump, tmp, BBLCR0);

	return 0;
}

/* Setup output surface */
/* The dest size is defined by input surface 1. The output can be on a larger
   canvas by setting the pitch */
static int
setup_dst_surface(SHBEU *beu, struct uio_map *ump, const struct shbeu_surface *dest)
{
	unsigned long tmp;
	const struct beu_format_info *info;
	unsigned long Y, C;

	if (!dest)
		return -1;

	info = dst_fmt_info(dest->format);
	if (!info)
		return -1;

	/* if supplied buffer(s) are not usable by the BEU, allocate ones that are */
	if (into_physical_buf(beu, dest->py, size_y(dest->format, dest->height * dest->pitch), &Y, &beu->dest.Y, 0) < 0)
		return -1;
	if (into_physical_buf(beu, dest->pc, size_c(dest->format, dest->height * dest->pitch), &C, &beu->dest.C, 0) < 0)
		return -1;

#ifdef DEBUG
	fprintf(stderr, "\ndest: fmt=%d: pitch=%lu\n", dest->format, dest->pitch);
	fprintf(stderr, "\tY/RGB (0x%lX), C (0x%lX)\n", Y, C);
#endif

	if (!dest->py)
		return -1;

	if ((dest->pitch % 4) || (dest->pitch > 4092))
		return -1;

	/* Surface pitch */
	tmp = size_y(dest->format, dest->pitch);
	write_reg(ump, tmp, BDMWR);

	write_reg(ump, Y, BDAYR);
	write_reg(ump, C, BDACR);
	write_reg(ump, 0, BAFXR);

	/* Surface format */
	write_reg(ump, info->bpXfr, BPKFR);

#ifdef __LITTLE_ENDIAN__
	/* byte/word swapping */
	tmp = read_reg(ump, BSWPR);
	tmp |= info->bswpr << 4;
	write_reg(ump, tmp, BSWPR);
#endif

	return 0;
}

int
shbeu_start_blend(
	SHBEU *pvt,
	const struct shbeu_surface *src1,
	const struct shbeu_surface *src2,
	const struct shbeu_surface *src3,
	const struct shbeu_surface *dest)
{
	struct uio_map *ump = &pvt->uio_mmio;
	unsigned long start_reg;
	unsigned long control_reg;
	const struct shbeu_surface *src_check = src1;
	unsigned long bblcr1 = 0;
	unsigned long bblcr0 = 0;

	debug_info("in");

	/* Check we have been passed at least an input and an output */
	if (!pvt || !src1 || !dest)
		return -1;

	/* Check the size of the destination surface is big enough */
	if (dest->pitch < src1->width)
		return -1;

	/* Ensure src2 and src3 formats are the same type (only input 1 on the
	   hardware has colorspace conversion */
	if (src2 && src3) {
		if (different_colorspace(src2->format, src3->format)) {
			if (different_colorspace(src1->format, src2->format)) {
				/* src2 is the odd one out, swap 1 and 2 */
				const struct shbeu_surface *tmp = src2;
				src2 = src1;
				src1 = tmp;
				bblcr1 = (1 << 24);
				bblcr0 = (2 << 24);
			} else {
				/* src3 is the odd one out, swap 1 and 3 */
				const struct shbeu_surface *tmp = src3;
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

	if (setup_src_surface(pvt, ump, 0, src1) < 0)
		goto err;
	if (setup_src_surface(pvt, ump, 1, src2) < 0)
		goto err;
	if (setup_src_surface(pvt, ump, 2, src3) < 0)
		goto err;
	if (setup_dst_surface(pvt, ump, dest) < 0)
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

	/* Free any temporary BEU buffers */
	free_temp_buf(pvt, &pvt->src[0]);
	free_temp_buf(pvt, &pvt->src[1]);
	free_temp_buf(pvt, &pvt->src[2]);
	free_temp_buf(pvt, &pvt->dest);

	uiomux_unlock(pvt->uiomux, UIOMUX_SH_BEU);

	debug_info("out");
}


int
shbeu_blend(
	SHBEU *pvt,
	const struct shbeu_surface *src1,
	const struct shbeu_surface *src2,
	const struct shbeu_surface *src3,
	const struct shbeu_surface *dest)
{
	int ret = 0;

	ret = shbeu_start_blend(pvt, src1, src2, src3, dest);

	if (ret == 0)
		shbeu_wait(pvt);

	return ret;
}

