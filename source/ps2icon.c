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

/*
 * This library is pretty much complete as of version 0.7.0.  The only thing not implemented
 * yet it loading of all textures in an animation aswell as animation timings.
 *	-- Martin
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <math.h>

#include "ps2icon.h"

/*
 * Playstation 2 is a Little Endian system,
 * we need functions to convert to Big Endian.
 */
static int16_t LittleToBig16(int16_t original)
{	// Little endian to big endian
	unsigned short converted;
	char *b;
	converted = 0;
	if (original == 0)
		return converted;
	b = (char *) &converted;
	*b++ = (char) (original);
	*b++ = (char) (original >> 8);
	return converted;
}

void *InitVertices(IconPtr *iIcon, void *iData)
{
	int	lVcount,
		lShape;
	IconVertex	*lIconVertex;
	IconTextureXY	*lIconTextureXY;
	Vertex		*lVertex,
				*lNormal;
	TextureXY	*lTextureXY;

	lVertex = iIcon->ip_vertex;
	lNormal = iIcon->ip_normal;
	lTextureXY = iIcon->ip_textureXY;

	// Iterate through all data and collect vectors,
	// normals and texture coordinates.
	lIconTextureXY = (IconTextureXY *) iData;
	for  (lVcount = 0; lVcount < iIcon->ip_vertices; lVcount++)
	{
		lIconVertex = (IconVertex *) lIconTextureXY;
		for (lShape = 0; lShape < iIcon->ip_shapes; lShape++)
		{
			lVertex[lShape * iIcon->ip_vertices].v_x = (float) (lIconVertex->iv_x) / 0x1000;
			lVertex[lShape * iIcon->ip_vertices].v_y = (float) - (lIconVertex->iv_y) / 0x1000;
			lVertex[lShape * iIcon->ip_vertices].v_z = (float) - (lIconVertex->iv_z) / 0x1000;
			lIconVertex++;
		}
		lVertex++;
		lNormal->v_x = (float) (lIconVertex->iv_x) / 0x1000;
		lNormal->v_y = (float) - (lIconVertex->iv_y) / 0x1000;
		lNormal->v_z = (float) - (lIconVertex->iv_z) / 0x1000;
		lIconVertex++;
		lNormal++;
		lIconTextureXY = (IconTextureXY *) lIconVertex;
		lTextureXY->t_x = (float) (lIconTextureXY->it_x) / 0x1000;
		lTextureXY->t_y = (float) (lIconTextureXY->it_y) / 0x1000;
		lTextureXY++;
		lIconTextureXY++;
	}
	return (void *) lIconTextureXY;
}

u_int32_t TIM2RGBA(u_int16_t iData)
{
	unsigned char	lRGBA[4];
	u_int16_t	lRGB;
	u_int32_t	*lReturn = (u_int32_t *) &lRGBA;

	lRGB = LittleToBig16(iData);
	lRGBA[0] = 8 * (lRGB & 0x1F);
	lRGBA[1] = 8 * ((lRGB >> 5) & 0x1F);
	lRGBA[2] = 8 * (lRGB >> 10);
	lRGBA[3] = 0xFF;

	return *lReturn;
}

void *InitTexture(int iMode, unsigned char *iData)
{
	int	i, lSize;
	u_int16_t	j;
	u_int32_t	*lTexturePtr, *lRGBA, *bajs;
	AnimHdr		*lHdr;
	AnimFrame	*lFrame;

	lHdr = (AnimHdr *) iData;
	lFrame = (AnimFrame *)(iData + sizeof(AnimHdr));

	lTexturePtr = (u_int32_t *) malloc(128 * 128 * 4);

	for (i = (lHdr->ah_frames); i > 0; i--)
	{
		lSize = ((lFrame->af_frameKeys) - 1) * 2;
		lFrame++;
		lFrame = &lFrame[0] + lSize;
	}

	iData = (unsigned char *) lFrame;
	lRGBA = lTexturePtr;
	bajs = lRGBA;

	if (iMode <= 7)
	{	// Uncompressed texture
		for (i = 0; i < (128 * 128); i++)
		{
			*lRGBA = TIM2RGBA((*(u_int16_t *) iData));
			lRGBA++;
			iData += 2;
		}
	}
	else
	{	//Compressed texture
		i = ((*(int16_t *) iData));
		iData += 4;
		do
		{
			j = ((*(int16_t *) iData)); 
			if (0XFF00 == (j & 0xFF00))
			{
				j = (0x0000 - j) & 0xFFFF;
				for (; j > 0; j--)
				{
					iData += 2;
					*lRGBA = TIM2RGBA((*(u_int16_t *) iData));
					lRGBA++;
				}
			}
			else
			{
				iData += 2;
				for (; j > 0; j--)
				{
					*lRGBA = TIM2RGBA((*(u_int16_t *) iData));
					lRGBA++;
				}
			}
			iData += 2;
		} while ((lRGBA - bajs) < 0x4000);
	}

	return (void *) lTexturePtr;
}

IconPtr *iconInit(void *iIconData)
{
	u_int32_t	lSize;
	IconPtr	*lIcon;
	IconHdr	*lHeader = (IconHdr *) iIconData;
	void	*lData = (void *) iIconData + sizeof(IconHdr);

	lIcon = malloc(sizeof(IconPtr));
	lIcon->ip_shapes = (lHeader->ih_shapes);
	lIcon->ip_vertices = (lHeader->ih_vertices);

	lSize= sizeof(Vertex) * (lIcon->ip_vertices * lIcon->ip_shapes);
	lIcon->ip_vertex = (Vertex *) malloc(lSize);

	lSize= sizeof(Vertex) * lIcon->ip_vertices;
	lIcon->ip_normal = (Vertex *) malloc(lSize);

	lSize = sizeof(TextureXY) * lIcon->ip_vertices;
	lIcon->ip_textureXY = (TextureXY *) malloc(lSize);
	
	lData = InitVertices(lIcon, lData);
	lIcon->ip_texture = InitTexture((int) (lHeader->ih_comp), (unsigned char *) lData);

	return lIcon;
}

void iconDestroy(IconPtr *iIcon)
{
	free(iIcon->ip_vertex);
	free(iIcon->ip_normal);
	free(iIcon->ip_textureXY);
	free(iIcon->ip_texture);
	free(iIcon);
}
