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
	struct shbeu_surface src1_hw;
	struct shbeu_surface src2_hw;
	struct shbeu_surface src3_hw;
	struct shbeu_surface dest_hw;
	struct shbeu_surface src1_user;
	struct shbeu_surface src2_user;
	struct shbeu_surface src3_user;
	struct shbeu_surface dest_user;
	struct shbeu_surface *p_src1_user;
	struct shbeu_surface *p_src2_user;
	struct shbeu_surface *p_src3_user;
	struct shbeu_surface *p_dest_user;
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

static void copy_plane(void *dst, void *src, int bpp, int h, int len, int dst_pitch, int src_pitch)
{
	int y;
	if (src && dst != src) {
		for (y=0; y<h; y++) {
			memcpy(dst, src, len * bpp);
			src += src_pitch * bpp;
			dst += dst_pitch * bpp;
		}
	}
}

/* Copy active surface contents - assumes output is big enough */
static void copy_surface(
	struct ren_vid_surface *out,
	const struct ren_vid_surface *in)
{
	const struct format_info *fmt;

	if (in == NULL || out == NULL)
		return;

	fmt = &fmts[in->format];

	copy_plane(out->py, in->py, fmt->y_bpp, in->h, in->w, out->pitch, in->pitch);

	copy_plane(out->pc, in->pc, fmt->c_bpp,
		in->h/fmt->c_ss_vert,
		in->w/fmt->c_ss_horz,
		out->pitch/fmt->c_ss_horz,
		in->pitch/fmt->c_ss_horz);

	copy_plane(out->pa, in->pa, 1, in->h, in->w, out->pitch, in->pitch);
}

/* Check/create surface that can be accessed by the hardware */
static int get_hw_surface(
	UIOMux * uiomux,
	struct shbeu_surface *out_spec,
	const struct shbeu_surface *in_spec)
{
	struct ren_vid_surface *out = &out_spec->s;
	const struct ren_vid_surface *in = &in_spec->s;
	unsigned long phys;
	int y;

	if (in == NULL || out == NULL)
		return 0;

	*out_spec = *in_spec;

	if (in->py) {
		phys = uiomux_all_virt_to_phys(in->py);
		if (!phys) {
			size_t len = size_y(in->format, in->h * in->w);
			/* Supplied buffer is not usable by the hardware! */
			out->py = uiomux_malloc(uiomux, UIOMUX_SH_BEU, len, 32);
			if (!out->py)
				return -1;
		}
	}

	if (in->pc) {
		phys = uiomux_all_virt_to_phys(in->pc);
		if (!phys) {
			size_t len = size_c(in->format, in->h * in->w);
			/* Supplied buffer is not usable by the hardware! */
			out->pc = uiomux_malloc(uiomux, UIOMUX_SH_BEU, len, 32);
			if (!out->pc)
				return -1;
		}
	}

	if (in->pa) {
		phys = uiomux_all_virt_to_phys(in->pa);
		if (!phys) {
			size_t len = size_a(in->format, in->h * in->w);
			/* Supplied buffer is not usable by the hardware! */
			out->pa = uiomux_malloc(uiomux, UIOMUX_SH_BEU, len, 32);
			if (!out->pa)
				return -1;
		}
	}

	return 0;
}

static void free_temp_buf(SHBEU *beu, struct ren_vid_surface *user, struct ren_vid_surface *hw)
{
	if (user == NULL || hw == NULL)
		return;

	if (hw->py && hw->py != user->py) {
		size_t len = size_y(hw->format, hw->h * hw->w);
		uiomux_free(beu->uiomux, UIOMUX_SH_BEU, hw->py, len);
	}
	if (hw->pc && hw->pc != user->pc) {
		size_t len = size_c(hw->format, hw->h * hw->w);
		uiomux_free(beu->uiomux, UIOMUX_SH_BEU, hw->pc, len);
	}
	if (hw->pa && hw->pa != user->pa) {
		size_t len = size_a(hw->format, hw->h * hw->w);
		uiomux_free(beu->uiomux, UIOMUX_SH_BEU, hw->pa, len);
	}
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
setup_src_surface(SHBEU *beu, struct uio_map *ump, int index, const struct shbeu_surface *spec)
{
	const int offsets[] = {SRC1_BASE, SRC2_BASE, SRC3_BASE};
	int offset = offsets[index];
	unsigned long tmp;
	const struct beu_format_info *info;
	const struct ren_vid_surface *surface = &spec->s;
	unsigned long Y, C, A;

	/* Not having an overlay surface is valid */
	if (!surface)
		return 0;

	info = src_fmt_info(surface->format);
	if (!info)
		return -1;

	Y = uiomux_all_virt_to_phys(surface->py);
	C = uiomux_all_virt_to_phys(surface->pc);
	A = uiomux_all_virt_to_phys(surface->pa);

#ifdef DEBUG
	fprintf(stderr, "\nsrc%d: fmt=%d: width=%d, height=%d pitch=%d\n",
		index+1, surface->format, surface->w, surface->h, surface->pitch);
	fprintf(stderr, "\tY/RGB (0x%lX), C (0x%lX), alpha (0x%lX)\n", Y, C, A);
	fprintf(stderr, "\toffset=(%d,%d), alternative alpha =%u\n", surface->x, surface->y, surface->alpha);
#endif

	if (!Y)
		return -1;

	if ((surface->w % 4) || (surface->pitch % 4) || (surface->h % 4))
		return -1;

	if ((surface->w > 4092) || (surface->pitch > 4092) || (surface->h > 4092))
		return -1;

	if (is_rgb(surface->format) && surface->pa)
		return -1;

	/* Surface pitch */
	tmp = size_y(surface->format, surface->pitch);
	write_reg(ump, tmp, BSMWR + offset);

	write_reg(ump, (surface->h << 16) | surface->w, BSSZR + offset);
	write_reg(ump, Y, BSAYR + offset);
	write_reg(ump, C, BSACR + offset);
	write_reg(ump, A, BSAAR + offset);

	/* Surface format */
	tmp = info->bpXfr;
	if (is_ycbcr(surface->format) && surface->pa)
		tmp += CHRR_YCBCR_ALPHA;
	write_reg(ump, tmp, BSIFR + offset);

	/* Position of overlay */
	tmp = (spec->y << 16) | spec->x;
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
		tmp |= ((spec->alpha & 0xFF) << index*8);
	write_reg(ump, tmp, BBLCR0);

	return 0;
}

/* Setup output surface */
/* The dest size is defined by input surface 1. The output can be on a larger
   canvas by setting the pitch */
static int
setup_dst_surface(SHBEU *beu, struct uio_map *ump, const struct shbeu_surface *spec)
{
	unsigned long tmp;
	const struct beu_format_info *info;
	const struct ren_vid_surface *dest = &spec->s;
	unsigned long Y, C;

	if (!dest)
		return -1;

	info = dst_fmt_info(dest->format);
	if (!info)
		return -1;

	Y = uiomux_all_virt_to_phys(dest->py);
	C = uiomux_all_virt_to_phys(dest->pc);

#ifdef DEBUG
	fprintf(stderr, "\ndest: fmt=%d: pitch=%d\n", dest->format, dest->pitch);
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
	const struct shbeu_surface *src1_in,
	const struct shbeu_surface *src2_in,
	const struct shbeu_surface *src3_in,
	const struct shbeu_surface *dest_in)
{
	struct uio_map *ump = &pvt->uio_mmio;
	unsigned long start_reg;
	unsigned long control_reg;
	const struct shbeu_surface *src_check;
	unsigned long bblcr1 = 0;
	unsigned long bblcr0 = 0;
	struct shbeu_surface *src1;
	struct shbeu_surface *src2;
	struct shbeu_surface *src3;
	struct shbeu_surface *dest;

	debug_info("in");

	src1 = (src1_in != NULL) ? &pvt->src1_hw : NULL;
	src2 = (src2_in != NULL) ? &pvt->src2_hw : NULL;
	src3 = (src3_in != NULL) ? &pvt->src3_hw : NULL;
	dest = (dest_in != NULL) ? &pvt->dest_hw : NULL;

	/* Keep track of the user surfaces */
	pvt->p_src1_user = (src1_in != NULL) ? &pvt->src1_user : NULL;
	pvt->p_src2_user = (src1_in != NULL) ? &pvt->src2_user : NULL;
	pvt->p_src3_user = (src1_in != NULL) ? &pvt->src3_user : NULL;
	pvt->p_dest_user = (src1_in != NULL) ? &pvt->dest_user : NULL;

	if (src1_in) pvt->src1_user = *src1_in;
	if (src2_in) pvt->src2_user = *src2_in;
	if (src3_in) pvt->src3_user = *src3_in;
	if (dest_in) pvt->dest_user = *dest_in;

	/* Check we have been passed at least an input and an output */
	if (!pvt || !src1_in || !dest_in)
		return -1;

	/* Check the size of the destination surface is big enough */
	if (dest_in->s.pitch <= src1_in->s.w)
		return -1;

	/* Check the size of the destination surface matches the parent surface */
	if (dest_in->s.w != src1_in->s.w || dest_in->s.h != src1_in->s.h)
		return -1;

	/* surfaces - use buffers the hardware can access */
	if (get_hw_surface(pvt->uiomux, src1, src1_in) < 0)
		return -1;
	if (get_hw_surface(pvt->uiomux, src2, src2_in) < 0)
		return -1;
	if (get_hw_surface(pvt->uiomux, src3, src3_in) < 0)
		return -1;
	if (get_hw_surface(pvt->uiomux, dest, dest_in) < 0)
		return -1;

	copy_surface(&src1->s, &src1_in->s);
	copy_surface(&src2->s, &src2_in->s);
	copy_surface(&src3->s, &src3_in->s);

	src_check = src1;

	/* Ensure src2 and src3 formats are the same type (only input 1 on the
	   hardware has colorspace conversion */
	if (src2 && src3) {
		if (different_colorspace(src2->s.format, src3->s.format)) {
			if (different_colorspace(src1->s.format, src2->s.format)) {
				/* src2 is the odd one out, swap 1 and 2 */
				struct shbeu_surface *tmp = src2;
				src2 = src1;
				src1 = tmp;
				bblcr1 = (1 << 24);
				bblcr0 = (2 << 24);
			} else {
				/* src3 is the odd one out, swap 1 and 3 */
				struct shbeu_surface *tmp = src3;
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
		if (different_colorspace(src1->s.format, src2->s.format)) {
			unsigned long bsifr = read_reg(ump, BSIFR + SRC1_BASE);
			debug_info("Setting BSIFR1 IN1TE bit");
			bsifr  |= (BSIFR1_IN1TE | BSIFR1_IN1TM);
			write_reg(ump, bsifr, BSIFR + SRC1_BASE);
		}

		src_check = src2;
	}

	/* Is input 1 colourspace (after the colorspace convertor) RGB? */
	if (is_rgb(src_check->s.format)) {
		unsigned long bpkfr = read_reg(ump, BPKFR);
		debug_info("Setting BPKFR RY bit");
		bpkfr |= BPKFR_RY;
		write_reg(ump, bpkfr, BPKFR);
	}

	/* Is the output colourspace different to input? */
	if (different_colorspace(dest->s.format, src_check->s.format)) {
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

	/* If we had to allocate hardware output buffer, copy the contents */
	copy_surface(&pvt->dest_user.s, &pvt->dest_hw.s);

	/* Free any temporary hardware buffers */
	free_temp_buf(pvt, &pvt->p_dest_user->s, &pvt->dest_hw.s);
	free_temp_buf(pvt, &pvt->p_src3_user->s, &pvt->src3_hw.s);
	free_temp_buf(pvt, &pvt->p_src2_user->s, &pvt->src2_hw.s);
	free_temp_buf(pvt, &pvt->p_src1_user->s, &pvt->src1_hw.s);

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

