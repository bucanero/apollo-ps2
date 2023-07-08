/*
* ps2icon 0.7.0 - PS2 savegame icon library
*
* Copyright (c) 2002 Martin Akesson <ma@placid.tv>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy  of  this   software  and  associated   documentation  files  (the
* "Software"),  to deal  in the Software  without  restriction, including
* without  limitation  the rights to  use, copy,  modify, merge, publish,
* distribute,  sublicense, and/or  sell  copies of the  Software, and  to
* permit persons to  whom the Software is furnished  to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR   IMPLIED,  INCLUDING   BUT  NOT  LIMITED  TO   THE  WARRANTIES   OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR  PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE  AUTHORS OR COPYRIGHT HOLDERS  BE LIABLE FOR  ANY
* CLAIM,  DAMAGES  OR OTHER  LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT  OR OTHERWISE,  ARISING FROM,  OUT OF  OR IN  CONNECTION  WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//================================================================================================
//   Typedefs and Defines
//================================================================================================

#define MAX_FRAMES	8

typedef struct _IconHdr
{	// Icon header
	uint32_t	ih_id;				// PS2 Icon ID
	uint32_t	ih_shapes;			// Number of shapes
	uint32_t	ih_comp;			// 0x07 = uncompressed, 0x0F = compressed textures
	uint32_t	ih_unk1;			// UNKNOWN
	uint32_t	ih_vertices;		// Number of vertices in icon. Always multiple of 3
} IconHdr;

typedef struct _IconVertex
{	// Icon coordinates describing each vertex
	int16_t	iv_x,
			iv_y,
			iv_z;	// X, Y & Z coordinates
	uint16_t	iv_unk1;	// UNKNOWN, Lighting ??
} IconVertex;

typedef struct _IconTextureXY
{	// Texture coordinates
	int16_t	it_x,
			it_y;
	uint32_t	it_rgba;
} IconTextureXY;

typedef struct _AnimHdr
{
	uint32_t	ah_id,
				ah_frameLen,
				ah_animSpeed,
				ah_playOffset,
				ah_frames;
} AnimHdr;

typedef struct _AnimFrame
{
	uint32_t	af_shapeId,
				af_frameKeys,
				af_unk1,
				af_unk2;
} AnimFrame;

typedef struct _Vertex
{	// Vertex coordinates
	float	v_x,
			v_y,
			v_z;
} Vertex;

typedef struct _TextureXY
{	// Texture coordinates
	float	t_x,
			t_y;
} TextureXY;

typedef struct _IconPtr
{	// Icon ptr structure
	int32_t		ip_shapes;		// Number of animation frames
	int32_t		ip_curFrame;	// Use this to keep track of anims
	int32_t		ip_vertices;	// Number of vertices
	Vertex		*ip_vertex;		// Vertex coordinates
	Vertex		*ip_normal;		// Normal coordinates
	TextureXY	*ip_textureXY;	// Texture coordinates
	uint8_t	*ip_texture;	// Pointer to texture data
} IconPtr;

//================================================================================================
//   Prototypes
//================================================================================================

IconPtr *iconInit(void *iIconData);
void iconDestroy(IconPtr *iIcon);
