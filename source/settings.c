#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "types.h"
#include "menu.h"
#include "saves.h"
#include "common.h"
#include "icon0.h"
#include "plugin.h"

#define PSP_UTILITY_COMMON_RESULT_OK (0)
#define GAME_PLUGIN_PATH             "ms0:/seplugins/game.txt"
#define SGKEY_DUMP_PLUGIN_PATH       "ms0:/seplugins/SGKeyDumper.prx"

char *strcasestr(const char *, const char *);
static char* ext_src[MAX_USB_DEVICES+1] = {"ms0", "ef0", NULL};
static char* sort_opt[] = {"Disabled", "by Name", "by Title ID", NULL};

menu_option_t menu_options[] = {
	{ .name = "\nBackground Music", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.music, 
		.callback = music_callback 
	},
	{ .name = "Menu Animations", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.doAni, 
		.callback = ani_callback 
	},
	{ .name = "Sort Saves", 
		.options = (char**) sort_opt,
		.type = APP_OPTION_LIST,
		.value = &apollo_config.doSort, 
		.callback = sort_callback 
	},
	{ .name = "External Saves Source",
		.options = (char**) ext_src,
		.type = APP_OPTION_LIST,
		.value = &apollo_config.storage,
		.callback = owner_callback
	},
	{ .name = "Version Update Check", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.update, 
		.callback = update_callback 
	},
	{ .name = "Update Application Data", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = upd_appdata_callback 
	},
	{ .name = "Clear Local Cache", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = clearcache_callback 
	},
	{ .name = "Enable Debug Log",
		.options = NULL,
		.type = APP_OPTION_CALL,
		.value = NULL,
		.callback = log_callback 
	},
	{ .name = NULL }
};


void music_callback(int sel)
{
	apollo_config.music = !sel;
}

void sort_callback(int sel)
{
	apollo_config.doSort = sel;
}

void ani_callback(int sel)
{
	apollo_config.doAni = !sel;
}

void clearcache_callback(int sel)
{
	LOG("Cleaning folder '%s'...", APOLLO_LOCAL_CACHE);
	clean_directory(APOLLO_LOCAL_CACHE);

	show_message("Local cache folder cleaned:\n" APOLLO_LOCAL_CACHE);
}

void upd_appdata_callback(int sel)
{
	int i;

	if (!http_download(ONLINE_PATCH_URL, "apollo-psp-update.zip", APOLLO_LOCAL_CACHE "appdata.zip", 1))
	{
		show_message("Error! Can't download data update pack!");
		return;
	}

	if ((i = extract_zip(APOLLO_LOCAL_CACHE "appdata.zip", APOLLO_DATA_PATH)) > 0)
		show_message("Successfully updated %d data files!", i);
	else
		show_message("Error! Can't extract data update pack!");

	unlink_secure(APOLLO_LOCAL_CACHE "appdata.zip");
}

void update_callback(int sel)
{
    apollo_config.update = !sel;

    if (!apollo_config.update)
        return;

	LOG("checking latest Apollo version at %s", APOLLO_UPDATE_URL);

	if (!http_download(APOLLO_UPDATE_URL, NULL, APOLLO_LOCAL_CACHE "ver.check", 0))
	{
		LOG("http request to %s failed", APOLLO_UPDATE_URL);
		return;
	}

	char *buffer;
	long size = 0;

	buffer = readTextFile(APOLLO_LOCAL_CACHE "ver.check", &size);

	if (!buffer)
		return;

	LOG("received %ld bytes", size);

	static const char find[] = "\"name\":\"Apollo Save Tool v";
	const char* start = strstr(buffer, find);
	if (!start)
	{
		LOG("no name found");
		goto end_update;
	}

	LOG("found name");
	start += sizeof(find) - 1;

	char* end = strchr(start, '"');
	if (!end)
	{
		LOG("no end of name found");
		goto end_update;
	}
	*end = 0;
	LOG("latest version is %s", start);

	if (strcasecmp(APOLLO_VERSION, start) == 0)
	{
		LOG("no need to update");
		goto end_update;
	}

	start = strstr(end+1, "\"browser_download_url\":\"");
	if (!start)
		goto end_update;

	start += 24;
	end = strchr(start, '"');
	if (!end)
	{
		LOG("no download URL found");
		goto end_update;
	}

	*end = 0;
	LOG("download URL is %s", start);

	if (show_dialog(DIALOG_TYPE_YESNO, "New version available! Download update?"))
	{
		if (http_download(start, NULL, "ms0:/APOLLO/apollo-psp.zip", 1))
			show_message("Update downloaded to ms0:/APOLLO/apollo-psp.zip");
		else
			show_message("Download error!");
	}

end_update:
	free(buffer);
	return;
}

void owner_callback(int sel)
{
	apollo_config.storage = sel;
}

void log_callback(int sel)
{
	dbglogger_init_mode(FILE_LOGGER, APOLLO_PATH "apollo.log", 1);
	show_message("Debug Logging Enabled!\n\n" APOLLO_PATH "apollo.log");
}

/*
static void initSavedata(SceUtilitySavedataParam * savedata, int mode, void* usr_data)
{
	memset(savedata, 0, sizeof(SceUtilitySavedataParam));
	savedata->base.size = sizeof(SceUtilitySavedataParam);
	savedata->base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	savedata->base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	savedata->base.graphicsThread = 0x11;
	savedata->base.accessThread = 0x13;
	savedata->base.fontThread = 0x12;
	savedata->base.soundThread = 0x10;
	savedata->mode = mode;
	savedata->overwrite = 1;
	savedata->focus = PSP_UTILITY_SAVEDATA_FOCUS_LATEST; // Set initial focus to the newest file (for loading)

	strncpy(savedata->key, "bucanero.com.ar", sizeof(savedata->key));
	strncpy(savedata->gameName, "NP0APOLLO", sizeof(savedata->gameName));	// First part of the save name, game identifier name
	strncpy(savedata->saveName, "-Settings", sizeof(savedata->saveName));	// Second part of the save name, save identifier name
	strncpy(savedata->fileName, "SETTINGS.BIN", sizeof(savedata->fileName));	// name of the data file

	// Allocate buffers used to store various parts of the save data
	savedata->dataBuf = usr_data;
	savedata->dataBufSize = sizeof(app_config_t);
	savedata->dataSize = sizeof(app_config_t);

	// Set save data
	if (mode == PSP_UTILITY_SAVEDATA_AUTOSAVE)
	{
		strcpy(savedata->sfoParam.title, "Apollo Save Tool");
		strcpy(savedata->sfoParam.savedataTitle,"Settings");
		strcpy(savedata->sfoParam.detail,"www.bucanero.com.ar");
		savedata->sfoParam.parentalLevel = 1;

		// Set icon0
		savedata->icon0FileData.buf = icon0;
		savedata->icon0FileData.bufSize = size_icon0;
		savedata->icon0FileData.size = size_icon0;
		savedata->focus = PSP_UTILITY_SAVEDATA_FOCUS_FIRSTEMPTY; // If saving, set inital focus to the first empty slot
	}
}

static int runSaveDialog(int mode, void* data)
{
	SceUtilitySavedataParam dialog;

	initSavedata(&dialog, mode, data);
	if (sceUtilitySavedataInitStart(&dialog) < 0)
		return 0;

	do {
		mode = sceUtilitySavedataGetStatus();
		switch(mode)
		{
			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilitySavedataUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilitySavedataShutdownStart();
				break;
		}
	} while (mode != PSP_UTILITY_DIALOG_FINISHED);

	return (dialog.base.result == PSP_UTILITY_COMMON_RESULT_OK);
}
*/

int save_app_settings(app_config_t* config)
{
	char filePath[256];
	Byte dest[4912];
	uLong destLen = sizeof(dest);

	LOG("Apollo Save Tool v%s - Patch Engine v%s", APOLLO_VERSION, APOLLO_LIB_VERSION);
	snprintf(filePath, sizeof(filePath), "%s%s%s%s", MS0_PATH, USER_PATH_HDD, "NP0APOLLO-Settings/", "ICON0.PNG");
	if (mkdirs(filePath) != SUCCESS)
	{
		LOG("sceSaveDataMount2 ERROR");
		return 0;
	}

	LOG("Saving Settings...");
	write_buffer(filePath, icon0, size_icon0);

	snprintf(filePath, sizeof(filePath), "%s%s%s%s", MS0_PATH, USER_PATH_HDD, "NP0APOLLO-Settings/", "PARAM.SFO");
	uncompress(dest, &destLen, paramsfo, size_paramsfo);
	write_buffer(filePath, dest, destLen);

	snprintf(filePath, sizeof(filePath), "%s%s%s%s", MS0_PATH, USER_PATH_HDD, "NP0APOLLO-Settings/", "SETTINGS.BIN");
	if (write_buffer(filePath, (uint8_t*) config, sizeof(app_config_t)) < 0)
	{
		LOG("Error saving settings!");
		return 0;
	}
/*
	if (!runSaveDialog(PSP_UTILITY_SAVEDATA_AUTOSAVE, config))
	{
		LOG("Save ERROR");
		return 0;
	}
*/
	return 1;
}

int load_app_settings(app_config_t* config)
{
	char filePath[256];
	app_config_t* file_data;
	size_t file_size;

	config->user_id = 0;

//	Byte dest[4912];
//	uLong dlen = sizeof(dest);
//	snprintf(filePath, sizeof(filePath), "%s%s%s%s", MS0_PATH, USER_PATH_HDD, "NP0APOLLO-Settings/", "PARAM.SFO");
//	read_buffer(filePath, (uint8_t**) &file_data, &file_size);
//	compress2(dest, &dlen, (Bytef*) file_data, file_size, 9);
//	write_buffer(filePath, dest, dlen);

	snprintf(filePath, sizeof(filePath), "%s%s%s%s", MS0_PATH, USER_PATH_HDD, "NP0APOLLO-Settings/", "SETTINGS.BIN");

	LOG("Loading Settings...");
	if (read_buffer(filePath, (uint8_t**) &file_data, &file_size) == SUCCESS && file_size == sizeof(app_config_t))
	{
		file_data->user_id = config->user_id;
		memcpy(config, file_data, file_size);

		LOG("Settings loaded: UserID (%08x)", config->user_id);
		free(file_data);
	}
	else
	{
		LOG("Settings not found, using defaults");
		save_app_settings(config);
		return 0;
	}
/*
	if (!runSaveDialog(PSP_UTILITY_SAVEDATA_AUTOLOAD, config))
	{
		LOG("Load ERROR");
		memcpy(config, &tmp_data, sizeof(app_config_t));
		return 0;
	}
*/

	return 1;
}

int install_sgkey_plugin(int install)
{
	char* data;
	size_t size;

	mkdirs(SGKEY_DUMP_PLUGIN_PATH);
	if (write_buffer(SGKEY_DUMP_PLUGIN_PATH, sgk_plugin, size_sgk_plugin) < 0)
		return 0;

	if (read_buffer(GAME_PLUGIN_PATH, (uint8_t**) &data, &size) < 0)
	{
		LOG("Error reading game.txt");
		if (!install)
			return 0;

		if (write_buffer(GAME_PLUGIN_PATH, SGKEY_DUMP_PLUGIN_PATH " 1\n", 33) < 0)
		{
			LOG("Error creating game.txt");
			return 0;
		}

		return 1;
	}

	if (install)
	{
		char *ptr = strcasestr(data, SGKEY_DUMP_PLUGIN_PATH " ");
		if (ptr != NULL && (ptr[31] == '1' || ptr[31] == '0'))
		{
			LOG("Plugin enabled");
			ptr[31] = '1';
			write_buffer(GAME_PLUGIN_PATH, data, size);
			free(data);

			return 1;
		}
		free(data);

		FILE* fp = fopen(GAME_PLUGIN_PATH, "a");
		if (!fp)
		{
			LOG("Error opening game.txt");
			return 0;
		}

		fprintf(fp, "%s%s", SGKEY_DUMP_PLUGIN_PATH, " 1\n");
		fclose(fp);
		return 1;
	}

	if (!install)
	{
		char *ptr = strcasestr(data, SGKEY_DUMP_PLUGIN_PATH " ");
		if (ptr != NULL && (ptr[31] == '1' || ptr[31] == '0'))
		{
			LOG("Plugin disabled");
			ptr[31] = '0';
			write_buffer(GAME_PLUGIN_PATH, data, size);
		}
		free(data);
		return 1;
	}

	return 0;
}
