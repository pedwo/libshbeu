/*
 * libshbeu: A library for controlling SH-Mobile BEU
 * Copyright (C) 2010 Renesas Technology Corp.
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

struct uio_map {
	unsigned long address;
	unsigned long size;
	void *iomem;
};

struct SHBEU {
	UIOMux *uiomux;
	struct uio_map uio_mmio;
	struct uio_map uio_mem;
};


/* Helper functions for reading registers. */

static unsigned long read_reg(struct uio_map *ump, int reg_nr)
{
	volatile unsigned long *reg = ump->iomem + reg_nr;

	return *reg;
}

static void write_reg(struct uio_map *ump, unsigned long value, int reg_nr)
{
	volatile unsigned long *reg = ump->iomem + reg_nr;

	*reg = value;
}

void shbeu_close(SHBEU *pvt)
{
	if (pvt) {
		if (pvt->uiomux)
			uiomux_close(pvt->uiomux);
		free(pvt);
	}
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

	ret = uiomux_get_mem (beu->uiomux, UIOMUX_SH_BEU,
		&beu->uio_mem.address,
		&beu->uio_mem.size,
		&beu->uio_mem.iomem);
	if (!ret)
		goto err;

	return beu;

err:
	shbeu_close(beu);
	return 0;
}

int
shbeu_start_blend(
	SHBEU *pvt,
	unsigned long src_py,
	unsigned long src_pc,
	unsigned long src_width,
	unsigned long src_height,
	unsigned long src_pitch,
	int src_fmt,
	unsigned long dst_py,
	unsigned long dst_pc,
	unsigned long dst_width,
	unsigned long dst_height,
	unsigned long dst_pitch,
	int dst_fmt)
{
	struct uio_map *ump = &pvt->uio_mmio;

#ifdef DEBUG
	fprintf(stderr, "%s IN\n", __FUNCTION__);
	fprintf(stderr, "src_fmt=%d: src_width=%lu, src_height=%lu src_pitch=%lu\n",
		src_fmt, src_width, src_height, src_pitch);
	fprintf(stderr, "dst_fmt=%d: dst_width=%lu, dst_height=%lu dst_pitch=%lu\n",
		dst_fmt, dst_width, dst_height, dst_pitch);
	fprintf(stderr, "rotate=%d\n", rotate);
#endif

	// TODO

#ifdef DEBUG
	fprintf(stderr, "%s OUT\n", __FUNCTION__);
#endif

	return 0;
}

void
shbeu_wait(SHBEU *pvt)
{
	uiomux_sleep(pvt->uiomux, UIOMUX_SH_BEU);

	// TODO

	uiomux_unlock(pvt->uiomux, UIOMUX_SH_BEU);
}

int
shbeu_blend(
	SHBEU *beu,
	unsigned long src_py,
	unsigned long src_pc,
	unsigned long src_width,
	unsigned long src_height,
	int src_fmt,
	unsigned long dst_py,
	unsigned long dst_pc,
	unsigned long dst_width,
	unsigned long dst_height,
	int dst_fmt)
{
	int ret = 0;

	ret = shbeu_start_blend(
		beu,
		src_py, src_pc, src_width, src_height, src_width, src_fmt,
		dst_py, dst_pc, dst_width, dst_height, dst_width, dst_fmt);

	if (ret == 0)
		shbeu_wait(beu);

	return ret;
}

