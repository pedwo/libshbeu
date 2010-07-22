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

	return beu;

err:
	shbeu_close(beu);
	return 0;
}

void disp_surface(const char * s, beu_surface_t *surface)
{
	fprintf(stderr, "%s: fmt=%d: width=%lu, height=%lu pitch=%lu\n",
		s, surface->format, surface->width, surface->height, surface->pitch);
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

#ifdef DEBUG
	fprintf(stderr, "%s IN\n", __func__);
	disp_surface("src1", src1);
	disp_surface("src2", src2);
	disp_surface("src3", src3);
	disp_surface("dest", dest);
#endif

	// TODO

#ifdef DEBUG
	fprintf(stderr, "%s OUT\n", __func__);
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

