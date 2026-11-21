/* 
   TINY3D - font library / (c) 2010 Hermes  <www.elotrolado.net>

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <zlib.h>

#include "types.h"
#include "libfont.h"
#include "menu.h"
#include "ttf_render.h"

struct t_font_description
{
    int w, h, bh;
    
    u8 first_char;
    u8 last_char;
    
    u8* rsx_text_offset;
    u32 rsx_bytes_per_char; 
    u32 color_format;

    short fw[256]; // chr width
    short fy[256]; // chr y correction
};

static struct t_font_datas
{
    int number_of_fonts;
    int current_font;

    struct t_font_description fonts[8];

    int sx, sy;
	int mono;
	int extra;

    u32 color, bkcolor;

    int align; //0 = left, 1 = center, 2 = right
    int autonewline;

    float X,Y,Z;

} font_datas;

typedef struct t_special_char
{
	char value;
	short fy;
	float scale;
	png_texture* image;
} special_char;

static special_char special_chars[MAX_SPECIAL_CHARS];
static int special_char_index = 0;


special_char* GetSpecialCharFromValue(const char value)
{
	int x;
	special_char* ret = NULL;

	for (x = 0; x < special_char_index; x++)
	{
		if (special_chars[x].value == value)
			ret = &(special_chars[x]);
	}
	return ret;
}

void ResetFont()
{
    font_datas.current_font = font_datas.number_of_fonts =0;

    font_datas.color = 0xffffffff;
    font_datas.bkcolor = 0;
    font_datas.align = FONT_ALIGN_LEFT;
    font_datas.X = font_datas.Y = font_datas.Z = 0.0f;
    font_datas.autonewline = 0;

    font_datas.sx = font_datas.sy = 8;
	font_datas.mono = 0;
}

u8 * AddFontFromBitmapArray(u8 *font, u8 *texture, u8 first_char, u8 last_char, int w, int h, int bits_per_pixel, int byte_order)
{
    int n, a, b;
    u32 buf[w*h];
    u8 i;
    
    if(font_datas.number_of_fonts >= 8) return texture;

    font_datas.fonts[font_datas.number_of_fonts].w = w;
    font_datas.fonts[font_datas.number_of_fonts].h = h;
    font_datas.fonts[font_datas.number_of_fonts].bh = h;
    font_datas.fonts[font_datas.number_of_fonts].color_format = 1; //TINY3D_TEX_FORMAT_A4R4G4B4; //TINY3D_TEX_FORMAT_A8R8G8B8;
    font_datas.fonts[font_datas.number_of_fonts].first_char = first_char;
    font_datas.fonts[font_datas.number_of_fonts].last_char  = last_char;
    font_datas.align = FONT_ALIGN_LEFT;

    font_datas.color = 0xffffffff;
    font_datas.bkcolor = 0x0;

    font_datas.sx = w;
    font_datas.sy = h;

    font_datas.Z = 0.0f;

    for(n = 0; n < 256; n++) {
        font_datas.fonts[font_datas.number_of_fonts].fw[n] = 0; 
        font_datas.fonts[font_datas.number_of_fonts].fy[n] = 0;
    }

    for(n = first_char; n <= last_char; n++) {

        font_datas.fonts[font_datas.number_of_fonts].fw[n] = w;

        texture = (u8 *) ((((long) texture) + 15) & ~15);

        if(n == first_char) font_datas.fonts[font_datas.number_of_fonts].rsx_text_offset = texture; //tiny3d_TextureOffset(texture);

        if(n == first_char+1) font_datas.fonts[font_datas.number_of_fonts].rsx_bytes_per_char = texture
            - font_datas.fonts[font_datas.number_of_fonts].rsx_text_offset;

        for(a = 0; a < h; a++) {
            for(b = 0; b < w; b++) {
                
                i = font[(b * bits_per_pixel)/8];

                if(byte_order) 
                    i= (i << ((b & (7/bits_per_pixel)) * bits_per_pixel))>> (8-bits_per_pixel);
                else
                    i >>= (b & (7/bits_per_pixel)) * bits_per_pixel;
                
                i = (i & ((1 << bits_per_pixel)-1)) * 255 / ((1 << bits_per_pixel)-1);

                if(i) {//TINY3D_TEX_FORMAT_A1R5G5B5
                    //i>>=3;
                    //*((u16 *) texture) = (1<<15) | (i<<10) | (i<<5) | (i);
                    //TINY3D_TEX_FORMAT_A4R4G4B4
                    buf[a*w + b] = 0xFF000000;
//                    i>>=4;
//                    *((u16 *) texture) = (i<<12) | 0xfff;
                } else {
//                    texture[0] = texture[1] = 0x0; //texture[2] = 0x0;
                    buf[a*w + b] = 0x0; // alpha
                }
            }
            font += (((w+7) & ~7) * bits_per_pixel) / 8;
        }

        // Black font texture
        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom((void*) buf, w, h, 32, 4 * w, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        *((SDL_Texture**) texture) = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        texture += sizeof(SDL_Texture*);

        // White font texture
        for (a = 0; a < h*w; a++)
            if (buf[a]) buf[a] = 0xFFFFFFFF;

        surface = SDL_CreateRGBSurfaceFrom((void*) buf, w, h, 32, 4 * w, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        *((SDL_Texture**) texture) = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        texture += sizeof(SDL_Texture*);
    }

    texture = (u8 *) ((((long) texture) + 15) & ~15);

    font_datas.number_of_fonts++;

    return texture;
}

/*
u8 * AddFontFromTTF(u8 *texture, u8 first_char, u8 last_char, int w, int h, 
    void (* ttf_callback) (u8 chr, u8 * bitmap, short *w, short *h, short *y_correction))
{
    int n, a, b;
    u8 i;
    u8 *font;
    static u8 letter_bitmap[257 * 256];
    
    int bits_per_pixel = 8;
    
    if(font_datas.number_of_fonts >= 8) return texture;

    if(h < 8) h = 8;
    if(w < 8) w = 8;
    if(h > 256) h = 256;
    if(w > 256) w = 256;

    font_datas.fonts[font_datas.number_of_fonts].w = w;
    font_datas.fonts[font_datas.number_of_fonts].h = h;
    font_datas.fonts[font_datas.number_of_fonts].bh = h+4;
    font_datas.fonts[font_datas.number_of_fonts].color_format = TINY3D_TEX_FORMAT_A4R4G4B4;
    font_datas.fonts[font_datas.number_of_fonts].first_char = first_char;
    font_datas.fonts[font_datas.number_of_fonts].last_char  = last_char;
    font_datas.align = FONT_ALIGN_LEFT;

    font_datas.color = 0xffffffff;
    font_datas.bkcolor = 0x0;

    font_datas.sx = w;
    font_datas.sy = h;

    font_datas.Z = 0.0f;

    for(n = 0; n < 256; n++) {
        font_datas.fonts[font_datas.number_of_fonts].fw[n] = 0; 
        font_datas.fonts[font_datas.number_of_fonts].fy[n] = 0;
    }

       
    for(n = first_char; n <= last_char; n++) {
        
        short hh = h;

        font = letter_bitmap;

        font_datas.fonts[font_datas.number_of_fonts].fw[n] = (short) w;
		
		ttf_callback((u8) (n & 255), letter_bitmap, &font_datas.fonts[font_datas.number_of_fonts].fw[n], &hh,  &font_datas.fonts[font_datas.number_of_fonts].fy[n]);

        // letter background correction
        if((hh + font_datas.fonts[font_datas.number_of_fonts].fy[n]) > font_datas.fonts[font_datas.number_of_fonts].bh) 
            font_datas.fonts[font_datas.number_of_fonts].bh = hh + font_datas.fonts[font_datas.number_of_fonts].fy[n];

        texture = (u8 *) ((((long) texture) + 15) & ~15);

        if(n == first_char) font_datas.fonts[font_datas.number_of_fonts].rsx_text_offset = tiny3d_TextureOffset(texture);

        if(n == first_char+1) font_datas.fonts[font_datas.number_of_fonts].rsx_bytes_per_char = tiny3d_TextureOffset(texture)
            - font_datas.fonts[font_datas.number_of_fonts].rsx_text_offset;

        for(a = 0; a < h; a++) {
            for(b = 0; b < w; b++) {
                
                i = font[(b * bits_per_pixel)/8];

                i >>= (b & (7/bits_per_pixel)) * bits_per_pixel;
                
                i = (i & ((1 << bits_per_pixel)-1)) * 255 / ((1 << bits_per_pixel)-1);

                if(i) {//TINY3D_TEX_FORMAT_A4R4G4B4
                    i>>=4;
                    *((u16 *) texture) = (i<<12) | 0xfff;
                } else {
              
                    texture[0] = texture[1] = 0x0; //texture[2] = 0x0;
                    //texture[3] = 0x0; // alpha
                } 
                texture+=2;
               
            }

            font += (w * bits_per_pixel) / 8;
                
        }
    
    }

    texture = (u8 *) ((((long) texture) + 15) & ~15);

    font_datas.number_of_fonts++;

    return texture;
}
*/

void SetCurrentFont(int nfont)
{
    if(nfont < 0 || nfont >= font_datas.number_of_fonts) nfont = 0;

    font_datas.current_font = nfont;
}

void SetFontSize(int sx, int sy)
{
    font_datas.sx = sx;
    font_datas.sy = sy;
}

void SetFontColor(u32 color, u32 bkcolor)
{
    font_datas.color   = color;
    font_datas.bkcolor = bkcolor;
}

void SetFontAlign(int mode)
{
    font_datas.align  = mode;
    font_datas.autonewline = 0;
}

void SetFontAutoNewLine(int width)
{
    font_datas.autonewline = width;
    font_datas.align  = FONT_ALIGN_LEFT;
}

void SetFontZ(float z)
{
    font_datas.Z  = z;
}

float GetFontX()
{
    return font_datas.X;
}

float GetFontY()
{
    return font_datas.Y;
}

void SetMonoSpace(int space)
{
	font_datas.mono = space;
}

void SetExtraSpace(int space)
{
	font_datas.extra = space;
}

void RegisterSpecialCharacter(char value, short fy, float scale, png_texture* image)
{
	// Verify special character
	if (value == 0)
		return;
	if (image->texture == NULL || image->size == 0)
		return;
	if (image->width == 0 || image->height == 0)
		return;
	
	// Verify value is not in use
	if (GetSpecialCharFromValue(value))
		return;

	// Verify room in array
	if (special_char_index < MAX_SPECIAL_CHARS)
	{
		special_chars[special_char_index].value = value;
	    special_chars[special_char_index].fy = fy;
	    special_chars[special_char_index].scale = scale;
    	special_chars[special_char_index].image = image;

		special_char_index++;
	}
}

int WidthFromStr(const char * str)
{
	return width_ttf_string(str, font_datas.sx, font_datas.sy);
}

// bytes in the UTF-8 character starting with c
static int utf8_char_len(unsigned char c)
{
	return (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
}

char* wrap_text(const char* text, int max_width)
{
	size_t len = strlen(text);
	char* out = malloc(len * 2 + 1);    // at most one '\n' added per character
	char* line = malloc(len + 1);
	const char* p = text;
	size_t o = 0;

	if (!out || !line)
	{
		free(out);
		free(line);
		return strdup(text);
	}

	for (;;)
	{
		size_t n = strcspn(p, "\n");   // one line of the original text
		size_t s = 0;

		while (s < n)
		{
			size_t i = s, fit = s, brk = 0, e;

			// the longest piece from s that fits, and the last space in it
			while (i < n)
			{
				size_t cl = utf8_char_len(p[i]);
				if (i + cl > n)
					cl = n - i;

				memcpy(line, p + s, i + cl - s);
				line[i + cl - s] = 0;
				if (WidthFromStr(line) > max_width)
					break;

				i += cl;
				fit = i;
				if (p[i - 1] == ' ')
					brk = i;
			}

			if (fit < n)
			{
				if (brk > s)
					fit = brk;          // break after the last space
				else if (fit == s)
					fit = s + utf8_char_len(p[s]);  // at least one character
			}

			// spaces at the break are dropped
			for (e = fit; e > s && p[e - 1] == ' '; e--);
			memcpy(out + o, p + s, e - s);
			o += e - s;

			s = fit;
			if (s < n)
				out[o++] = '\n';
		}

		if (!p[n])
			break;

		out[o++] = '\n';
		p += n + 1;
	}

	out[o] = 0;
	free(line);
	return out;
}

void shorten_text(char* text, int max_width)
{
	size_t n = strlen(text), lo = 0, hi, mid;
	char* tmp;

	if (WidthFromStr(text) <= max_width || (tmp = malloc(n + 4)) == NULL)
		return;

	// binary search for the longest start (cut at a UTF-8 character
	// boundary) that fits with "..." after it: it's drawn on every frame
	hi = n;
	while (lo < hi)
	{
		mid = (lo + hi + 1) / 2;
		while (mid > lo && ((unsigned char) text[mid] & 0xC0) == 0x80)
			mid--;

		if (mid == lo)
		{
			// no boundary between lo and hi other than lo: try the next one up
			size_t next = lo + utf8_char_len(text[lo]);
			if (next > hi)
				break;
			mid = next;
		}

		memcpy(tmp, text, mid);
		strcpy(tmp + mid, "...");
		if (WidthFromStr(tmp) <= max_width)
			lo = mid;
		else
			hi = mid - 1;
	}

	// hi may now be inside a character: lo is always a boundary
	strcpy(text + lo, "...");
	free(tmp);
}

int WidthFromStrMono(u8 * str)
{
    int w = 0;

    while(*str) {
		w += (font_datas.sx * font_datas.mono) / font_datas.fonts[font_datas.current_font].w + font_datas.extra;
		str++;
    }

    return w;
}

int DrawCharSpecial(float x, float y, float z, const special_char* schr, uint8_t draw)
{
    float dx = (float)font_datas.sx / (float)schr->image->width;
    float dy = (float)font_datas.sy / (float)schr->image->height;
    float min = (dx >  dy ? dy : dx);

	dx = (min * schr->scale * schr->image->width);
	dy = (min * schr->scale * schr->image->height);

	if (!draw)
		return (int)dx;

	if (schr->fy)
		y += (float)schr->fy;
	else
		y += ((float)font_datas.sy - dy)/2;
	
	// Load sprite texture
    DrawTexture(schr->image, x, y, z, dx, dy, 0xFFFFFF00 | RGBA_A(font_datas.color));

    return (int)dx;
}

void orbis2dDrawChar(float x, float y, const u8* bitmap, int bw, int bh, int dw, int dh, u32 rgba)
{
    SDL_Texture* sdl_tex = ((SDL_Texture**) bitmap)[(rgba & 0xFFFFFF00) ? 1 : 0];
    SDL_Rect dest = {
        .x = x,
        .y = y,
        .w = dw,
        .h = dh,
    };

    SDL_SetTextureAlphaMod(sdl_tex, RGBA_A(rgba));
    SDL_RenderCopy(renderer, sdl_tex, NULL, &dest);
}

void DrawCharMono(float x, float y, float z, u8 chr)
{
	float dx = font_datas.sx, dy = font_datas.sy;
	float dx2 = (dx * font_datas.mono) / font_datas.fonts[font_datas.current_font].w;
	float dy2 = (float)(dy * font_datas.fonts[font_datas.current_font].bh) / (float)font_datas.fonts[font_datas.current_font].h;

	if (font_datas.number_of_fonts <= 0) return;

	if (chr < font_datas.fonts[font_datas.current_font].first_char) return;

	if (font_datas.bkcolor) {
        SDL_FRect rect = {
            .x = x,
            .y = y,
            .w = dx2,
            .h = dy2,
        };

        SDL_SetRenderDrawColor(renderer, RGBA_R(font_datas.bkcolor), RGBA_G(font_datas.bkcolor), RGBA_B(font_datas.bkcolor), RGBA_A(font_datas.bkcolor));
        SDL_RenderFillRectF(renderer, &rect);
	}

	y += (float)(font_datas.fonts[font_datas.current_font].fy[chr] * font_datas.sy) / (float)(font_datas.fonts[font_datas.current_font].h);

	if (chr > font_datas.fonts[font_datas.current_font].last_char) return;

	// Load sprite texture
    orbis2dDrawChar(x, y, font_datas.fonts[font_datas.current_font].rsx_text_offset + font_datas.fonts[font_datas.current_font].rsx_bytes_per_char
		* (chr - font_datas.fonts[font_datas.current_font].first_char), font_datas.fonts[font_datas.current_font].w,
		font_datas.fonts[font_datas.current_font].h, dx, dy, font_datas.color);
}

void DrawChar(float x, float y, float z, u8 chr)
{
	special_char* schr = GetSpecialCharFromValue(chr);
	if (schr)
	{
		DrawCharSpecial(x, y, z, schr, 1);
		return;
	}

	float dx = font_datas.sx, dy = font_datas.sy;
	float dx2 = (dx * font_datas.fonts[font_datas.current_font].fw[chr]) / font_datas.fonts[font_datas.current_font].w;
	float dy2 = (float)(dy * font_datas.fonts[font_datas.current_font].bh) / (float)font_datas.fonts[font_datas.current_font].h;

	if (font_datas.number_of_fonts <= 0) return;

	if (chr < font_datas.fonts[font_datas.current_font].first_char) return;

	if (font_datas.bkcolor) {
        SDL_FRect rect = {
            .x = x,
            .y = y,
            .w = dx2,
            .h = dy2,
        };

        SDL_SetRenderDrawColor(renderer, RGBA_R(font_datas.bkcolor), RGBA_G(font_datas.bkcolor), RGBA_B(font_datas.bkcolor), RGBA_A(font_datas.bkcolor));
        SDL_RenderFillRectF(renderer, &rect);
	}

	y += (float)(font_datas.fonts[font_datas.current_font].fy[chr] * font_datas.sy) / (float)(font_datas.fonts[font_datas.current_font].h);

	if (chr > font_datas.fonts[font_datas.current_font].last_char) return;

	// Load sprite texture
    orbis2dDrawChar(x, y, font_datas.fonts[font_datas.current_font].rsx_text_offset + font_datas.fonts[font_datas.current_font].rsx_bytes_per_char
		* (chr - font_datas.fonts[font_datas.current_font].first_char), font_datas.fonts[font_datas.current_font].w,
		font_datas.fonts[font_datas.current_font].h, dx, dy, font_datas.color);
}

static int i_must_break_line(const char *str, float x)
{
    int xx =0;
	int dx = (font_datas.sx+font_datas.extra);
	
    while(*str) {
        if(((u8)*str) <= 32) break;
        xx += dx * font_datas.fonts[font_datas.current_font].fw[((u8)*str)] / font_datas.fonts[font_datas.current_font].w;
        str++;
    }

    if(*str && (x+xx) >= font_datas.autonewline) return 1;

    return 0;
}

float DrawStringMono(float x, float y, const char *str)
{
	float initX = x;
	int dx = font_datas.sx;
    font_datas.mono = font_datas.fonts[font_datas.current_font].w;
	
    switch (font_datas.align)
    {
    case FONT_ALIGN_SCREEN_CENTER:
        x= (SCREEN_WIDTH - WidthFromStrMono((u8 *) str)) / 2;
        break;

    case FONT_ALIGN_RIGHT:
		x -= WidthFromStrMono((u8 *) str);
        break;

    case FONT_ALIGN_CENTER:
		x -= WidthFromStrMono((u8 *) str)/2;
        break;

    default:
        break;
    }

    while (*str) {
        
        if(*str == '\n') {
            x = initX; 
            y += font_datas.sy * font_datas.fonts[font_datas.current_font].bh / font_datas.fonts[font_datas.current_font].h;
            str++;
            continue;
        } else {
            if(font_datas.autonewline && i_must_break_line(str, x)) {
                x = initX; 
                y += font_datas.sy * font_datas.fonts[font_datas.current_font].bh / font_datas.fonts[font_datas.current_font].h;
            }
        }

        DrawChar(x, y, font_datas.Z, (u8) *str);
		x += (dx * font_datas.mono) / font_datas.fonts[font_datas.current_font].w + font_datas.extra;
        str++; 
    }

    font_datas.X = x; font_datas.Y = y;

    return x;
}

int skip_icon(int x, int y, char c)
{
	special_char* schr = GetSpecialCharFromValue(c);
	if (schr)
		return DrawCharSpecial(x, y, font_datas.Z, schr, 0);

	else return 0;
}

int draw_icon(int x, int y, char c)
{
	special_char* schr = GetSpecialCharFromValue(c);
	if (schr)
		return DrawCharSpecial(x, y, font_datas.Z, schr, 1);

	else return 0;
}

float DrawString(float x, float y, const char *str)
{
    switch (font_datas.align)
    {
    case FONT_ALIGN_SCREEN_CENTER:
        x= (SCREEN_WIDTH - WidthFromStr(str)) / 2;
        break;

    case FONT_ALIGN_RIGHT:
		x -= WidthFromStr(str);
        break;

    case FONT_ALIGN_CENTER:
		x -= WidthFromStr(str)/2;
        break;

    default:
        break;
    }

	display_ttf_string((int)x +1, (int)y +1, str, 0x00000000 | (font_datas.color & 0x000000ff), 0, font_datas.sx, font_datas.sy+4, &skip_icon);

	return display_ttf_string((int)x, (int)y, str, font_datas.color, font_datas.bkcolor, font_datas.sx, font_datas.sy+4, &draw_icon);
}

float DrawFormatString(float x, float y, char *format, ...)
{
    char buff[4096];
    va_list opt;

    va_start(opt, format);
    vsprintf((void *) buff, format, opt);
    va_end(opt);

    return DrawString(x, y, buff);
}

float DrawFormatStringMono(float x, float y, char *format, ...)
{
    char buff[4096];
    char *str = buff;
    va_list opt;

    va_start(opt, format);
    vsprintf((void *) buff, format, opt);
    va_end(opt);

    while (*str) {
        DrawCharMono(x, y, font_datas.Z, (u8) *str);
        x += font_datas.sx;
        str++;
    }

    return(x);
}
