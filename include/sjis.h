#ifndef SJIS_H
#define SJIS_H

#include <stdint.h>

// Converts a Shift-JIS save title to a new UTF-8 string (input is modified)
char* sjis2utf8(char* input);

// Position of a Unicode character among the JIS X 0208 non-kanji (rows 1-8)
// and level 1 kanji (rows 16-47), the order of the PS2 BIOS font
// (rom0:KROM); -1 if it's not one of them
int jis_glyph_index(uint32_t unicode);

// Loads the BIOS font (path NULL: rom0:KROM); done once, on first use.
// Returns 0 if it isn't available.
int krom_load(const char* path);

// Draws a character as a size x size, 8-bit alpha glyph into bitmap (row
// pitch stride). Returns 0 if the BIOS font doesn't have it.
int krom_render(uint32_t unicode, uint8_t* bitmap, int stride, int size);

#endif
