/*
 * Shift-JIS -> UTF-8 for PS1/PS2 save titles
 *
 * Save titles (icon.sys, PS1 block headers, PSV/VMC entries) are Shift-JIS.
 * Full-width Latin letters, digits and punctuation become ASCII, so they're
 * drawn with the UI's Latin font; everything else is converted to UTF-8 with
 * the JIS X 0208 table in shiftjis.h.
 *
 * Japanese glyphs come from the PS2 BIOS font (rom0:KROM): 15x15, 1 bit per
 * pixel glyphs for the JIS X 0208 non-kanji and level 1 kanji, 30 bytes each
 * (one 16-bit row per line, top bit = leftmost pixel), in JIS order. The
 * UI's fonts are Latin only, so the TTF renderer asks here for the
 * characters they don't have (kana and kanji in Japanese save titles).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "shiftjis.h"
#include "sjis.h"

// ASCII for Shift-JIS 0x8140..0x81AC; '?' cells have no ASCII look-alike
// (々, ヾ, ー, 〇, ∞, ÷, ...) and are kept as they are
static const char SJIS_REPLACEMENT_TABLE[] =
    " ,.,..:;?!\"*'`*^"
    "-_??????????" "--/\\" // split: "??-" would be a trigraph
    "~||--''\"\"()()[]{"
    "}<><>[][][]+-+X?"
    "-==<><>????*'\"CY"
    "$c&%#&*@S*******"
    "*******T><^_'='";

// NEC special characters (row 13, Shift-JIS 0x8740..0x879C), missing from
// the JIS X 0208 table; 0 = unassigned
static const uint16_t SJIS_NEC_ROW13[0x9D - 0x40] = {
	// 0x8740: circled numbers 1..20
	0x2460, 0x2461, 0x2462, 0x2463, 0x2464, 0x2465, 0x2466, 0x2467, 0x2468, 0x2469,
	0x246A, 0x246B, 0x246C, 0x246D, 0x246E, 0x246F, 0x2470, 0x2471, 0x2472, 0x2473,
	// 0x8754: Roman numerals I..X (written as ASCII, see sjis2utf8)
	0x2160, 0x2161, 0x2162, 0x2163, 0x2164, 0x2165, 0x2166, 0x2167, 0x2168, 0x2169,
	// 0x875E
	0,
	// 0x875F: squared units
	0x3349, 0x3314, 0x3322, 0x334D, 0x3318, 0x3327, 0x3303, 0x3336, 0x3351, 0x3357,
	0x330D, 0x3326, 0x3323, 0x332B, 0x334A, 0x333B, 0x339C, 0x339D, 0x339E, 0x338E,
	0x338F, 0x33C4, 0x33A1,
	// 0x8776
	0, 0, 0, 0, 0, 0, 0, 0,
	// 0x877E: era name Heisei, 0x877F (not a trail byte)
	0x337B, 0,
	// 0x8780
	0x301D, 0x301F, 0x2116, 0x33CD, 0x2121, 0x32A4, 0x32A5, 0x32A6, 0x32A7, 0x32A8,
	0x3231, 0x3232, 0x3239, 0x337E, 0x337D, 0x337C, 0x2252, 0x2261, 0x222B, 0x222E,
	0x2211, 0x221A, 0x22A5, 0x2220, 0x221F, 0x22BF, 0x2235, 0x2229, 0x222A,
};

// half-width katakana (0xA1..0xDF) as full-width: the BIOS font has no
// half-width glyphs, and they're hard to read at UI size anyway
static const uint16_t SJIS_HALFWIDTH_KANA[0xE0 - 0xA1] = {
	0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2,                         // 。「」、・ヲ
	0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, // ァィゥェォャュョッ
	0x30FC,                                                                 // ー
	0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA,                                 // アイウエオ
	0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3,                                 // カキクケコ
	0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD,                                 // サシスセソ
	0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8,                                 // タチツテト
	0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE,                                 // ナニヌネノ
	0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB,                                 // ハヒフヘホ
	0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2,                                 // マミムメモ
	0x30E4, 0x30E6, 0x30E8,                                                 // ヤユヨ
	0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED,                                 // ラリルレロ
	0x30EF, 0x30F3,                                                         // ワン
	0x309B, 0x309C,                                                         // ゛゜
};

// katakana + (semi-)voiced mark as one character (ﾌﾞ -> ブ, ﾊﾟ -> パ); 0 if none
static uint16_t sjis_voiced(uint16_t kana, int semi)
{
	if (kana >= 0x30CF && kana <= 0x30DB && (kana - 0x30CF) % 3 == 0)     // ハヒフヘホ
		return kana + (semi ? 2 : 1);

	if (semi)
		return 0;

	if (kana == 0x30A6)                                                     // ウ -> ヴ
		return 0x30F4;

	if ((kana >= 0x30AB && kana <= 0x30C1 && (kana & 1)) ||                 // カ .. チ
		kana == 0x30C4 || kana == 0x30C6 || kana == 0x30C8)                 // ツテト
		return kana + 1;

	return 0;
}

static const char* const SJIS_ROMAN[10] = {
	"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"
};

// first byte of a two-byte Shift-JIS character
static int sjis_is_lead(uint8_t c)
{
	return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
}

//Convert Shift-JIS characters to ASCII equivalent (in place)
static void sjis2ascii(uint8_t* bData)
{
	int i = 0, j = 0;

	while (bData[i])
	{
		// single-byte character, or a lead byte cut off by the end
		if (!sjis_is_lead(bData[i]) || !bData[i+1])
		{
			bData[j++] = bData[i++];
			continue;
		}

		uint16_t ch = (bData[i] << 8) | bData[i+1];
		i += 2;

		// 'A' .. 'Z'
		// '0' .. '9'
		if ((ch >= 0x8260 && ch <= 0x8279) || (ch >= 0x824F && ch <= 0x8258))
		{
			bData[j++] = (ch & 0xFF) - 0x1F;
			continue;
		}

		// 'a' .. 'z'
		if (ch >= 0x8281 && ch <= 0x829A)
		{
			bData[j++] = (ch & 0xFF) - 0x20;
			continue;
		}

		// punctuation; 0x8148 is the full-width '?' itself
		if (ch >= 0x8140 && ch <= 0x81AC &&
			(SJIS_REPLACEMENT_TABLE[(ch & 0xFF) - 0x40] != '?' || ch == 0x8148))
		{
			bData[j++] = SJIS_REPLACEMENT_TABLE[(ch & 0xFF) - 0x40];
			continue;
		}

		// no ASCII equivalent: keep both bytes
		bData[j++] = ch >> 8;
		bData[j++] = ch & 0xFF;
	}

	bData[j] = 0;
}

static uint16_t sjis_table(size_t offset)
{
	offset <<= 1;
	return (shiftJIS_convTable[offset] << 8) | shiftJIS_convTable[offset + 1];
}

// PSV files (PS1/PS2) savegame titles are stored in Shift-JIS
char* sjis2utf8(char* input)
{
	// Simplify the input and decode standard ASCII characters
	sjis2ascii((uint8_t*) input);

	const uint8_t* in = (const uint8_t*) input;
	size_t len = strlen(input);
	// a character takes at most 3 UTF-8 bytes; Roman numerals take up to 4
	// ASCII bytes but come from 2 input bytes
	char* output = malloc(3 * len + 1);
	size_t i = 0, o = 0, last_o = 0;
	uint16_t last = 0;      // previous character, for half-width voiced marks

	if (!output)
		return NULL;

	while (i < len)
	{
		uint8_t lead = in[i++];
		uint16_t unicodeValue;

		if (lead < 0x80)
			unicodeValue = lead; // ASCII: the table has JIS-Roman, where '\' and '~' are yen and overline
		else if (lead >= 0xA1 && lead <= 0xDF)
		{
			uint16_t v;

			unicodeValue = SJIS_HALFWIDTH_KANA[lead - 0xA1];
			// a voiced mark joins the kana written just before it
			if (lead >= 0xDE && (v = sjis_voiced(last, lead == 0xDF)) != 0)
			{
				o = last_o;
				unicodeValue = v;
			}
		}
		else if (!sjis_is_lead(lead))
			unicodeValue = sjis_table(lead);
		else
		{
			if (i >= len)
				break;

			uint8_t trail = in[i++];

			if (lead == 0x87 && trail >= 0x54 && trail <= 0x5D)
			{
				// Roman numerals: ASCII, so the Latin font draws them
				const char* r = SJIS_ROMAN[trail - 0x54];
				size_t n = strlen(r);
				memcpy(output + o, r, n);
				o += n;
				last = 0;
				continue;
			}

			if (lead == 0x87)
				unicodeValue = (trail >= 0x40 && trail <= 0x9C && SJIS_NEC_ROW13[trail - 0x40]) ?
					SJIS_NEC_ROW13[trail - 0x40] : ' ';
			else if (lead >= 0xF0)
				unicodeValue = '?'; // user-defined characters
			else
				unicodeValue = sjis_table(((lead >> 4) == 0x8 ? 0x100 : (lead >> 4) == 0x9 ? 0x1100 : 0x2100) +
					((lead & 0xF) << 8) + trail);
		}

		last = unicodeValue;
		last_o = o;

		//converting to UTF8
		if (unicodeValue < 0x80)
		{
			output[o++] = unicodeValue;
		}
		else if (unicodeValue < 0x800)
		{
			output[o++] = 0xC0 | (unicodeValue >> 6);
			output[o++] = 0x80 | (unicodeValue & 0x3f);
		}
		else
		{
			output[o++] = 0xE0 | (unicodeValue >> 12);
			output[o++] = 0x80 | ((unicodeValue & 0xfff) >> 6);
			output[o++] = 0x80 | (unicodeValue & 0x3f);
		}
	}

	output[o] = 0;
	return output;
}

// Unicode -> JIS X 0208 glyph index, sorted by Unicode (built on first use)
typedef struct
{
	uint16_t unicode;
	uint16_t index;
} jis_glyph_t;

#define JIS_GLYPHS 3489 // 524 non-kanji + 2965 level 1 kanji

static jis_glyph_t* jis_glyphs;
static int jis_count;

static int jis_glyph_cmp(const void* a, const void* b)
{
	return (int) ((const jis_glyph_t*) a)->unicode - (int) ((const jis_glyph_t*) b)->unicode;
}

static int jis_glyphs_build(void)
{
	jis_glyphs = malloc(JIS_GLYPHS * sizeof(jis_glyph_t));
	if (!jis_glyphs)
		return 0;

	// rows 1-8 then 16-47, cell by cell; unassigned cells are U+0020 in the
	// table and have no glyph
	for (int row = 1; row <= 47 && jis_count < JIS_GLYPHS; row++)
	{
		if (row > 8 && row < 16)
			continue;

		for (int cell = 1; cell <= 94; cell++)
		{
			uint8_t lead = ((row - 1) >> 1) + 0x81;
			uint8_t trail = (row & 1) ? cell + 0x3F + (cell >= 64) : cell + 0x9E;
			uint16_t u = sjis_table(((lead >> 4) == 0x8 ? 0x100 : 0x1100) + ((lead & 0xF) << 8) + trail);

			if (u == ' ')
				continue;

			jis_glyphs[jis_count].unicode = u;
			jis_glyphs[jis_count].index = jis_count;
			jis_count++;
		}
	}

	qsort(jis_glyphs, jis_count, sizeof(jis_glyph_t), jis_glyph_cmp);
	return 1;
}

int jis_glyph_index(uint32_t unicode)
{
	if (unicode > 0xFFFF || (!jis_glyphs && !jis_glyphs_build()))
		return -1;

	jis_glyph_t key = { .unicode = unicode };
	jis_glyph_t* g = bsearch(&key, jis_glyphs, jis_count, sizeof(jis_glyph_t), jis_glyph_cmp);

	return g ? g->index : -1;
}

// BIOS font glyphs are doubled with Scale2x, which keeps diagonals and curves
// smooth, emboldened by a pixel, then box-filtered down to the UI's 24 pixel
// em, anti-aliased

#define KROM_PATH       "rom0:KROM"
#define KROM_GLYPH_SZ   30          // bytes per glyph
#define KROM_DIM        15          // glyph width and height
#define KROM_2X         (KROM_DIM * 2)

static uint8_t* krom_data;
static int krom_state;              // 0: not loaded yet, 1: loaded, -1: unavailable

int krom_load(const char* path)
{
	int fd, total = 0, r;
	const int size = JIS_GLYPHS * KROM_GLYPH_SZ;

	if (krom_state)
		return (krom_state > 0);

	krom_state = -1;
	if ((fd = open(path ? path : KROM_PATH, O_RDONLY)) < 0)
		return 0;

	if ((krom_data = malloc(size)) != NULL)
		while (total < size && (r = read(fd, krom_data + total, size - total)) > 0)
			total += r;

	close(fd);
	if (total < size)
	{
		free(krom_data);
		krom_data = NULL;
		return 0;
	}

	krom_state = 1;
	return 1;
}

static int krom_pixel(const uint8_t* g, int x, int y)
{
	if (x < 0 || y < 0 || x >= KROM_DIM || y >= KROM_DIM)
		return 0;

	return (((g[y * 2] << 8) | g[y * 2 + 1]) >> (15 - x)) & 1;
}

// Scale2x (EPX): each pixel becomes 2x2, corners follow matching neighbours
static void krom_scale2x(const uint8_t* g, uint8_t out[KROM_2X][KROM_2X])
{
	for (int y = 0; y < KROM_DIM; y++)
		for (int x = 0; x < KROM_DIM; x++)
		{
			int p = krom_pixel(g, x, y);
			int a = krom_pixel(g, x, y - 1);    // up
			int b = krom_pixel(g, x + 1, y);    // right
			int c = krom_pixel(g, x - 1, y);    // left
			int d = krom_pixel(g, x, y + 1);    // down

			out[y*2][x*2]         = (c == a && c != d && a != b) ? a : p;
			out[y*2][x*2 + 1]     = (a == b && a != c && b != d) ? b : p;
			out[y*2 + 1][x*2]     = (d == c && d != b && c != a) ? c : p;
			out[y*2 + 1][x*2 + 1] = (b == d && b != a && d != c) ? d : p;
		}
}

// overlap of [a0,a1) and [b0,b1)
static int overlap(int a0, int a1, int b0, int b1)
{
	int lo = (a0 > b0) ? a0 : b0;
	int hi = (a1 < b1) ? a1 : b1;

	return (hi > lo) ? hi - lo : 0;
}

int krom_render(uint32_t unicode, uint8_t* bitmap, int stride, int size)
{
	uint8_t big[KROM_2X][KROM_2X];
	int index;

	if (size <= 0 || size > stride || !krom_load(NULL) || (index = jis_glyph_index(unicode)) < 0)
		return 0;

	krom_scale2x(krom_data + index * KROM_GLYPH_SZ, big);

	// 1 pixel wider vertical strokes: KROM's thin lines next to the UI's
	// heavy Latin font look washed out otherwise
	for (int y = 0; y < KROM_2X; y++)
		for (int x = KROM_2X - 1; x > 0; x--)
			big[y][x] |= big[y][x - 1];

	// box filter 30x30 -> size x size, on a grid where a source pixel is
	// 'size' units wide and an output pixel KROM_2X units
	for (int y = 0; y < size; y++)
		for (int x = 0; x < size; x++)
		{
			int sum = 0;

			for (int sy = y * KROM_2X / size; sy < KROM_2X && sy * size < (y + 1) * KROM_2X; sy++)
			{
				int wy = overlap(sy * size, (sy + 1) * size, y * KROM_2X, (y + 1) * KROM_2X);

				for (int sx = x * KROM_2X / size; sx < KROM_2X && sx * size < (x + 1) * KROM_2X; sx++)
					if (big[sy][sx])
						sum += wy * overlap(sx * size, (sx + 1) * size, x * KROM_2X, (x + 1) * KROM_2X);
			}

			bitmap[y * stride + x] = sum * 255 / (KROM_2X * KROM_2X);
		}

	return 1;
}
