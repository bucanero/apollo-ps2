#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "ps2icon.h"


static uint32_t TIM2RGBA(uint16_t lRGB)
{
	unsigned char	lRGBA[4];

	lRGBA[0] = 8 * (lRGB & 0x1F);
	lRGBA[1] = 8 * ((lRGB >> 5) & 0x1F);
	lRGBA[2] = 8 * (lRGB >> 10);
	lRGBA[3] = 0xFF;

	return *((uint32_t *) &lRGBA);
}

void* ps2IconTexture(const uint8_t* icon_data)
{
	int i;
	uint16_t j;
	Icon_Header header;
	Animation_Header anim_header;
	Frame_Data animation;
	uint32_t *lTexturePtr, *lRGBA;
	const uint8_t* iData = icon_data;

	//read header:
	memcpy(&header, icon_data, sizeof(Icon_Header));
	iData += sizeof(Icon_Header);

	//n_vertices has to be divisible by three, that's for sure:
	if(header.file_id != 0x010000 || header.n_vertices % 3 != 0) {
		return NULL;
	}

	//read icon data from file: https://ghulbus-inc.de/projects/ps2iconsys/
	///Vertex data
	// each vertex consists of animation_shapes tuples for vertex coordinates,
	// followed by one vertex coordinate tuple for normal coordinates
	// followed by one texture data tuple for texture coordinates and color
	for(i=0; i<header.n_vertices; i++) {
		iData += sizeof(Vertex_Coord) * header.animation_shapes;
		iData += sizeof(Vertex_Coord);
		iData += sizeof(Texture_Data);
	}

	//animation data
	// preceeded by an animation header, there is a frame data/key set for every frame:
	memcpy(&anim_header, iData, sizeof(Animation_Header));
	iData += sizeof(Animation_Header);

	//read animation data:
	for(i=0; i<anim_header.n_frames; i++) {
		memcpy(&animation, iData, sizeof(Frame_Data));
		iData += sizeof(Frame_Data);
		if(animation.n_keys > 0) {
			iData += sizeof(Frame_Key) * animation.n_keys;
		}
	}

	lTexturePtr = (uint32_t *) malloc(128 * 128 * 4);
	lRGBA = lTexturePtr;

	if (header.texture_type <= 7)
	{	// Uncompressed texture
		for (i = 0; i < (128 * 128); i++)
		{
			*lRGBA = TIM2RGBA((*(uint16_t *) iData));
			lRGBA++;
			iData += 2;
		}
	}
	else
	{	//Compressed texture
		i = ((*(int32_t *) iData));
		iData += 4;
		do
		{
			j = ((*(int16_t *) iData)); 
			if (0XFF00 == (j & 0xFF00))
			{
				for (j = (0x0000 - j) & 0xFFFF; j > 0; j--)
				{
					iData += 2;
					*lRGBA = TIM2RGBA((*(uint16_t *) iData));
					lRGBA++;
				}
			}
			else
			{
				iData += 2;
				for (; j > 0; j--)
				{
					*lRGBA = TIM2RGBA((*(uint16_t *) iData));
					lRGBA++;
				}
			}
			iData += 2;
		} while ((lRGBA - lTexturePtr) < 0x4000);
	}

	return (lTexturePtr);
}
