#include <unistd.h>
#include <string.h>

#include "saves.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"

#define FONT_W  24
#define FONT_H  23
#define STEP_X  -2         // horizontal displacement

static int sx = SCREEN_WIDTH;

const char * menu_about_strings[] = { "Bucanero", "Developer",
									"", "",
									"", "",
									"Leon  ", "  Luna",
									"2009-2022", "2011-2023",
									NULL };

/***********************************************************************
* Draw a string of chars, amplifing y by sin(x)
***********************************************************************/
static void draw_sinetext(int y, const char* string)
{
    int x = sx;       // every call resets the initial x
    int sl = strlen(string);
    char tmp[2] = {0, 0};
    float amp;

    SetFontSize(FONT_W, FONT_H);
    for(int i = 0; i < sl; i++)
    {
        amp = sinf(x      // testing sinf() from math.h
                 * 0.02)  // it turns out in num of bends
                 * 5;    // +/- vertical bounds over y

        if(x > 0 && x < SCREEN_WIDTH - FONT_W)
        {
            tmp[0] = string[i];
            DrawStringMono(x, y + amp, tmp);
        }

        x += FONT_W;
    }

    //* Move string by defined step
    sx += STEP_X;

    if(sx + (sl * FONT_W) < 0)           // horizontal bound, then loop
        sx = SCREEN_WIDTH + FONT_W;
}

static void _draw_AboutMenu(u8 alpha)
{
	u8 alp2 = ((alpha*2) > 0xFF) ? 0xFF : (alpha * 2); 
    
    //------------- About Menu Contents
	DrawTextureCenteredX(&menu_textures[logo_text_png_index], SCREEN_WIDTH/2, 50, 0, menu_textures[logo_text_png_index].width * 3/4, menu_textures[logo_text_png_index].height * 3/4, 0xFFFFFF00 | alp2);

    SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetCurrentFont(font_adonais_regular);
	SetFontColor(APP_FONT_MENU_COLOR | alpha, 0);
	SetFontSize(APP_FONT_SIZE_JARS);
	DrawStringMono(0, 120, "PlayStation 2 version");
	DrawStringMono(0, 212, "In memory of");
    
    for (int cnt = 0; menu_about_strings[cnt] != NULL; cnt += 2)
    {
        SetFontAlign(FONT_ALIGN_RIGHT);
		DrawStringMono((SCREEN_WIDTH / 2) - 20, 180 + (cnt * 12), menu_about_strings[cnt]);
        
		SetFontAlign(FONT_ALIGN_LEFT);
		DrawStringMono((SCREEN_WIDTH / 2) + 20, 180 + (cnt * 12), menu_about_strings[cnt + 1]);
    }

	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetFontColor(APP_FONT_COLOR | alpha, 0);
	SetFontSize(APP_FONT_SIZE_SELECTION);

	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetCurrentFont(font_adonais_regular);
	SetFontColor(APP_FONT_MENU_COLOR | alp2, 0);
	SetFontSize(APP_FONT_SIZE_JARS);
	DrawStringMono(0, 350, "www.bucanero.com.ar");
	SetFontAlign(FONT_ALIGN_LEFT);
}

void Draw_AboutMenu_Ani(void)
{
	int ani = 0;
	for (ani = 0; ani < MENU_ANI_MAX; ani++)
	{
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);

		DrawHeader_Ani(cat_about_png_index, "About", "v" APOLLO_VERSION, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

		//------------- About Menu Contents

		int rate = (0x100 / (MENU_ANI_MAX - 0x60));
		u8 about_a = (u8)((((ani - 0x60) * rate) > 0xFF) ? 0xFF : ((ani - 0x60) * rate));
		if (ani < 0x60)
			about_a = 0;

		_draw_AboutMenu(about_a);

		SDL_RenderPresent(renderer);

		if (about_a == 0xFF)
			return;
	}
}

static void _draw_LeonLuna(void)
{
	DrawTextureCentered(&menu_textures[leon_luna_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 0, menu_textures[leon_luna_png_index].width, menu_textures[leon_luna_png_index].height, 0xFFFFFF00 | 0xFF);
//	DrawTexture(&menu_textures[help_png_index], 0, 320, 0, SCREEN_WIDTH + 10, 40, 0xFFFFFF00 | 0xFF);

	SetFontColor(APP_FONT_COLOR | 0xFF, 0);
	draw_sinetext(330, "... in memory of Leon & Luna - may your days be filled with eternal joy ...");
}

void Draw_AboutMenu(int ll)
{
	if (ll)
		return(_draw_LeonLuna());

	DrawHeader(cat_about_png_index, 0, "About", "v" APOLLO_VERSION, APP_FONT_TITLE_COLOR | 0xFF, 0xffffffff, 0);
	_draw_AboutMenu(0xFF);
}
