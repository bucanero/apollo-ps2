/*
* Copyright (c) 2008 Andreas Weis (http://www.ghulbus-inc.de/)
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

#ifndef PS2ICON_H
#define PS2ICON_H

#include <stdint.h>
#include <stddef.h>

//================================================================================================
//   Typedefs and Defines
//================================================================================================

/** File header
 */
typedef struct Icon_Header_t {
	uint32_t file_id;						///< reserved; should be: 0x010000 (but does not have to ;) )
	uint32_t animation_shapes;				///< number of animation shapes per vertex
	uint32_t texture_type;					///< texture type - 0x07: uncompressed, 0x06: uncompresses, 0x0f: RLE compression
	uint32_t reserved;						///< reserved; should be: 0x3F800000 (but does not have to ;) )
	uint32_t n_vertices;					///< number of vertices; must be a multiple of 3
} Icon_Header;

/** Set of vertex coordinates
 * @note The f16_* fields indicate float16 data; divide by 4096.0f to convert to float32;
 */
typedef struct Vertex_Coord_t {
	short f16_x;								///< vertex x coordinate in float16
	short f16_y;								///< vertex y coordinate in float16
	short f16_z;								///< vertex z coordinate in float16
	short f16_unknown;							///< unknown; seems to influence lightning?
} Vertex_Coord;

/** Set of texture coordinates
 * @note The f16_* fields indicate float16 data; divide by 4096.0f to convert to float32;
 */
typedef struct Texture_Data_t {
	short        f16_u;							///< vertex u texture coordinate in float16
	short        f16_v;							///< vertex v texture coordinate in float16
	uint32_t     color;							///< vertex color (32 bit RGBA)
} Texture_Data;

/** Animation header
 */
typedef struct Animation_Header_t {
	uint32_t id_tag;						///< ???
	uint32_t frame_length;					///< ???
	float    anim_speed;					///< ???
	uint32_t play_offset;					///< ???
	uint32_t n_frames;						///< number of frames in the animation
} Animation_Header;

/** Per-frame animation data
 */
typedef struct Frame_Data_t {
	uint32_t shape_id;						///< shape used for this frame
	uint32_t n_keys;						///< number of keys corresponding to this frame
} Frame_Data;

/** Per-key animation data
 */
typedef struct Frame_Key_t {
	float time;									///< ???
	float value;								///< ???
} Frame_Key;


/** A parsed icon: the morph targets, the per-vertex attributes they share,
 *  and the 128x128 texture.
 *
 *  `texture` is always allocated, even for an icon that carries none - those
 *  are drawn from their vertex colours alone. `has_texture` says which it is,
 *  so the renderer can substitute white instead of black.
 */
typedef struct {
	int       shape_count;      ///< morph targets, at least 1
	int       vertex_count;     ///< a multiple of 3
	float    *shapes;           ///< shape_count * vertex_count * 3
	float    *normals;          ///< vertex_count * 3
	float    *uvs;              ///< vertex_count * 2
	uint8_t  *colors;           ///< vertex_count * 4, RGBA, 0x80-centred
	uint32_t *texture;          ///< 128 * 128 RGBA, never NULL after a parse
	int       has_texture;
	int       still_shape;      ///< the shape a still frame should use
} ps2icon_t;

//================================================================================================
//   Prototypes
//================================================================================================

/** Parse an .ico of `len` bytes. Returns 0 when geometry was read, negative
 *  when the file is not a usable icon. Free with ps2icon_free(). */
int ps2icon_parse(const uint8_t *data, size_t len, ps2icon_t *out);

void ps2icon_free(ps2icon_t *icon);

/** Width and height of the image ps2IconRender() returns. */
#define PS2ICON_SIZE	128

/** Size of an icon.sys file (libmc's mcIcon). The header takes it as raw bytes
 *  so it doesn't need <libmc.h>, whose tamtypes.h clashes with the u32/s32
 *  macros in Apollo's types.h when included after it. */
#define PS2ICON_SYS_SIZE	964

/** Render an .ico as the PS2 browser shows it: a still 3D view of the model,
 *  lit with the lights of the save's icon.sys (`iconsys`: PS2ICON_SYS_SIZE
 *  bytes, or NULL for default lighting) on a transparent background.
 *  Returns a PS2ICON_SIZE x PS2ICON_SIZE image (R,G,B,A bytes per pixel) to
 *  free(), or NULL when the icon cannot be parsed or rendered. */
uint8_t* ps2IconRender(const uint8_t *data, size_t len, const void *iconsys);

#endif
