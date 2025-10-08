#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include <ahx_rpc.h>

#include "types.h"
#include "menu.h"
#include "saves.h"
#include "common.h"
#include "icons.h"


char *strcasestr(const char *, const char *);
static const char* ext_src[MAX_USB_DEVICES+1] = {"mass:/", "host:/", "cdfs:/", NULL};
static const char* sort_opt[] = {"Disabled", "by Name", "by Title ID", "by Type", NULL};

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
/*
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
*/
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

	if(apollo_config.music)
		AHX_Play();
	else
		AHX_Pause();
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

	char *buffer = readTextFile(APOLLO_LOCAL_CACHE "ver.check");
	if (!buffer)
		return;

	LOG("received %ld bytes", strlen(buffer));

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
	dbglogger_init_mode(FILE_LOGGER, USB_PATH "apollo.log", 1);
	show_message("Debug Logging Enabled!\n\n" USB_PATH "apollo.log");
}

int save_app_settings(app_config_t* config)
{
	char filePath[256];

	LOG("Apollo Save Tool v%s - Patch Engine v%s", APOLLO_VERSION, APOLLO_LIB_VERSION);
	snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "icon.sys");
	if (mkdirs(filePath) != SUCCESS)
	{
		LOG("sceSaveDataMount2 ERROR");
		return 0;
	}

	LOG("Saving Settings...");
	if (file_exists(filePath) != SUCCESS)
	{
		uLong destLen = size_icon_sys;
		Bytef *dest = malloc(destLen);

		uncompress(dest, &destLen, zicon_sys, sizeof(zicon_sys));
		write_buffer(filePath, dest, destLen);
		free(dest);

		snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "icon.ico");
		destLen = size_icon_ico;
		dest = malloc(destLen);
		uncompress(dest, &destLen, zicon_ico, sizeof(zicon_ico));
		write_buffer(filePath, dest, destLen);
		free(dest);
	}

	snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "SETTINGS.BIN");
	if (write_buffer(filePath, (uint8_t*) config, sizeof(app_config_t)) < 0)
	{
		LOG("Error saving settings!");
		return 0;
	}

	return 1;
}

int load_app_settings(app_config_t* config)
{
	char filePath[256];
	app_config_t* file_data;
	size_t file_size;

	config->user_id = 0;

	snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "SETTINGS.BIN");

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

	return 1;
}
