/* 
	Apollo PSP main.c
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <ahx_rpc.h>
#include <zlib.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libmc.h>

#include "saves.h"
#include "utils.h"
#include "common.h"
#include "ps2pad.h"

//Font
#include "libfont.h"
#include "ttf_render.h"
#include "font_adonais.h"
#include "font-8x16.h"
#include "zfont.h"

//Menus
#include "menu.h"
#include "menu_gui.h"

//Sound
extern const uint8_t _binary_data_inside_ahx_start;
extern const uint8_t _binary_data_inside_ahx_size;

// Audio player
extern const uint8_t _binary_data_ahx_irx_start;
extern const uint8_t _binary_data_ahx_irx_size;

static void *font_ttf = NULL;

#define load_menu_texture(name, type) \
	({ extern const uint8_t _binary_data_##name##_##type##_start; \
		extern const uint8_t _binary_data_##name##_##type##_size; \
		menu_textures[name##_##type##_index].buffer = &_binary_data_##name##_##type##_start; \
		menu_textures[name##_##type##_index].size = (int) &_binary_data_##name##_##type##_size; \
	}); \
		if (!LoadMenuTexture(NULL, name##_##type##_index)) return 0;

void update_usb_path(char *p);
void update_hdd_path(char *p);
void update_vmc_path(char *p);
void update_db_path(char *p);

app_config_t apollo_config = {
    .app_name = "APOLLO",
    .app_ver = APOLLO_VERSION,
    .save_db = ONLINE_URL,
    .music = 1,
    .doSort = 1,
    .doAni = 1,
    .update = 0,
    .storage = 0,
    .user_id = 0,
};

int close_app = 0;
int idle_time = 0;                          // Set by readPad

png_texture * menu_textures;                // png_texture array for main menu, initialized in LoadTexture
SDL_Window* window;                         // SDL window
SDL_Renderer* renderer;                     // SDL software renderer
uint32_t* texture_mem;                      // Pointers to texture memory
uint32_t* free_mem;                         // Pointer after last texture


/*
* HDD save list
*/
save_list_t hdd_saves = {
    .icon_id = cat_hdd_png_index,
    .title = "MemCard Saves",
    .list = NULL,
    .path = "",
    .ReadList = &ReadUserList,
    .ReadCodes = &ReadCodes,
    .UpdatePath = &update_hdd_path,
};

/*
* USB save list
*/
save_list_t usb_saves = {
    .icon_id = cat_usb_png_index,
    .title = "External Saves",
    .list = NULL,
    .path = "",
    .ReadList = &ReadUsbList,
    .ReadCodes = &ReadCodes,
    .UpdatePath = &update_usb_path,
};

/*
* PS1 VMC list
*/
save_list_t vmc1_saves = {
	.icon_id = cat_warning_png_index,
	.title = "Virtual MemCard",
    .list = NULL,
    .path = "",
    .ReadList = &ReadVmc1List,
    .ReadCodes = &ReadVmc1Codes,
    .UpdatePath = &update_vmc_path,
};

/*
* PS2 VMC list
*/
save_list_t vmc2_saves = {
	.icon_id = cat_warning_png_index,
	.title = "Virtual MemCard",
    .list = NULL,
    .path = "",
    .ReadList = &ReadVmc2List,
    .ReadCodes = &ReadVmc2Codes,
    .UpdatePath = &update_vmc_path,
};

/*
* Online code list
*/
save_list_t online_saves = {
	.icon_id = cat_db_png_index,
	.title = "Online Database",
    .list = NULL,
    .path = "",
    .ReadList = &ReadOnlineList,
    .ReadCodes = &ReadOnlineSaves,
    .UpdatePath = &update_db_path,
};

/*
* User Backup code list
*/
save_list_t user_backup = {
    .icon_id = cat_bup_png_index,
    .title = "User Tools",
    .list = NULL,
    .path = "",
    .ReadList = &ReadBackupList,
    .ReadCodes = &ReadBackupCodes,
    .UpdatePath = NULL,
};


static const char* get_menu_help(int id)
{
	switch (id)
	{
	case MENU_PS1VMC_SAVES:
	case MENU_PS2VMC_SAVES:
	case MENU_USB_SAVES:
	case MENU_HDD_SAVES:
	case MENU_ONLINE_DB:
		return "\x10 Select    \x13 Back    \x12 Details    \x11 Refresh";

	case MENU_HEX_EDITOR:
		return "\x10 Value Up  \x11 Value Down   \x13 Exit";

	case MENU_CREDITS:
	case MENU_PATCH_VIEW:
	case MENU_SAVE_DETAILS:
		return "\x13 Back";

	case MENU_USER_BACKUP:
		return "\x10 Select    \x13 Back    \x11 Refresh";

	case MENU_SETTINGS:
	case MENU_CODE_OPTIONS:
		return "\x10 Select    \x13 Back";

	case MENU_PATCHES:
		return "\x10 Select    \x12 View Code    \x13 Back";
	}

	return "";
}

static int initPad(void)
{
    // Set sampling mode
    if (ps2PadInit() < 0)
    {
        LOG("[ERROR] Failed to open pad!");
        return 0;
    }

    return 1;
}

// Used only in initialization. Allocates 64 mb for textures and loads the font
static int LoadTextures_Menu(void)
{
	font_ttf = malloc(size_font_ttf);
	texture_mem = malloc(256 * 8 * 4);
	menu_textures = (png_texture *)calloc(TOTAL_MENU_TEXTURES, sizeof(png_texture));

	if(!texture_mem || !menu_textures || !font_ttf)
		return 0; // fail!

	ResetFont();
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) data_font_Adonais, (u8 *) texture_mem, 0x20, 0x7e, 32, 31, 1, BIT7_FIRST_PIXEL);
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) console_font_8x16, (u8 *) free_mem, 0, 0xFF, 8, 16, 1, BIT7_FIRST_PIXEL);

	uncompress(font_ttf, &size_font_ttf, (const Bytef *) zfont, sizeof(zfont));
	if (TTFLoadFont(0, NULL, font_ttf, size_font_ttf) != SUCCESS)
		return 0;

	free_mem = (u32*) init_ttf_table((u8*) free_mem);
	set_ttf_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WIN_SKIP_LF);

	//Init Main Menu textures
	load_menu_texture(bgimg, png);
	load_menu_texture(cheat, png);
//	load_menu_texture(leon, jpg);

	load_menu_texture(circle_loading_bg, png);
	load_menu_texture(circle_loading_seek, png);
	load_menu_texture(edit_shadow, png);

	load_menu_texture(footer_ico_circle, png);
	load_menu_texture(footer_ico_cross, png);
	load_menu_texture(footer_ico_square, png);
	load_menu_texture(footer_ico_triangle, png);
	load_menu_texture(header_dot, png);
	load_menu_texture(header_line, png);

	load_menu_texture(mark_arrow, png);
	load_menu_texture(mark_line, png);
	load_menu_texture(opt_off, png);
	load_menu_texture(opt_on, png);
	load_menu_texture(scroll_bg, png);
	load_menu_texture(scroll_lock, png);
	load_menu_texture(help, png);
	load_menu_texture(buk_scr, png);
	load_menu_texture(cat_about, png);
	load_menu_texture(cat_cheats, png);
	load_menu_texture(cat_opt, png);
	load_menu_texture(cat_usb, png);
	load_menu_texture(cat_bup, png);
	load_menu_texture(cat_db, png);
	load_menu_texture(cat_hdd, png);
	load_menu_texture(cat_sav, png);
	load_menu_texture(cat_warning, png);
	load_menu_texture(column_1, png);
	load_menu_texture(column_2, png);
	load_menu_texture(column_3, png);
	load_menu_texture(column_4, png);
	load_menu_texture(column_5, png);
	load_menu_texture(column_6, png);
	load_menu_texture(column_7, png);
	load_menu_texture(jar_about, png);
	load_menu_texture(jar_about_hover, png);
	load_menu_texture(jar_bup, png);
	load_menu_texture(jar_bup_hover, png);
	load_menu_texture(jar_db, png);
	load_menu_texture(jar_db_hover, png);
	load_menu_texture(jar_empty, png);
	load_menu_texture(jar_hdd, png);
	load_menu_texture(jar_hdd_hover, png);
	load_menu_texture(jar_opt, png);
	load_menu_texture(jar_opt_hover, png);
	load_menu_texture(jar_usb, png);
	load_menu_texture(jar_usb_hover, png);
	load_menu_texture(logo, png);
	load_menu_texture(logo_text, png);
	load_menu_texture(tag_lock, png);
	load_menu_texture(tag_own, png);
	load_menu_texture(tag_pack, png);
	load_menu_texture(tag_ps1, png);
	load_menu_texture(tag_ps2, png);
	load_menu_texture(tag_vmc, png);
	load_menu_texture(tag_warning, png);
	load_menu_texture(tag_zip, png);
	load_menu_texture(tag_net, png);
	load_menu_texture(tag_apply, png);
	load_menu_texture(tag_transfer, png);

	load_menu_texture(trp_sync, png);
	load_menu_texture(trp_gold, png);

	menu_textures[icon_png_file_index].texture = NULL;

	u32 tBytes = free_mem - texture_mem;
	LOG("LoadTextures_Menu() :: Allocated %db (%.02fkb, %.02fmb) for textures", tBytes, tBytes / (float)1024, tBytes / (float)(1024 * 1024));
	return 1;
}

static void LoadSounds(void)
{
	AHX_Init();
	AHX_LoadSongBuffer((void*) &_binary_data_inside_ahx_start, (int) &_binary_data_inside_ahx_size);
	AHX_SubSong(0);
}

void update_usb_path(char* path)
{
	sprintf(path, "%s", menu_options[3].options[apollo_config.storage]);
	if (dir_exists(path) == SUCCESS)
		return;

	path[0] = 0;
}

void update_hdd_path(char* path)
{
	int sel;
	const char* memcard_opt[] = {"Memory Card 1", "Memory Card 2", NULL};

	path[0] = 0;
	if (dir_exists(MC0_PATH) == SUCCESS && dir_exists(MC1_PATH) == SUCCESS)
	{
		if ((sel = show_multi_dialog(memcard_opt, "Select Memory Card")) < 0)
			return;

		strcpy(path, sel ? MC1_PATH : MC0_PATH);
		return;
	}
	else if (dir_exists(MC0_PATH) == SUCCESS)
		strcpy(path, MC0_PATH);

	else if (dir_exists(MC1_PATH) == SUCCESS)
		strcpy(path, MC1_PATH);
}

void update_vmc_path(char* path)
{
	if (file_exists(path) == SUCCESS)
		return;

	path[0] = 0;
}

void update_db_path(char* path)
{
	strcpy(path, apollo_config.save_db);
}

static void registerSpecialChars(void)
{
	// Register save tags
	RegisterSpecialCharacter(CHAR_TAG_PS1, 0, 1.5, &menu_textures[tag_ps1_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS2, 0, 1.5, &menu_textures[tag_ps2_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_VMC, 0, 1.0, &menu_textures[tag_vmc_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PACK, 0, 0.8, &menu_textures[tag_pack_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_LOCKED, 0, 1.3, &menu_textures[tag_lock_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_OWNER, 0, 1.3, &menu_textures[tag_own_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_WARNING, 0, 1.3, &menu_textures[tag_warning_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_APPLY, 2, 1.0, &menu_textures[tag_apply_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_ZIP, 0, 1.0, &menu_textures[tag_zip_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_NET, 0, 1.0, &menu_textures[tag_net_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_TRANSFER, 0, 1.0, &menu_textures[tag_transfer_png_index]);

	// Register button icons
	RegisterSpecialCharacter(CHAR_BTN_X, 0, 1.2, &menu_textures[footer_ico_cross_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_S, 0, 1.2, &menu_textures[footer_ico_square_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_T, 0, 1.2, &menu_textures[footer_ico_triangle_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_O, 0, 1.2, &menu_textures[footer_ico_circle_png_index]);

	// Register trophy icons
	RegisterSpecialCharacter(CHAR_TRP_GOLD, 2, 0.9f, &menu_textures[trp_gold_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_SYNC, 0, 1.2f, &menu_textures[trp_sync_png_index]);
}

static void terminate(void)
{
	LOG("Exiting...");

	AHX_Quit();
}

static int initInternal(void)
{
	// load common modules
	int ret;

	// Initialise
	SifInitRpc(0);

	if(mcInit(MC_TYPE_XMC) < 0) {
		LOG("Failed to initialise memcard server!");
		return(0);
	}

	return 1;
}

/*
	Program start
*/
int main(int argc, char *argv[])
{
#ifdef APOLLO_ENABLE_LOGGING
	// Frame tracking info for debugging
	uint32_t lastFrameTicks  = 0;
	uint32_t startFrameTicks = 0;
	uint32_t deltaFrameTicks = 0;

	dbglogger_init_mode(TTY_LOGGER, NULL, 0);
#endif

	// Initialize SDL functions
	LOG("Initializing SDL");
	if (SDL_Init(SDL_INIT_VIDEO) != SUCCESS)
	{
		LOG("Failed to initialize SDL: %s", SDL_GetError());
		return (-1);
	}

	initInternal();
	http_init();
	initPad();

	// Open a handle to audio output device
	if (SifExecModuleBuffer((void*) &_binary_data_ahx_irx_start, (int) &_binary_data_ahx_irx_size, 0, NULL, NULL) < 0)
	{
		LOG("[ERROR] Failed to load module: AHX");
		return (-1);
	}

	// Create a window context
	LOG("Creating a window");
	window = SDL_CreateWindow("main", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_FULLSCREEN);
	if (!window) {
		LOG("SDL_CreateWindow: %s", SDL_GetError());
		return (-1);
	}

	// Create a renderer (OpenGL ES2)
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		LOG("SDL_CreateRenderer: %s", SDL_GetError());
		return (-1);
	}

	mkdirs(APOLLO_DATA_PATH);
	mkdirs(APOLLO_LOCAL_CACHE);

	// Load texture
	if (!LoadTextures_Menu())
	{
		LOG("Failed to load menu textures!");
		return (-1);
	}
	registerSpecialChars();
	initMenuOptions();
	LoadSounds();

	// Load application settings
	load_app_settings(&apollo_config);

	// Unpack application data on first run
	if (file_exists(APOLLO_LOCAL_CACHE "appdata.zip") == SUCCESS)
	{
//		clean_directory(APOLLO_DATA_PATH);
		if (extract_zip(APOLLO_LOCAL_CACHE "appdata.zip", APOLLO_DATA_PATH))
			show_message("Successfully installed local application data");

		unlink_secure(APOLLO_LOCAL_CACHE "appdata.zip");
	}

	// dedicated to Luna ~ in loving memory (2011 - 2023)
	// Splash screen logo (fade-in)
	drawSplashLogo(1);
	sleep(2);
 
	// Setup font
	SetExtraSpace(-10);
	SetCurrentFont(font_adonais_regular);

	// Splash screen logo (fade-out)
	drawSplashLogo(-1);
	SDL_DestroyTexture(menu_textures[buk_scr_png_index].texture);
	
	//Set options
	update_callback(!apollo_config.update);

	// Start BGM audio thread
	music_callback(!apollo_config.music);
	Draw_MainMenu_Ani();

	while (!close_app)
	{
#ifdef APOLLO_ENABLE_LOGGING
        startFrameTicks = SDL_GetTicks();
        deltaFrameTicks = startFrameTicks - lastFrameTicks;
        lastFrameTicks  = startFrameTicks;
#endif
		// Clear the canvas
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x80);
		SDL_RenderClear(renderer);

		ps2PadUpdate();
		drawScene();

		//Draw help
		if (menu_id)
		{
			u8 alpha = 0xFF;
			if (ps2PadGetConf()->idle > 0x100)
			{
				int dec = (ps2PadGetConf()->idle - 0x100) * 4;
				alpha = (dec > 0xFF) ? 0 : (alpha - dec);
			}
			
			SetFontSize(APP_FONT_SIZE_DESCRIPTION);
			SetCurrentFont(font_adonais_regular);
			SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
			SetFontColor(APP_FONT_COLOR | alpha, 0);
			DrawString(0, SCREEN_HEIGHT - 40, get_menu_help(menu_id));
			SetFontAlign(FONT_ALIGN_LEFT);
		}

#ifdef APOLLO_ENABLE_LOGGING
		// Calculate FPS and ms/frame
		SetFontColor(APP_FONT_COLOR | 0xFF, 0);
		DrawFormatString(50, 500, "FPS: %d", (1000 / deltaFrameTicks));
#endif
		// Propagate the updated window to the screen
		SDL_RenderPresent(renderer);
	}

	drawEndLogo();

	// Cleanup resources
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	// Stop all SDL sub-systems
	SDL_Quit();
	http_end();
	ps2PadFinish();
	terminate();

	return 0;
}
