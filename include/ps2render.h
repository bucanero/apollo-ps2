/*
 * ps2render.h - software renderer for PS2 save icons
 *
 * Ported from Apollo PS4 (PR #280), which mirrors the WebGL renderer
 * (web-ps2/icon3d.js): it rasterises the model with the same camera, lighting
 * and shading, and hands back an RGBA image (R,G,B,A bytes per pixel).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef PS2RENDER_H
#define PS2RENDER_H

#include <stdint.h>
#include <stddef.h>

#include <libmc.h>

#include "ps2icon.h"

/* What goes behind the model. */
enum ps2render_bg {
	PS2RENDER_BG_TRANSPARENT = 0,   /* alpha 0, so the PNG composites anywhere */
	PS2RENDER_BG_GRADIENT           /* the four icon.sys corner colours */
};

/*
 * Render a still frame of `icon` into a `size` x `size` RGBA buffer.
 *
 * `sys` supplies the three directional lights, the ambient term and (for
 * PS2RENDER_BG_GRADIENT) the background corners; pass NULL to use the same
 * fallback lighting the web renderer uses for a save with no readable
 * icon.sys. `supersample` is the factor each axis is rendered at before being
 * averaged down - 1 disables it, 4 is plenty.
 *
 * `*out` is malloc'd and owned by the caller. Returns 0, or negative on bad
 * arguments or an allocation failure.
 */
int ps2icon_render(const ps2icon_t *icon, const mcIcon *sys,
		   int size, int supersample, enum ps2render_bg bg,
		   uint8_t **out);

#endif
