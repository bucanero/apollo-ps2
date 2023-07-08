#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "menu.h"
#include "libfont.h"
#include "ps2pad.h"

#define PSP_UTILITY_OSK_MAX_TEXT_LENGTH			(512)
#define DIALOG_WINDOW_WIDTH						(300)
#define DIALOG_WINDOW_HEIGHT					(200)

int init_loading_screen(const char* msg);
void stop_loading_screen(void);

static char progress_bar[128];

static void drawDialog(const char* msg, const char *dialog_opts[], int selected)
{
    SDL_Rect dialog_rect = {
        (SCREEN_WIDTH - DIALOG_WINDOW_WIDTH)/2,
        (SCREEN_HEIGHT - DIALOG_WINDOW_HEIGHT)/2,
        DIALOG_WINDOW_WIDTH,
        DIALOG_WINDOW_HEIGHT };

    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &dialog_rect);
    DrawTexture(&menu_textures[help_png_index], dialog_rect.x +4, dialog_rect.y +4, 0, dialog_rect.w -4, dialog_rect.h -4, 0xFFFFFF00 | 0xFF);

    SetFontSize(APP_FONT_SIZE_DESCRIPTION);
    SetFontColor(0xFFFFFF00 | 0xFF, 0);
    SetFontAlign(FONT_ALIGN_SCREEN_CENTER);

    DrawString(0, dialog_rect.y + 0x10, msg);

    if (selected < 0)
    {
        DrawString(0, dialog_rect.y + 170, dialog_opts[0]);
        SetFontAlign(FONT_ALIGN_LEFT);
        return;
    }

    for (int i = 0; dialog_opts[i] != NULL; i++)
    {
        DrawString(0, dialog_rect.y + 0x30 + (i*16), dialog_opts[i]);

        if (selected == i)
        {
            DrawTexture(&menu_textures[mark_line_png_index], dialog_rect.x, dialog_rect.y + 0x30 + (i*16), 0, dialog_rect.w, menu_textures[mark_line_png_index].height, 0xFFFFFF00 | 0xFF);
//            DrawTextureCenteredX(&menu_textures[mark_arrow_png_index], MENU_ICON_OFF + MENU_TITLE_OFF, y_off, 0, (2 * APP_LINE_OFFSET) / 3, APP_LINE_OFFSET + 2, 0xFFFFFF00 | alpha);
        }
    }
    SetFontAlign(FONT_ALIGN_LEFT);
}

int show_dialog(int tdialog, const char * format, ...)
{
    va_list opt;
    char buf[512];
    const char* ok_opt[] = {"\x10 OK", NULL};
    const char* yesno_opt[] = {"\x10 Yes   \x13 No", NULL};

    va_start(opt, format);
    vsnprintf(buf, sizeof(buf), format, opt);
    va_end(opt);

    do {
        SDL_RenderClear(renderer);
        DrawBackgroundTexture(0, 0xFF);

        drawDialog(buf, (tdialog == DIALOG_TYPE_YESNO) ? yesno_opt : ok_opt, -1);
        ps2PadUpdate();
        if (ps2PadGetButtonPressed(PAD_CIRCLE))
            return 0;

        SDL_RenderPresent(renderer);
    } while (!ps2PadGetButtonPressed(PAD_CROSS));

    return (1);
}

void init_progress_bar(const char* msg)
{
    SetExtraSpace(0);
    SetCurrentFont(font_console_6x10);
    strncpy(progress_bar, msg, sizeof(progress_bar) - 1);
    init_loading_screen(progress_bar);
    SetFontSize(6, 10);
}

void end_progress_bar(void)
{
    stop_loading_screen();
    SetCurrentFont(font_adonais_regular);
    SetExtraSpace(-10);
}

void update_progress_bar(uint64_t progress, const uint64_t total_size, const char* msg)
{
    float bar_value = (100.0f * ((double) progress)) / ((double) (total_size ? total_size : ~0));

    snprintf(progress_bar, sizeof(progress_bar), "%44.0f%%", bar_value);
    bar_value /= 2.5f;

    for (int i = 0; i < 40; i++)
        progress_bar[i] = (i+1 <= (int)bar_value) ? '\xDB' : '\xB0';
}

static int convert_to_utf16(const char* utf8, uint16_t* utf16, uint32_t available)
{
    int count = 0;
    while (*utf8)
    {
        uint8_t ch = (uint8_t)*utf8++;
        uint32_t code;
        uint32_t extra;

        if (ch < 0x80)
        {
            code = ch;
            extra = 0;
        }
        else if ((ch & 0xe0) == 0xc0)
        {
            code = ch & 31;
            extra = 1;
        }
        else if ((ch & 0xf0) == 0xe0)
        {
            code = ch & 15;
            extra = 2;
        }
        else
        {
            // TODO: this assumes there won't be invalid utf8 codepoints
            code = ch & 7;
            extra = 3;
        }

        for (uint32_t i=0; i<extra; i++)
        {
            uint8_t next = (uint8_t)*utf8++;
            if (next == 0 || (next & 0xc0) != 0x80)
            {
                goto utf16_end;
            }
            code = (code << 6) | (next & 0x3f);
        }

        if (code < 0xd800 || code >= 0xe000)
        {
            if (available < 1) goto utf16_end;
            utf16[count++] = (uint16_t)code;
            available--;
        }
        else // surrogate pair
        {
            if (available < 2) goto utf16_end;
            code -= 0x10000;
            utf16[count++] = 0xd800 | (code >> 10);
            utf16[count++] = 0xdc00 | (code & 0x3ff);
            available -= 2;
        }
    }

utf16_end:
    utf16[count]=0;
    return count;
}

static int convert_from_utf16(const uint16_t* utf16, char* utf8, uint32_t size)
{
    int count = 0;
    while (*utf16)
    {
        uint32_t code;
        uint16_t ch = *utf16++;
        if (ch < 0xd800 || ch >= 0xe000)
        {
            code = ch;
        }
        else // surrogate pair
        {
            uint16_t ch2 = *utf16++;
            if (ch < 0xdc00 || ch > 0xe000 || ch2 < 0xd800 || ch2 > 0xdc00)
            {
                goto utf8_end;
            }
            code = 0x10000 + ((ch & 0x03FF) << 10) + (ch2 & 0x03FF);
        }

        if (code < 0x80)
        {
            if (size < 1) goto utf8_end;
            utf8[count++] = (char)code;
            size--;
        }
        else if (code < 0x800)
        {
            if (size < 2) goto utf8_end;
            utf8[count++] = (char)(0xc0 | (code >> 6));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 2;
        }
        else if (code < 0x10000)
        {
            if (size < 3) goto utf8_end;
            utf8[count++] = (char)(0xe0 | (code >> 12));
            utf8[count++] = (char)(0x80 | ((code >> 6) & 0x3f));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 3;
        }
        else
        {
            if (size < 4) goto utf8_end;
            utf8[count++] = (char)(0xf0 | (code >> 18));
            utf8[count++] = (char)(0x80 | ((code >> 12) & 0x3f));
            utf8[count++] = (char)(0x80 | ((code >> 6) & 0x3f));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 4;
        }
    }

utf8_end:
    utf8[count]=0;
    return count;
}

int osk_dialog_get_text(const char* title, char* text, uint32_t size)
{
/*
    int done;
    SceUtilityOskData data;
    SceUtilityOskParams params;
    uint16_t intext[PSP_UTILITY_OSK_MAX_TEXT_LENGTH];
    uint16_t outtext[PSP_UTILITY_OSK_MAX_TEXT_LENGTH];
    uint16_t desc[PSP_UTILITY_OSK_MAX_TEXT_LENGTH];

    memset(&intext, 0, sizeof(intext));
    memset(&outtext, 0, sizeof(outtext));
    memset(&desc, 0, sizeof(desc));

    convert_to_utf16(title, desc, countof(desc) - 1);
    convert_to_utf16(text, intext, countof(intext) - 1);

    memset(&data, 0, sizeof(SceUtilityOskData));
    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT; // Use system default for text input
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL; // Allow all input types
    data.desc = desc;
    data.intext = intext;
    data.outtextlength = PSP_UTILITY_OSK_MAX_TEXT_LENGTH;
    data.outtextlimit = size; // Limit input to 32 characters
    data.outtext = outtext;

    memset(&params, 0, sizeof(params));
    ConfigureDialog(&params.base, sizeof(params));
    params.datacount = 1;
    params.data = &data;

    if (sceUtilityOskInitStart(&params) < 0)
        return 0;

    do {
        done = sceUtilityOskGetStatus();
        switch(done)
        {
            case PSP_UTILITY_DIALOG_VISIBLE:
                sceUtilityOskUpdate(1);
                break;
            
            case PSP_UTILITY_DIALOG_QUIT:
                sceUtilityOskShutdownStart();
                break;
        }

        drawDialogBackground();
    } while (done != PSP_UTILITY_DIALOG_FINISHED);

    if (data.result == PSP_UTILITY_OSK_RESULT_CANCELLED)
        return 0;

    return (convert_from_utf16(data.outtext, text, size - 1));
*/
return 0;
}
