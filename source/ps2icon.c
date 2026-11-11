#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "ps2render.h"


static uint16_t read_le16(const uint8_t *buf)
{
	return (uint16_t)(buf[0] | (buf[1] << 8));
}

// TIM2 16-bit texel (A1B5G5R5) to R,G,B,A bytes
static uint32_t TIM2RGBA(const uint8_t *buf)
{
	uint8_t RGBA[4];
	uint16_t lRGB = read_le16(buf);

	RGBA[0] = 8 * (lRGB & 0x1F);
	RGBA[1] = 8 * ((lRGB >> 5) & 0x1F);
	RGBA[2] = 8 * (lRGB >> 10);
	RGBA[3] = 0xFF;

	return *((uint32_t *) &RGBA);
}

//Bytes still readable at 'off' in a buffer of 'len' bytes
#define ICON_AVAIL(len, off)	(((off) < (len)) ? ((len) - (off)) : 0)

//Texels the output buffer can still take
#define ICON_TEXELS		(128 * 128)

void ps2icon_free(ps2icon_t *icon)
{
	if (!icon)
		return;

	free(icon->shapes);
	free(icon->normals);
	free(icon->uvs);
	free(icon->colors);
	free(icon->texture);
	memset(icon, 0, sizeof(*icon));
}

/* 12.4 fixed point, as the file stores every coordinate. */
#define ICON_F16(v)  ((float)(int16_t)(v) / 4096.0f)

int ps2icon_parse(const uint8_t* iData, size_t len, ps2icon_t *out)
{
	uint32_t i;
	uint16_t j;
	Icon_Header header;
	Animation_Header anim_header;
	Frame_Data animation;
	uint32_t *lTexturePtr, *lRGBA;
	size_t offset = 0, vertex_size, geom = 0;
	int s_i, v_i, ok = -1;

	if (!out)
		return -1;

	memset(out, 0, sizeof(*out));

	lTexturePtr = (uint32_t *) calloc(ICON_TEXELS, sizeof(uint32_t));
	if (!lTexturePtr)
		return -1;

	out->texture = lTexturePtr;

	//read header:
	if (len < sizeof(Icon_Header))
		return ok;

	memcpy(&header, iData, sizeof(Icon_Header));
	offset += sizeof(Icon_Header);

	//n_vertices has to be divisible by three, that's for sure:
	if(header.file_id != 0x010000 || header.n_vertices % 3 != 0)
		return ok;

	//a vertex needs at least one animation shape, and the count is bounded so
	//the size calculation below cannot overflow
	if(header.animation_shapes == 0 || header.animation_shapes > 0xFFFF)
		return ok;

	//read icon data from file: https://ghulbus-inc.de/projects/ps2iconsys/
	///Vertex data
	// each vertex consists of animation_shapes tuples for vertex coordinates,
	// followed by one vertex coordinate tuple for normal coordinates
	// followed by one texture data tuple for texture coordinates and color
	vertex_size = (size_t)sizeof(Vertex_Coord) * header.animation_shapes
			+ sizeof(Vertex_Coord) + sizeof(Texture_Data);

	//divide instead of multiplying out, so the check cannot overflow
	if (header.n_vertices > ICON_AVAIL(len, offset) / vertex_size)
		return ok;

	//pull the geometry out on the way past. A renderer needs every shape
	//(they are morph targets), the normals, and the texture coordinates and
	//colour each vertex carries.
	out->shape_count = (int)header.animation_shapes;
	out->vertex_count = (int)header.n_vertices;
	out->shapes = malloc(sizeof(float) * 3 * header.animation_shapes * header.n_vertices);
	out->normals = malloc(sizeof(float) * 3 * header.n_vertices);
	out->uvs = malloc(sizeof(float) * 2 * header.n_vertices);
	out->colors = malloc(4 * (size_t)header.n_vertices);

	if (!out->shapes || !out->normals || !out->uvs || !out->colors) {
		ps2icon_free(out);
		return -1;
	}

	geom = offset;
	for (v_i = 0; v_i < out->vertex_count; v_i++) {
		for (s_i = 0; s_i < out->shape_count; s_i++) {
			const uint8_t *p = &iData[geom];
			float *dst = &out->shapes[((size_t)s_i * out->vertex_count + v_i) * 3];

			dst[0] = ICON_F16(read_le16(&p[0]));
			dst[1] = ICON_F16(read_le16(&p[2]));
			dst[2] = ICON_F16(read_le16(&p[4]));
			geom += sizeof(Vertex_Coord);
		}

		out->normals[v_i * 3 + 0] = ICON_F16(read_le16(&iData[geom + 0]));
		out->normals[v_i * 3 + 1] = ICON_F16(read_le16(&iData[geom + 2]));
		out->normals[v_i * 3 + 2] = ICON_F16(read_le16(&iData[geom + 4]));
		geom += sizeof(Vertex_Coord);

		out->uvs[v_i * 2 + 0] = ICON_F16(read_le16(&iData[geom + 0]));
		out->uvs[v_i * 2 + 1] = ICON_F16(read_le16(&iData[geom + 2]));
		memcpy(&out->colors[v_i * 4], &iData[geom + 4], 4);
		geom += sizeof(Texture_Data);
	}

	offset += vertex_size * header.n_vertices;

	//animation data
	// preceeded by an animation header, there is a frame data/key set for every frame:
	if (ICON_AVAIL(len, offset) < sizeof(Animation_Header))
		return ok;

	memcpy(&anim_header, &iData[offset], sizeof(Animation_Header));
	offset += sizeof(Animation_Header);

	//read animation data:
	for(i=0; i<anim_header.n_frames; i++) {
		if (ICON_AVAIL(len, offset) < sizeof(Frame_Data))
			return ok;

		memcpy(&animation, &iData[offset], sizeof(Frame_Data));
		offset += sizeof(Frame_Data);

		/* The still the web thumbnailer draws is the first frame's shape, but
		 * only when there is a sequence to interpolate: with fewer than two
		 * frames it falls back to shape 0. Matching that keeps the two
		 * renderers showing the same pose. */
		if (i == 0 && anim_header.n_frames >= 2)
			out->still_shape = (int)animation.shape_id < out->shape_count
					 ? (int)animation.shape_id : out->shape_count - 1;

		if (animation.n_keys > ICON_AVAIL(len, offset) / sizeof(Frame_Key))
			return ok;

		offset += sizeof(Frame_Key) * animation.n_keys;
	}

	//everything the renderer needs has been read; the texture is a bonus
	ok = 0;

	lRGBA = lTexturePtr;

	if (header.texture_type <= 7)
	{	// Uncompressed texture
		// Some icons carry no texture at all: the file ends after the animation
		// block and the model is drawn from its vertex colours. Hand back the
		// cleared buffer rather than reading past the end of the file.
		if (ICON_AVAIL(len, offset) < (ICON_TEXELS * 2))
			return ok;

		for (i = 0; i < ICON_TEXELS; i++, offset += 2)
			*lRGBA++ = TIM2RGBA(&iData[offset]);
	}
	else
	{	//Compressed texture
		offset += 4;

		while ((lRGBA - lTexturePtr) < ICON_TEXELS)
		{
			if (ICON_AVAIL(len, offset) < 2)
				break;

			j = read_le16(&iData[offset]);

			if (j & 0x8000)
			{	//a run of literal texels: 0x10000 - j of them, so up to 32768
				for (j = (0x0000 - j) & 0xFFFF; j > 0; j--)
				{
					offset += 2;

					if (ICON_AVAIL(len, offset) < 2 || (lRGBA - lTexturePtr) >= ICON_TEXELS)
						break;

					*lRGBA++ = TIM2RGBA(&iData[offset]);
				}
			}
			else
			{	//one texel repeated j times
				offset += 2;

				if (ICON_AVAIL(len, offset) < 2)
					break;

				for (; j > 0; j--)
				{
					if ((lRGBA - lTexturePtr) >= ICON_TEXELS)
						break;

					*lRGBA++ = TIM2RGBA(&iData[offset]);
				}
			}
			offset += 2;
		}
	}

	/* Only a full 128x128 counts. A truncated RLE stream leaves the tail of
	 * the buffer zeroed, and handing that to the renderer as a texture draws
	 * the model black; the JavaScript parser reports the same case as
	 * textureless so the model falls back to its vertex colours. */
	out->has_texture = (lRGBA - lTexturePtr) == ICON_TEXELS;

	return ok;
}

// 2x supersampling is plenty at this size
#define ICON_SUPERSAMPLE	2

uint8_t* ps2IconRender(const uint8_t *data, size_t len, const void *iconsys)
{
	ps2icon_t icon = { 0 };
	uint8_t *out = NULL;
	mcIcon sysbuf;
	const mcIcon *sys = NULL;

	if (!data || ps2icon_parse(data, len, &icon) < 0)
	{
		ps2icon_free(&icon);
		return NULL;
	}

	// copy rather than cast: the caller's bytes may not be aligned for the
	// floats inside, and an unaligned float load faults on the EE.
	// Only trust icon.sys lighting that carries the right magic.
	if (iconsys)
	{
		memcpy(&sysbuf, iconsys, sizeof(sysbuf));
		if (memcmp(sysbuf.head, "PS2D", 4) == 0)
			sys = &sysbuf;
	}

	if (ps2icon_render(&icon, sys, PS2ICON_SIZE, ICON_SUPERSAMPLE, PS2RENDER_BG_TRANSPARENT, &out) < 0)
		out = NULL;

	ps2icon_free(&icon);
	return out;
}

_Static_assert(sizeof(mcIcon) == PS2ICON_SYS_SIZE, "icon.sys size");
