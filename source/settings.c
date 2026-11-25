#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "types.h"
#include "menu.h"
#include "saves.h"
#include "ramcache.h"
#include "music.h"
#include "common.h"
#include "icons.h"
#include "mx4sio.h"


char *strcasestr(const char *, const char *);
// New sources go last: apollo_config.storage (saved in SETTINGS.BIN) is an index into this list
static const char* ext_src[MAX_USB_DEVICES+1] = {USB_PATH, HOST_PATH, DISC_PATH, MX4SIO_PATH, NULL};
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
	{ .name = "Check for Updates", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = update_callback 
	},
/*
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
	{ .name = "Debug Log",
		.options = NULL,
		.type = APP_OPTION_BOOL,
		.value = &apollo_config.dbglog,
		.callback = log_callback 
	},
	{ .name = NULL }
};


void music_callback(int sel)
{
	apollo_config.music = !sel;

	if(apollo_config.music)
		music_play();
	else
		music_pause();
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
	if (ramcache_enabled())
	{
		LOG("Cleaning RAM cache...");
		show_message("RAM cache cleaned (%d files)", ramcache_clear());
		return;
	}

	LOG("Cleaning folder '%s'...", APOLLO_LOCAL_CACHE);
	clean_directory(APOLLO_LOCAL_CACHE);

	show_message("Local cache folder cleaned:\n" APOLLO_LOCAL_CACHE);
}

void upd_appdata_callback(int sel)
{
	int i;

	if (!http_download(ONLINE_PATCH_URL, "apollo-ps2-update.zip", APOLLO_LOCAL_CACHE "appdata.zip", 1))
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

// "1.4.0" -> 0x010400, so versions compare as numbers
static uint32_t version_number(const char* version)
{
	unsigned int major = 0, minor = 0, patch = 0;

	sscanf(version, "%u.%u.%u", &major, &minor, &patch);
	return ((major & 0xFF) << 16) | ((minor & 0xFF) << 8) | (patch & 0xFF);
}

// The apollo-ps2.zip download in a GitHub release (JSON), NULL if missing.
// Found by name: the release has other assets too (e.g. a disc image).
static char* find_release_zip(char* json)
{
	static const char find[] = "\"browser_download_url\":\"";
	char *url, *end;

	for (url = strstr(json, find); url; url = strstr(end + 1, find))
	{
		url += sizeof(find) - 1;
		if ((end = strchr(url, '"')) == NULL)
			return NULL;

		if (end - url > 15 && strncmp(end - 15, "/apollo-ps2.zip", 15) == 0)
		{
			*end = 0;
			return url;
		}
	}

	return NULL;
}

// Checks the latest release on GitHub, and reports the result on screen
void update_callback(int sel)
{
	static const char find[] = "\"name\":\"Apollo Save Tool v";
	char *buffer, *latest, *end, *url;

	LOG("checking latest Apollo version at %s", APOLLO_UPDATE_URL);

	if (!network_up())
	{
		LOG("no network");
		show_message("Can't check for updates: no network connection\n\n(PS2 Network Adapter and DHCP required)");
		return;
	}

	if (!http_download(APOLLO_UPDATE_URL, NULL, APOLLO_LOCAL_CACHE "ver.check", 0))
	{
		LOG("http request to %s failed", APOLLO_UPDATE_URL);
		show_message("Can't check for updates:\nGitHub couldn't be reached");
		return;
	}

	buffer = readTextFile(APOLLO_LOCAL_CACHE "ver.check");
	unlink_secure(APOLLO_LOCAL_CACHE "ver.check");
	if (!buffer)
		return;

	LOG("received %d bytes", (int) strlen(buffer));

	if ((latest = strstr(buffer, find)) == NULL || (end = strchr(latest + sizeof(find) - 1, '"')) == NULL)
	{
		LOG("no release name found");
		show_message("Can't check for updates:\nunexpected reply from GitHub");
		goto end_update;
	}

	latest += sizeof(find) - 1;
	*end = 0;
	LOG("latest version is %s (running %s)", latest, APOLLO_VERSION);

	if (version_number(latest) <= version_number(APOLLO_VERSION))
	{
		show_message("Apollo Save Tool v%s is up to date\n\n(latest release: v%s)", APOLLO_VERSION, latest);
		goto end_update;
	}

	url = find_release_zip(end + 1);
	LOG("download URL is %s", url ? url : "(not found)");

	// No USB drive (files are kept in RAM) or no zip: just say where to get it
	if (!url || ramcache_enabled())
	{
		show_message("New version available: v%s\n\nDownload it from:\ngithub.com/bucanero/apollo-ps2/releases", latest);
		goto end_update;
	}

	if (show_dialog(DIALOG_TYPE_YESNO, "New version available: v%s\n\nDownload apollo-ps2.zip to\n%s ?", latest, APOLLO_APP_PATH))
	{
		if (http_download(url, NULL, APOLLO_APP_PATH "apollo-ps2.zip", 1))
			show_message("Update downloaded to:\n" APOLLO_APP_PATH "apollo-ps2.zip");
		else
			show_message("Error! Can't download the update");
	}

end_update:
	free(buffer);
}

void owner_callback(int sel)
{
	apollo_config.storage = sel;
}

#define APOLLO_LOG_FILE		USB_PATH "apollo.log"

// Back to the build's own logging: the console (TTY) in debug builds, none otherwise
static void log_default(void)
{
	dbglogger_stop();
#ifdef APOLLO_ENABLE_LOGGING
	dbglogger_init_mode(TTY_LOGGER, NULL, 0);
#endif
}

// overwrite: start a new log file instead of adding to it
static int log_to_file(int overwrite)
{
	if (dbglogger_init_mode(FILE_LOGGER, APOLLO_LOG_FILE, overwrite) == FILE_LOGGER)
	{
		LOG("Apollo Save Tool v%s - Patch Engine v%s", APOLLO_VERSION, APOLLO_LIB_VERSION);
		return 1;
	}

	// no USB drive
	log_default();
	return 0;
}

void log_callback(int sel)
{
	apollo_config.dbglog = !sel;

	if (!apollo_config.dbglog)
	{
		LOG("Debug log disabled");
		log_default();
		show_message("Debug Logging Disabled");
		return;
	}

	if (!log_to_file(1))
	{
		apollo_config.dbglog = 0;
		show_message("Error! Can't create the log file:\n" APOLLO_LOG_FILE);
		return;
	}

	show_message("Debug Logging Enabled!\n\n" APOLLO_LOG_FILE);
}

/*
 * Settings folder: where the app was launched from (argv[0])
 *
 * Launched from a Memory Card or USB drive, SETTINGS.BIN is kept next to the
 * ELF. Anything else (disc, HDD, host, other devices, no argv[0]) uses the
 * default Memory Card folder. The path is normalized as wLaunchELF does:
 * '\' -> '/', "massN:"/"usb[N]:" -> "mass:", and a missing '/' after the
 * device ("mc0:APPS/x.elf") is added.
 */
#define SETTINGS_DIR_DEFAULT	MC0_PATH "APOLLO-99PS2/"
#define SETTINGS_FILE			"SETTINGS.BIN"

static char settings_dir[256] = SETTINGS_DIR_DEFAULT;

// "mass", "mass0".."mass9", "usb", "usb0".."usb9", followed by ':'; returns the device name length
static int usb_device_len(const char* path)
{
	int len;

	if (strncmp(path, "mass", 4) == 0)
		len = 4;
	else if (strncmp(path, "usb", 3) == 0)
		len = 3;
	else
		return 0;

	if (path[len] >= '0' && path[len] <= '9')
		len++;

	return (path[len] == ':') ? len : 0;
}

void set_settings_path(const char* argv0)
{
	char path[256];
	const char* rest;
	char* p;

	snprintf(settings_dir, sizeof(settings_dir), "%s", SETTINGS_DIR_DEFAULT);

	if (!argv0 || !argv0[0])
	{
		LOG("No launch path: settings in %s", settings_dir);
		return;
	}

	snprintf(path, sizeof(path), "%s", argv0);
	for (p = path; *p; p++)
		if (*p == '\\')
			*p = '/';

	if ((strncmp(path, "mc0:", 4) == 0 || strncmp(path, "mc1:", 4) == 0))
	{
		rest = path + 4;
		snprintf(settings_dir, sizeof(settings_dir), "%.4s/%s", path, rest + (*rest == '/'));
	}
	else if (usb_device_len(path))
	{
		rest = path + usb_device_len(path) + 1;
		snprintf(settings_dir, sizeof(settings_dir), USB_PATH "%s", rest + (*rest == '/'));
	}
	else
	{
		LOG("Launched from %s: settings in %s", argv0, settings_dir);
		return;
	}

	// keep the folder, drop the ELF name
	p = strrchr(settings_dir, '/');
	p[1] = 0;

	LOG("Launched from %s: settings in %s", argv0, settings_dir);
}

// At start-up, when enabled: keeps adding to the log file, so the log of a
// session that went wrong isn't lost by starting Apollo again
void log_startup(void)
{
	if (apollo_config.dbglog && !log_to_file(0))
		LOG("Debug log: can't open " APOLLO_LOG_FILE);

	LOG("Settings folder: %s", settings_dir);
}

// The Memory Card save icon only goes in Apollo's own folder, not a shared one (APPS/, BOOT/...)
static int settings_need_icons(void)
{
	size_t len = strlen(settings_dir);

	return (strncmp(settings_dir, "mc", 2) == 0 && len >= 13 &&
		strcmp(settings_dir + len - 13, "APOLLO-99PS2/") == 0);
}

int save_app_settings(app_config_t* config)
{
	char filePath[256];

	snprintf(filePath, sizeof(filePath), "%s%s", settings_dir, SETTINGS_FILE);
	if (mkdirs(filePath) != SUCCESS)
	{
		LOG("Error creating settings folder %s", settings_dir);
		return 0;
	}

	LOG("Saving Settings to %s...", filePath);
	snprintf(filePath, sizeof(filePath), "%s%s", settings_dir, "icon.sys");
	if (settings_need_icons() && file_exists(filePath) != SUCCESS)
	{
		uLong destLen = size_icon_sys;
		Bytef *dest = malloc(destLen);

		uncompress(dest, &destLen, zicon_sys, sizeof(zicon_sys));
		write_buffer(filePath, dest, destLen);
		free(dest);

		snprintf(filePath, sizeof(filePath), "%s%s", settings_dir, "icon.ico");
		destLen = size_icon_ico;
		dest = malloc(destLen);
		uncompress(dest, &destLen, zicon_ico, sizeof(zicon_ico));
		write_buffer(filePath, dest, destLen);
		free(dest);
	}

	snprintf(filePath, sizeof(filePath), "%s%s", settings_dir, SETTINGS_FILE);
	if (write_buffer(filePath, (uint8_t*) config, sizeof(app_config_t)) < 0)
	{
		LOG("Error saving settings!");
		return 0;
	}

	return 1;
}

// Reads SETTINGS.BIN from a folder into config; 0 if it's missing or not the right size
static int read_settings_file(const char* dir, app_config_t* config)
{
	char filePath[256];
	app_config_t* file_data;
	size_t file_size;

	snprintf(filePath, sizeof(filePath), "%s%s", dir, SETTINGS_FILE);
	if (read_buffer(filePath, (uint8_t**) &file_data, &file_size) != SUCCESS)
		return 0;

	if (file_size != sizeof(app_config_t))
	{
		LOG("Ignoring %s: wrong size (%d bytes)", filePath, (int) file_size);
		free(file_data);
		return 0;
	}

	file_data->user_id = config->user_id;
	memcpy(config, file_data, file_size);
	free(file_data);

	LOG("Settings loaded from %s: UserID (%08x)", filePath, config->user_id);
	return 1;
}

int load_app_settings(app_config_t* config)
{
	config->user_id = 0;

	LOG("Loading Settings...");
	if (read_settings_file(settings_dir, config))
		return 1;

	// First run from a new place: start from the default folder's settings, if any
	if (strcmp(settings_dir, SETTINGS_DIR_DEFAULT) != 0 && read_settings_file(SETTINGS_DIR_DEFAULT, config))
	{
		save_app_settings(config);
		return 1;
	}

	LOG("Settings not found, using defaults");
	save_app_settings(config);
	return 0;
}
