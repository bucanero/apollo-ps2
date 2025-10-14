#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <libmc.h>

#include "saves.h"
#include "common.h"
#include "settings.h"
#include "utils.h"
#include "mcio.h"
#include "ps1card.h"

#define UTF8_CHAR_STAR		"\xE2\x98\x85"

#define CHAR_ICON_NET		"\x09"
#define CHAR_ICON_ZIP		"\x0C"
#define CHAR_ICON_VMC		"\x04"
#define CHAR_ICON_COPY		"\x0B"
#define CHAR_ICON_SIGN		"\x06"
#define CHAR_ICON_USER		"\x07"
#define CHAR_ICON_LOCK		"\x08"
#define CHAR_ICON_WARN		"\x0F"
#define CHAR_ICON_PACK		"\x0E"


/*
 * Function:		endsWith()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Checks to see if a ends with b
 * Arguments:
 *	a:				String
 *	b:				Potential end
 * Return:			pointer if true, NULL if false
 */
static char* endsWith(const char * a, const char * b)
{
	int al = strlen(a), bl = strlen(b);
    
	if (al < bl)
		return NULL;

	a += (al - bl);
	while (*a)
		if (toupper(*a++) != toupper(*b++)) return NULL;

	return (char*) (a - bl);
}

/*
 * Function:		readFile()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		reads the contents of a file into a new buffer
 * Arguments:
 *	path:			Path to file
 * Return:			Pointer to the newly allocated buffer
 */
char * readTextFile(const char * path)
{
	FILE *f = fopen(path, "rb");

	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize <= 0)
	{
		fclose(f);
		return NULL;
	}

	char * string = malloc(fsize + 1);
	fread(string, fsize, 1, f);
	fclose(f);

	string[fsize] = 0;

	return string;
}

static code_entry_t* _createCmdCode(uint8_t type, const char* name, char code)
{
	code_entry_t* entry = (code_entry_t *)calloc(1, sizeof(code_entry_t));
	entry->type = type;
	entry->name = strdup(name);
	asprintf(&entry->codes, "%c", code);

	return entry;
}

static option_entry_t* _initOptions(int count)
{
	option_entry_t* options = (option_entry_t*)calloc(count, sizeof(option_entry_t));

	for(int i = 0; i < count; i++)
	{
		options[i].sel = -1;
		options[i].opts = list_alloc();
	}

	return options;
}

static option_entry_t* _createMcOptions(int count, const char* name, char value)
{
	option_value_t* optval;
	option_entry_t* options = _initOptions(count);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s (%s)", name, MC0_PATH);
	asprintf(&optval->value, "%c%c", value, STORAGE_MC0);
	list_append(options[0].opts, optval);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s (%s)", name, MC1_PATH);
	asprintf(&optval->value, "%c%c", value, STORAGE_MC1);
	list_append(options[0].opts, optval);

	return options;
}

static option_entry_t* _createExtOptions(int count, const char* name, char value)
{
	option_value_t* optval;
	option_entry_t* options = _initOptions(count);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s (%s)", name, USB_PATH);
	asprintf(&optval->value, "%c%c", value, STORAGE_MASS);
	list_append(options[0].opts, optval);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s (%s)", name, HOST_PATH);
	asprintf(&optval->value, "%c%c", value, STORAGE_HOST);
	list_append(options[0].opts, optval);

	return options;
}

static save_entry_t* _createSaveEntry(uint16_t flag, const char* name)
{
	save_entry_t* entry = (save_entry_t *)calloc(1, sizeof(save_entry_t));
	entry->flags = flag;
	entry->name = strdup(name);

	return entry;
}

static void _walk_dir_list(const char* startdir, const char* inputdir, const char* mask, list_t* list)
{
	char fullname[256];	
	struct dirent *dirp;
	int len = strlen(startdir);
	DIR *dp = opendir(inputdir);

	if (!dp) {
		LOG("Failed to open input directory: '%s'", inputdir);
		return;
	}

	while ((dirp = readdir(dp)) != NULL)
	{
		if ((strcmp(dirp->d_name, ".")  == 0) || (strcmp(dirp->d_name, "..") == 0) ||
			(strcmp(dirp->d_name, "ICON0.PNG") == 0) || (strcmp(dirp->d_name, "PARAM.SFO") == 0) || (strcmp(dirp->d_name,"PIC1.PNG") == 0) ||
			(strcmp(dirp->d_name, "icon.sys") == 0) || (strcmp(dirp->d_name, "SND0.AT3") == 0))
			continue;

		snprintf(fullname, sizeof(fullname), "%s%s", inputdir, dirp->d_name);

		if (wildcard_match_icase(dirp->d_name, mask))
		{
			//LOG("Adding file '%s'", fullname+len);
			list_append(list, strdup(fullname+len));
		}
	}
	closedir(dp);
}

static option_entry_t* _getFileOptions(const char* save_path, const char* mask, uint8_t is_cmd)
{
	char *filename;
	list_t* file_list;
	list_node_t* node;
	option_value_t* optval;
	option_entry_t* opt;

	LOG("Loading filenames {%s} from '%s'...", mask, save_path);

	file_list = list_alloc();
	_walk_dir_list(save_path, save_path, mask, file_list);

	if (!list_count(file_list))
	{
		is_cmd = 0;
		asprintf(&filename, CHAR_ICON_WARN " --- %s%s --- " CHAR_ICON_WARN, save_path, mask);
		list_append(file_list, filename);
	}

	opt = _initOptions(1);

	for (node = list_head(file_list); (filename = list_get(node)); node = list_next(node))
	{
		LOG("Adding '%s' (%s)", filename, mask);

		optval = malloc(sizeof(option_value_t));
		optval->name = filename;

		if (is_cmd)
			asprintf(&optval->value, "%c", is_cmd);
		else
			asprintf(&optval->value, "%s", mask);

		list_append(opt[0].opts, optval);
	}

	list_free(file_list);

	return opt;
}

static void _addBackupCommands(save_entry_t* item)
{
	code_entry_t* cmd;
	const char* filter = (item->flags & SAVE_FLAG_PS1) ? item->dir_name : "*";

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " File Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	if (item->flags & SAVE_FLAG_PS2)
	{
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy save files", CMD_CODE_NULL);
		cmd->options_count = 1;
		cmd->options = (item->flags & SAVE_FLAG_MEMCARD) ?
			_createExtOptions(1, "Copy Save to Mass Storage", CMD_COPY_SAVE_USB) :
			_createMcOptions(1, "Copy Save to Memory Card", CMD_COPY_SAVE_HDD);
		list_append(item->codes, cmd);

		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export save files to Zip", CMD_CODE_NULL);
		cmd->options_count = 1;
		cmd->options = _createExtOptions(1, "Export Zip to Mass Storage", CMD_EXPORT_ZIP_USB);
		list_append(item->codes, cmd);
	}

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export single save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, filter, CMD_EXPORT_DATA_FILE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Import single save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, filter, CMD_IMPORT_DATA_FILE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Hex Edit save game files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, filter, CMD_HEX_EDIT_FILE);
	list_append(item->codes, cmd);
}

static option_entry_t* _getSaveTitleIDs(const char* title_id)
{
	option_value_t* optval;
	option_entry_t* opt;
	char tmp[16];
	const char *ptr;
	const char *tid = NULL;//get_game_title_ids(title_id);

	if (!tid)
		tid = title_id;

	LOG("Adding TitleIDs=%s", tid);
	opt = _initOptions(1);

	ptr = tid;
	while (*ptr++)
	{
		if ((*ptr == '/') || (*ptr == 0))
		{
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, tid, ptr - tid);
			optval = malloc(sizeof(option_value_t));
			asprintf(&optval->name, "%s", tmp);
			asprintf(&optval->value, "%c", SFO_CHANGE_TITLE_ID);
			list_append(opt[0].opts, optval);
			tid = ptr+1;
		}
	}

	return opt;
}

static void add_ps1_commands(save_entry_t* save)
{
	char path[256];
	code_entry_t* cmd;

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Save Game Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save to .MCS format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export .MCS to Storage", (save->flags & SAVE_FLAG_VMC) ? CMD_EXP_VMC1SAVE : CMD_EXP_PS1SAVE);
	cmd->options[0].id = PS1SAVE_MCS;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save to .PSX format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export .PSX to Storage", (save->flags & SAVE_FLAG_VMC) ? CMD_EXP_VMC1SAVE : CMD_EXP_PS1SAVE);
	cmd->options[0].id = PS1SAVE_AR;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save to .PSV format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export .PSV to Storage", (save->flags & SAVE_FLAG_VMC) ? CMD_EXP_VMC1SAVE : CMD_EXP_PS1SAVE);
	cmd->options[0].id = PS1SAVE_PSV;
	list_append(save->codes, cmd);

	return;
}

static void add_ps2_commands(save_entry_t* item)
{
	code_entry_t* cmd;

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Save Game Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save to .PSU format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export .PSU to Storage", (item->flags & SAVE_FLAG_VMC) ? CMD_EXP_VMC2SAVE : CMD_EXP_PS2SAVE);
	cmd->options[0].id = FILE_TYPE_PSU;
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save to .PSV format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export .PSV to Storage", (item->flags & SAVE_FLAG_VMC) ? CMD_EXP_VMC2SAVE : CMD_EXP_PS2SAVE);
	cmd->options[0].id = FILE_TYPE_PSV;
	list_append(item->codes, cmd);

	if (item->flags & SAVE_FLAG_VMC)
		return;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save to .CBS format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export .CBS to Storage", CMD_EXP_PS2SAVE);
	cmd->options[0].id = FILE_TYPE_CBS;
	list_append(item->codes, cmd);

	return;
}

static option_entry_t* get_file_entries(const char* path, const char* mask)
{
	return _getFileOptions(path, mask, CMD_CODE_NULL);
}

/*
 * Function:		ReadLocalCodes()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads an entire NCL file into an array of code_entry
 * Arguments:
 *	path:			Path to ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadCodes(save_entry_t * save)
{
	code_entry_t * code;
	char filePath[256];
	char * buffer = NULL;

	save->codes = list_alloc();

	if (wildcard_match_icase(save->dir_name, "B?EXEC-*E?"))
	{
		code = _createCmdCode(PATCH_NULL, "----- " CHAR_ICON_WARN "Save linked to MemCard ID" CHAR_ICON_WARN " -----", CMD_CODE_NULL);
		list_append(save->codes, code);
	}

	code = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(save->codes, code);

	code = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(save->codes, code);

	if (save->flags & SAVE_FLAG_PS1)
		add_ps1_commands(save);
	else
		add_ps2_commands(save);

	_addBackupCommands(save);

	snprintf(filePath, sizeof(filePath), APOLLO_DATA_PATH "%s.savepatch", save->title_id);
	if ((buffer = readTextFile(filePath)) == NULL)
		goto skip_end;

	code = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Cheats " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);	
	list_append(save->codes, code);

	code = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Apply Changes", CMD_RESIGN_SAVE);
	list_append(save->codes, code);

	code = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Raw Patch File", CMD_VIEW_RAW_PATCH);
	list_append(save->codes, code);

	LOG("Loading BSD codes '%s'...", filePath);
	load_patch_code_list(buffer, save->codes, &get_file_entries, save->path);
	free (buffer);

skip_end:
	LOG("Loaded %ld codes", list_count(save->codes));

	return list_count(save->codes);
}

static void add_vmc1_import_saves(list_t* list, const char* path, const char* folder)
{
	code_entry_t * cmd;
	DIR *d;
	struct dirent *dir;
	char psvPath[256];

	snprintf(psvPath, sizeof(psvPath), "%s%s", path, folder);
	d = opendir(psvPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!endsWith(dir->d_name, ".PSV") && !endsWith(dir->d_name, ".MCS") && !endsWith(dir->d_name, ".PSX") &&
			!endsWith(dir->d_name, ".PS1") && !endsWith(dir->d_name, ".MCB") && !endsWith(dir->d_name, ".PDA"))
			continue;

		// check for PS1 PSV saves
		if (endsWith(dir->d_name, ".PSV"))
		{
			snprintf(psvPath, sizeof(psvPath), "%s%s%s", path, folder, dir->d_name);
			if (read_file(psvPath, (uint8_t*) psvPath, 0x40) < 0 || psvPath[0x3C] != 0x01)
				continue;
		}

		snprintf(psvPath, sizeof(psvPath), CHAR_ICON_COPY "%c %s", CHAR_TAG_PS1, dir->d_name);
		cmd = _createCmdCode(PATCH_COMMAND, psvPath, CMD_IMP_VMC1SAVE);
		asprintf(&cmd->file, "%s%s%s", path, folder, dir->d_name);
		cmd->codes[1] = FILE_TYPE_PS1;
		list_append(list, cmd);

		LOG("[%s] F(%X) name '%s'", cmd->file, cmd->flags, cmd->name+2);
	}

	closedir(d);
}

int ReadVmc1Codes(save_entry_t * save)
{
	code_entry_t * cmd;

	save->codes = list_alloc();

	if (save->type == FILE_TYPE_MENU)
	{
		add_vmc1_import_saves(save->codes, save->path, PS1_SAVES_PATH_USB);
		add_vmc1_import_saves(save->codes, save->path, PS3_SAVES_PATH_USB);
		if (!list_count(save->codes))
		{
			list_free(save->codes);
			save->codes = NULL;
			return 0;
		}

		list_bubbleSort(save->codes, &sortCodeList_Compare);

		return list_count(save->codes);
	}

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Save Game Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .MCS format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Copy .MCS Save to Storage", CMD_EXP_VMC1SAVE);
	cmd->options[0].id = PS1SAVE_MCS;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .PSV format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Copy .PSV Save to Storage", CMD_EXP_VMC1SAVE);
	cmd->options[0].id = PS1SAVE_PSV;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .PSX format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Copy .PSX Save to Storage", CMD_EXP_VMC1SAVE);
	cmd->options[0].id = PS1SAVE_AR;
	list_append(save->codes, cmd);

	LOG("Loaded %ld codes", list_count(save->codes));

	return list_count(save->codes);
}

static void add_vmc_import_saves(list_t* list, const char* path, const char* folder)
{
	code_entry_t * cmd;
	DIR *d;
	struct dirent *dir;
	char psvPath[256];
	char data[64];
	char title[256];
	int type, toff;

	snprintf(psvPath, sizeof(psvPath), "%s%s", path, folder);
	d = opendir(psvPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!S_ISREG(dir->d_stat.st_mode))
			continue;

		if (endsWith(dir->d_name, ".PSV"))
		{
			toff = 0x80;
			type = FILE_TYPE_PSV;
		}
		else if (endsWith(dir->d_name, ".PSU"))
		{
			toff = 0x40;
			type = FILE_TYPE_PSU;
		}
		else if (endsWith(dir->d_name, ".MAX"))
		{
			toff = 0x10;
			type = FILE_TYPE_MAX;
		}
		else if (endsWith(dir->d_name, ".CBS"))
		{
			toff = 0x14;
			type = FILE_TYPE_CBS;
		}
		else if (endsWith(dir->d_name, ".XPS") || endsWith(dir->d_name, ".SPS"))
		{
			toff = 0x15;
			type = FILE_TYPE_XPS;
		}
		else continue;

		snprintf(psvPath, sizeof(psvPath), "%s%s%s", path, folder, dir->d_name);
		LOG("Reading %s...", psvPath);

		if (type == FILE_TYPE_PSV && (read_file(psvPath, (uint8_t*) data, 0x40) < 0 || data[0x3C] != 0x02))
			continue;

		FILE *fp = fopen(psvPath, "rb");
		if (!fp) {
			LOG("Unable to open '%s'", psvPath);
			continue;
		}

		fseek(fp, toff, SEEK_SET);
		if (type == FILE_TYPE_XPS)
		{
			// Skip the variable size header
			fread(&toff, 1, sizeof(int), fp);
			fseek(fp, toff, SEEK_CUR);
			fread(&toff, 1, sizeof(int), fp);
			fseek(fp, toff + 10, SEEK_CUR);
		}
		fread(data, 1, sizeof(data), fp);
		fclose(fp);

		snprintf(title, sizeof(title), "%s (%.10s) %s", CHAR_ICON_COPY, data+2, dir->d_name);
		cmd = _createCmdCode(PATCH_COMMAND, title, CMD_IMP_VMC2SAVE);
		cmd->file = strdup(psvPath);
		cmd->codes[1] = type;
		list_append(list, cmd);

		LOG("[%s] F(%X) name '%s'", cmd->file, cmd->flags, cmd->name+2);
	}

	closedir(d);
}

int ReadVmc2Codes(save_entry_t * save)
{
	code_entry_t * cmd;
	char filePath[256];
	char * buffer = NULL;

	save->codes = list_alloc();

	if (save->type == FILE_TYPE_MENU)
	{
		add_vmc_import_saves(save->codes, save->path, PS2_SAVES_PATH_USB);
		add_vmc_import_saves(save->codes, save->path, PS3_SAVES_PATH_USB);

		if (!list_count(save->codes))
		{
			list_free(save->codes);
			save->codes = NULL;
			return 0;
		}
		list_bubbleSort(save->codes, &sortCodeList_Compare);

		return list_count(save->codes);
	}

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Save Transfer " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy save game to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Copy Save to Memory Card", CMD_COPY_SAVE_VMC);

//	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy save game to Mass Storage", CMD_CODE_NULL);
//	cmd->options_count = 1;
//	cmd->options = _createExtOptions(1, "Copy Save to Mass Storage", CMD_COPY_SAVE_USB);

	list_append(save->codes, cmd);

	if (save->flags & SAVE_FLAG_PS1)
		add_ps1_commands(save);
	else
		add_ps2_commands(save);

	LOG("Loaded %ld codes", list_count(save->codes));

	return list_count(save->codes);
}

/*
 * Function:		ReadOnlineSaves()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads an entire NCL file into an array of code_entry
 * Arguments:
 *	filename:		File name ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadOnlineSaves(save_entry_t * game)
{
	code_entry_t* item;
	option_value_t* optval;
	char path[256];
	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%s.txt", game->title_id);

	if (file_exists(path) == SUCCESS && strcmp(apollo_config.save_db, ONLINE_URL) == 0)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(game->path, "saves.txt", path, 0);
	}
	else
	{
		if (!http_download(game->path, "saves.txt", path, 0))
			return 0;
	}

	char *data = readTextFile(path);
	if (!data)
		return 0;

	game->codes = list_alloc();

	for (char *ptr, *line = strtok(data, "\r\n"); line; line = strtok(NULL, "\r\n"))
	{
		// skip invalid lines
		if ((ptr = strchr(line, '=')) == NULL)
			continue;

		*ptr++ = 0;
		snprintf(path, sizeof(path), CHAR_ICON_ZIP " %s", ptr);
		item = _createCmdCode(PATCH_COMMAND, path, CMD_CODE_NULL);
		item->file = strdup(line);

		item->options_count = 1;
		item->options = _createMcOptions(1, "Download to Mass Storage", CMD_DOWNLOAD_USB);
		optval = list_get_item(item->options[0].opts, 0);
		memcpy(optval->name + 26, "mass:", 5);
		optval->value[1] = STORAGE_MASS;

		optval = list_get_item(item->options[0].opts, 1);
		memcpy(optval->name + 26, "host:", 5);
		optval->value[1] = STORAGE_HOST;
		list_append(game->codes, item);

		LOG("[%s%s] %s", game->path, item->file, item->name + 1);
	}

	free(data);
	LOG("Loaded %d saves", list_count(game->codes));

	return (list_count(game->codes));
}

int ReadOfflineSaves(save_entry_t * game)
{
	code_entry_t* item;
	option_value_t* optval;
	char path[256];

	snprintf(path, sizeof(path), "%ssaves.txt", game->path);

	char *data = readTextFile(path);
	if (!data)
		return 0;
	
	game->codes = list_alloc();

	for (char *ptr, *line = strtok(data, "\r\n"); line; line = strtok(NULL, "\r\n"))
	{
		if ((ptr = strchr(line, '=')) == NULL)
			continue;

		*ptr++ = 0;
		snprintf(path, sizeof(path), CHAR_ICON_ZIP " %s", ptr);
		item = _createCmdCode(PATCH_COMMAND, path, CMD_CODE_NULL);
		item->file = strdup(line);

		item->options_count = 1;
		item->options = _createMcOptions(1, "Import to Memory Card", CMD_DOWNLOAD_MC);

		optval = malloc(sizeof(option_value_t));
		asprintf(&optval->name, "Copy to Storage (%s)", USB_PATH);
		asprintf(&optval->value, "%c%c", CMD_DOWNLOAD_MC, STORAGE_MASS);
		list_append(item->options[0].opts, optval);

		optval = malloc(sizeof(option_value_t));
		asprintf(&optval->name, "Copy to Storage (%s)", HOST_PATH);
		asprintf(&optval->value, "%c%c", CMD_DOWNLOAD_MC, STORAGE_HOST);
		list_append(item->options[0].opts, optval);
		list_append(game->codes, item);

		LOG("[%s%s] %s", game->path, item->file, item->name + 1);
	}

	free(data);
	LOG("Loaded %d saves", list_count(game->codes));

	return (list_count(game->codes));
}

list_t * ReadBackupList(const char* userPath)
{
	save_entry_t * item;
	code_entry_t * cmd;
	list_t *list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_ZIP, CHAR_ICON_ZIP " Extract Archives (Zip)");
	item->path = strdup(menu_options[3].options[apollo_config.storage]);
	item->type = FILE_TYPE_ZIP;
	list_append(list, item);
/*
	item = _createSaveEntry(0, CHAR_ICON_NET " Network Tools");
	item->path = strdup(MC0_PATH);
	item->type = FILE_TYPE_NET;
	list_append(list, item);
*/
	return list;
}

int ReadBackupCodes(save_entry_t * bup)
{
	code_entry_t * cmd;
	char tmp[256];

	switch(bup->type)
	{
	case FILE_TYPE_ZIP:
		break;

	case FILE_TYPE_NET:
		bup->codes = list_alloc();
//		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " URL link Downloader (http, https, ftp, ftps)", CMD_URL_DOWNLOAD);
//		list_append(bup->codes, cmd);
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Local Web Server (full system access)", CMD_NET_WEBSERVER);
		list_append(bup->codes, cmd);
		return list_count(bup->codes);
/*
	case FILE_TYPE_PRX:
		bup->codes = list_alloc();

		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " Install Save-game Key Dumper plugin", CMD_SETUP_PLUGIN);
		cmd->codes[1] = 1;
		list_append(bup->codes, cmd);
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Disable Save-game Key Dumper plugin", CMD_SETUP_PLUGIN);
		cmd->codes[1] = 0;
		list_append(bup->codes, cmd);

		return list_count(bup->codes);
*/
	default:
		return 0;
	}

	bup->codes = list_alloc();

	LOG("Loading files from '%s'...", bup->path);

	DIR *d;
	struct dirent *dir;
	d = opendir(bup->path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (!endsWith(dir->d_name, ".RAR") && !endsWith(dir->d_name, ".ZIP") && !endsWith(dir->d_name, ".7Z"))
				continue;

			snprintf(tmp, sizeof(tmp), CHAR_ICON_ZIP " Extract %s%s", bup->path, dir->d_name);
			cmd = _createCmdCode(PATCH_COMMAND, tmp, CMD_EXTRACT_ARCHIVE);
			asprintf(&cmd->file, "%s%s", bup->path, dir->d_name);

			LOG("[%s] name '%s'", cmd->file, cmd->name +2);
			list_append(bup->codes, cmd);
		}
		closedir(d);
	}

	if (!list_count(bup->codes))
	{
		list_free(bup->codes);
		bup->codes = NULL;
		return 0;
	}

	LOG("%ld items loaded", list_count(bup->codes));

	return list_count(bup->codes);
}

/*
 * Function:		UnloadGameList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Free entire array of game_entry
 * Arguments:
 *	list:			Array of game_entry to free
 *	count:			number of game entries
 * Return:			void
 */
void UnloadGameList(list_t * list)
{
	list_node_t *node, *nc, *no;
	save_entry_t *item;
	code_entry_t *code;
	option_value_t* optval;

	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		if (item->name)
		{
			free(item->name);
			item->name = NULL;
		}

		if (item->path)
		{
			free(item->path);
			item->path = NULL;
		}

		if (item->dir_name)
		{
			free(item->dir_name);
			item->dir_name = NULL;
		}

		if (item->title_id)
		{
			free(item->title_id);
			item->title_id = NULL;
		}

		if (item->icon)
		{
			free(item->icon);
			item->icon = NULL;
		}

		if (item->codes)
		{
			for (nc = list_head(item->codes); (code = list_get(nc)); nc = list_next(nc))
			{
				if (code->codes)
				{
					free (code->codes);
					code->codes = NULL;
				}
				if (code->name)
				{
					free (code->name);
					code->name = NULL;
				}
				if (code->options && code->options_count > 0)
				{
					for (int z = 0; z < code->options_count; z++)
					{
						for (no = list_head(code->options[z].opts); (optval = list_get(no)); no = list_next(no))
						{
							if (optval->name)
								free(optval->name);
							if (optval->value)
								free(optval->value);

							free(optval);
						}
						list_free(code->options[z].opts);

						if (code->options[z].line)
							free(code->options[z].line);
					}
					
					free (code->options);
				}

				free(code);
			}
			
			list_free(item->codes);
			item->codes = NULL;
		}

		free(item);
	}

	list_free(list);
	
	LOG("UnloadGameList() :: Successfully unloaded game list");
}

int sortCodeList_Compare(const void* a, const void* b)
{
	return strcasecmp(((code_entry_t*) a)->name, ((code_entry_t*) b)->name);
}

/*
 * Function:		qsortSaveList_Compare()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Compares two game_entry for QuickSort
 * Arguments:
 *	a:				First code
 *	b:				Second code
 * Return:			1 if greater, -1 if less, or 0 if equal
 */
int sortSaveList_Compare(const void* a, const void* b)
{
	return strcasecmp(((save_entry_t*) a)->name, ((save_entry_t*) b)->name);
}

static int parseTypeFlags(int flags)
{
	if (flags & SAVE_FLAG_PACKED)
		return ((flags & SAVE_FLAG_PS1) ? FILE_TYPE_PSX : FILE_TYPE_PSU);
	else if (flags & SAVE_FLAG_VMC)
		return ((flags & SAVE_FLAG_PS1) ? FILE_TYPE_VMC : FILE_TYPE_VMC+1);
	else if (flags & SAVE_FLAG_PS1)
		return FILE_TYPE_PS1;
	else if (flags & SAVE_FLAG_PS2)
		return FILE_TYPE_PS2;

	return 0;
}

int sortSaveList_Compare_Type(const void* a, const void* b)
{
	int ta = parseTypeFlags(((save_entry_t*) a)->flags);
	int tb = parseTypeFlags(((save_entry_t*) b)->flags);

	if (ta == tb)
		return sortSaveList_Compare(a, b);
	else if (ta < tb)
		return -1;
	else
		return 1;
}

int sortSaveList_Compare_TitleID(const void* a, const void* b)
{
	char* ta = ((save_entry_t*) a)->title_id;
	char* tb = ((save_entry_t*) b)->title_id;

	if (!ta)
		return (-1);

	if (!tb)
		return (1);

	int ret = strcasecmp(ta, tb);

	return (ret ? ret : sortSaveList_Compare(a, b));
}

static void check_ps1_savegame(const char* path, const char* d_name, list_t *list, int flags)
{
	uint64_t size = 0;
	save_entry_t *item;
	char savePath[256];
	char buf[128];

	snprintf(savePath, sizeof(savePath), "%s%s", path, d_name);
	LOG("Checking PS1 save '%s'...", savePath);

	get_file_size(savePath, &size);
	if (!size || size % 0x2000 != 0 || size > 0x20000 ||
		(read_file(savePath, buf, sizeof(buf)) < 0) || buf[0] != 'S' || buf[1] != 'C')
		return;

	char* tmp = sjis2utf8(buf+4);
	item = _createSaveEntry(SAVE_FLAG_PS1 | flags, tmp);
	item->path = strdup(path);
	item->type = FILE_TYPE_PS1;
	item->dir_name = strdup(d_name);
	asprintf(&item->icon, "%c", 0);
	asprintf(&item->title_id, "%.10s", d_name+((strlen(d_name) < 12) ? 0 : 2));
	free(tmp);

	if(strlen(item->title_id) == 10 && item->title_id[4] == '-')
		memmove(&item->title_id[4], &item->title_id[5], 6);

	LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
	list_append(list, item);
}

static void read_ps2_savegames(const char* userPath, list_t *list, int flags)
{
	DIR *d;
	mcIcon iconsys;
	struct dirent *dir;
	save_entry_t *item;
	char sysPath[256];

	d = opendir(userPath);
	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		else if (flags & SAVE_FLAG_PS1CARD)
		{
			check_ps1_savegame(userPath, dir->d_name, list, flags);
			continue;
		}
		else if (!S_ISDIR(dir->d_stat.st_mode))
			continue;

		snprintf(sysPath, sizeof(sysPath), "%s%s/icon.sys", userPath, dir->d_name);

		LOG("Reading %s...", sysPath);
		if (read_file(sysPath, (uint8_t*) &iconsys, sizeof(mcIcon)) < 0) {
			LOG("Unable to read from '%s'", sysPath);

			// check PS1 save on PS2 memcard
			snprintf(sysPath, sizeof(sysPath), "%s%s/", userPath, dir->d_name);
			check_ps1_savegame(sysPath, dir->d_name, list, flags);
			continue;
		}

		memcpy(sysPath, iconsys.title, sizeof(iconsys.title));
		if (iconsys.nlOffset)
		{
			memmove(&sysPath[iconsys.nlOffset+2], &sysPath[iconsys.nlOffset], sizeof(iconsys.title) - iconsys.nlOffset);
			memcpy(&sysPath[iconsys.nlOffset], "\x81\x50", 2);
		}

		char* tmp = sjis2utf8(sysPath);
		item = _createSaveEntry(SAVE_FLAG_PS2 | flags, tmp);
		item->type = FILE_TYPE_PS2;
		item->icon = strdup(iconsys.view);
		item->dir_name = strdup(dir->d_name);
		asprintf(&item->title_id, "%.10s", dir->d_name+((strlen(dir->d_name) < 12) ? 0 : 2));
		asprintf(&item->path, "%s%s/", userPath, dir->d_name);
		free(tmp);

		if(strlen(item->title_id) == 10 && item->title_id[4] == '-')
			memmove(&item->title_id[4], &item->title_id[5], 6);

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

static void read_usb_savegames(const char* userPath, list_t *list)
{
	char path[256];

	snprintf(path, sizeof(path), "%s%s", userPath, PS2_SAVES_PATH_USB);
	read_ps2_savegames(path, list, 0);
}

static int set_psx_import_codes(save_entry_t* item)
{
	code_entry_t* cmd;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Import to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Import to MemCard", CMD_IMP_SAVE_MC);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(item->codes, cmd);

	return list_count(item->codes);
}

static void read_psx_savegames(const char* userPath, const char* folder, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char psvPath[256];
	char data[64];
	int type, toff;

	snprintf(psvPath, sizeof(psvPath), "%s%s", userPath, folder);
	d = opendir(psvPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!S_ISREG(dir->d_stat.st_mode))
			continue;

		if (endsWith(dir->d_name, ".PSX"))
		{
			toff = 0;
			type = FILE_TYPE_PSX;
		}
		else if (endsWith(dir->d_name, ".MCS"))
		{
			toff = 0x0A;
			type = FILE_TYPE_MCS;
		}
		else if (endsWith(dir->d_name, ".MAX"))
		{
			toff = 0x10;
			type = FILE_TYPE_MAX;
		}
		else if (endsWith(dir->d_name, ".CBS"))
		{
			toff = 0x14;
			type = FILE_TYPE_CBS;
		}
		else if (endsWith(dir->d_name, ".PSU"))
		{
			toff = 0x40;
			type = FILE_TYPE_PSU;
		}
		else if (endsWith(dir->d_name, ".PSV"))
		{
			snprintf(psvPath, sizeof(psvPath), "%s%s%s", userPath, folder, dir->d_name);
			if (read_file(psvPath, (uint8_t*) data, sizeof(data)) < 0)
				continue;

			toff = (data[0x3C] == 0x01) ? 0x64 : 0x80;
			type = FILE_TYPE_PSV;
		}
		else if (endsWith(dir->d_name, ".XPS") || endsWith(dir->d_name, ".SPS"))
		{
			toff = 0x15;
			type = FILE_TYPE_XPS;
		}
		else
			continue;

		snprintf(psvPath, sizeof(psvPath), "%s%s%s", userPath, folder, dir->d_name);
		LOG("Reading %s...", psvPath);

		FILE *fp = fopen(psvPath, "rb");
		if (!fp) {
			LOG("Unable to open '%s'", psvPath);
			continue;
		}

		fseek(fp, toff, SEEK_SET);
		if (type == FILE_TYPE_XPS)
		{
			// Skip the variable size header
			fread(&toff, 1, sizeof(int), fp);
			fseek(fp, toff, SEEK_CUR);
			fread(&toff, 1, sizeof(int), fp);
			fseek(fp, toff + 10, SEEK_CUR);
			toff = 0;
		}
		fread(data, 1, sizeof(data), fp);
		fclose(fp);

		item = _createSaveEntry(SAVE_FLAG_PACKED, dir->d_name);
		set_psx_import_codes(item);

		item->flags |= (type == FILE_TYPE_PSX || type == FILE_TYPE_MCS || toff == 0x64) ? SAVE_FLAG_PS1 : SAVE_FLAG_PS2;
		item->type = type;
		item->path = strdup(psvPath);
		item->dir_name = strdup(data);
		asprintf(&item->title_id, "%.10s", data+2);

		if(strlen(item->title_id) == 10 && item->title_id[4] == '-')
			memmove(&item->title_id[4], &item->title_id[5], 6);

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

static void read_vmc_files(const char* userPath, const char* folder, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char psvPath[256];
	uint64_t size;
	uint16_t flag;

	snprintf(psvPath, sizeof(psvPath), "%s%s", userPath, folder);
	d = opendir(psvPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (S_ISDIR(dir->d_stat.st_mode))
		{
			snprintf(psvPath, sizeof(psvPath), "%s%s/", folder, dir->d_name);
			read_vmc_files(userPath, psvPath, list);
			continue;
		}

		if (!S_ISREG(dir->d_stat.st_mode) || !(endsWith(dir->d_name, ".VMC") || endsWith(dir->d_name, ".VM2") ||
			endsWith(dir->d_name, ".BIN") || endsWith(dir->d_name, ".PS2") || endsWith(dir->d_name, ".CARD") ||
			// PS1 VMCs
			endsWith(dir->d_name, ".MCD") || endsWith(dir->d_name, ".MCR") || endsWith(dir->d_name, ".GME") ||
			endsWith(dir->d_name, ".VM1") || endsWith(dir->d_name, ".VMP") || endsWith(dir->d_name, ".VGS") ||
			endsWith(dir->d_name, ".SRM")))
			continue;

		snprintf(psvPath, sizeof(psvPath), "%s%s%s", userPath, folder, dir->d_name);
		get_file_size(psvPath, &size);

		LOG("Checking %s...", psvPath);
		switch (size)
		{
		case PS1CARD_SIZE:
		case 0x20040:
		case 0x20080:
		case 0x200A0:
		case 0x20F40:
			flag = SAVE_FLAG_PS1;
			break;

		case 0x800000:
		case 0x840000:
		case 0x1000000:
		case 0x1080000:
		case 0x2000000:
		case 0x2100000:
		case 0x4000000:
		case 0x4200000:
			flag = SAVE_FLAG_PS2;
			break;

		default:
			continue;
		}

		item = _createSaveEntry(flag | SAVE_FLAG_VMC, dir->d_name);
		item->type = FILE_TYPE_VMC;
		item->path = strdup(psvPath);
		item->title_id = strdup("VMC");
		asprintf(&item->dir_name, "%s%s", userPath, folder);

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

/*
 * Function:		ReadUserList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads the entire userlist folder into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
list_t * ReadUsbList(const char* userPath)
{
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;

	if (dir_exists(userPath) != SUCCESS)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS2, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();
	item->path = strdup(userPath);
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Copy Saves to MemCard", CMD_COPY_SAVES_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Copy Saves to MemCard", CMD_COPY_ALL_SAVES_HDD);
	list_append(item->codes, cmd);

//	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Start local Web Server", CMD_SAVE_WEBSERVER);
//	list_append(item->codes, cmd);
	list_append(list, item);

	read_usb_savegames(userPath, list);
	read_vmc_files(userPath, IMP_OPLVMC_PATH_USB, list);
	read_vmc_files(userPath, IMP_PS2VMC_PATH_USB, list);
	read_vmc_files(userPath, IMP_PS1VMC_PATH_USB, list);
	read_vmc_files(userPath, IMP_POPS_PATH_USB, list);
	read_psx_savegames(userPath, PS1_SAVES_PATH_USB, list);
	read_psx_savegames(userPath, PS2_SAVES_PATH_USB, list);
	read_psx_savegames(userPath, PS3_SAVES_PATH_USB, list);

	return list;
}

list_t * ReadUserList(const char* userPath)
{
	int mctype;
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;

	if (dir_exists(userPath) != SUCCESS)
		return NULL;

	list = list_alloc();
	mctype = check_memcard_type(userPath);

	item = _createSaveEntry((mctype == sceMcTypePS1) ? (SAVE_FLAG_PS1|SAVE_FLAG_PS1CARD) : SAVE_FLAG_PS2, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();
	item->path = strdup(userPath);
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Copy Saves to Backup Storage", CMD_COPY_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Copy Saves to Backup Storage", CMD_COPY_ALL_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " .PSU Export selected Saves to Backup Storage", CMD_CODE_NULL);
	if (mctype == sceMcTypePS1)
		memcpy(cmd->name +3, "MCS", 3);

	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Copy Saves to Backup Storage", CMD_EXPORT_SAVES);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " .PSU Export All Saves to Backup Storage", CMD_CODE_NULL);
	if (mctype == sceMcTypePS1)
		memcpy(cmd->name +3, "MCS", 3);

	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Copy Saves to Backup Storage", CMD_EXPORT_ALL_SAVES);
	list_append(item->codes, cmd);

//	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Start local Web Server", CMD_SAVE_WEBSERVER);
//	list_append(item->codes, cmd);
	list_append(list, item);

	read_ps2_savegames(userPath, list, SAVE_FLAG_MEMCARD | (mctype == sceMcTypePS1 ? SAVE_FLAG_PS1CARD : 0));

	return list;
}

/*
 * Function:		ReadOnlineList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads the entire gamelist file into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
static void _ReadSaveDbList(const char* base, const char* filepath, uint16_t flags, list_t *list)
{
	save_entry_t *item;
	char *data = readTextFile(filepath);

	if (!data)
		return;

	for (char *ptr, *line = strtok(data, "\r\n"); line; line = strtok(NULL, "\r\n"))
	{
		if ((ptr = strchr(line, '=')) == NULL)
			continue;

		*ptr++ = 0;
		item = _createSaveEntry(flags, ptr);
		item->title_id = strdup(line);
		asprintf(&item->path, "%s%s/", base, item->title_id);

		LOG("+ [%s] %s", item->title_id, item->name);
		list_append(list, item);
	}

	free(data);
}

static void _ReadOnlineListEx(const char* urlPath, uint16_t flag, list_t *list)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%04X_games.txt", flag);

	if (file_exists(path) == SUCCESS && strcmp(apollo_config.save_db, ONLINE_URL) == 0)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(urlPath, "games.txt", path, 0);
	}
	else
	{
		if (!http_download(urlPath, "games.txt", path, 0))
			return;
	}

	_ReadSaveDbList(urlPath, path, flag | SAVE_FLAG_ONLINE, list);
}

list_t * ReadOnlineList(const char* urlPath)
{
	char url[256];
	list_t *list = list_alloc();

	// PSV save-games (Zip folder)
	snprintf(url, sizeof(url), "%s" "PS1/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS1, list);

	// PS2 save-games (Zip folder)
	snprintf(url, sizeof(url), "%sPS2/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS2, list);

	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	return list;
}

list_t * ReadOfflineList(const char* sysPath)
{
	char base[256];
	char path[256];
	list_t *list = list_alloc();

	// PSV save-games (Zip folder)
	snprintf(base, sizeof(base), "%sPS1/", sysPath);
	snprintf(path, sizeof(path), "%sgames.txt", base);
	_ReadSaveDbList(base, path, SAVE_FLAG_PS1 | SAVE_FLAG_OFFLINE, list);

	// PS2 save-games (Zip folder)
	snprintf(base, sizeof(base), "%sPS2/", sysPath);
	snprintf(path, sizeof(path), "%sgames.txt", base);
	_ReadSaveDbList(base, path, SAVE_FLAG_PS2 | SAVE_FLAG_OFFLINE, list);

	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	return list;
}

list_t * ReadVmc1List(const char* userPath)
{
	char filePath[256];
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	ps1mcData_t* mcdata;

	if (!openMemoryCard(userPath, 0))
	{
		LOG("Error: no PS1 Memory Card detected! (%s)", userPath);
		return NULL;
	}

	mcdata = getMemoryCardData();
	if (!mcdata)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS1, CHAR_ICON_VMC " Memory Card Management");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->title_id = strdup("VMC");
	item->codes = list_alloc();
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Copy Saves to MemCard", CMD_COPY_SAVES_VMC);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Copy Saves to MemCard", CMD_COPY_ALL_SAVES_VMC);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export selected Saves to Storage (.PSV)", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export Saves to Storage", CMD_EXP_SAVES_VMC);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export all Saves to Storage (.PSV)", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export Saves to Storage", CMD_EXP_ALL_SAVES_VMC);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Virtual Memory Card " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export Memory Card to .VM1 format", CMD_CODE_NULL);
	cmd->file = strdup(strrchr(userPath, '/')+1);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Save .VM1 Memory Card to Storage", CMD_EXP_PS1_VM1);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export Memory Card to .VMP format", CMD_CODE_NULL);
	cmd->file = strdup(strrchr(userPath, '/')+1);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Save .VMP Memory Card to Storage", CMD_EXP_PS1_VMP);
	list_append(item->codes, cmd);
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PS1, CHAR_ICON_COPY " Import Saves to Virtual Card");
	item->dir_name = strdup(userPath);
	item->path = strdup(userPath);
	strchr(item->path, '/')[1] = 0;
	asprintf(&item->title_id, " %s", item->path);
	item->type = FILE_TYPE_MENU;
	list_append(list, item);

	for (int i = 0; i < PS1CARD_MAX_SLOTS; i++)
	{
		if (mcdata[i].saveType != PS1BLOCK_INITIAL)
			continue;

		LOG("Reading '%s'...", mcdata[i].saveName);

		char* tmp = sjis2utf8(mcdata[i].saveTitle);
		item = _createSaveEntry(SAVE_FLAG_PS1 | SAVE_FLAG_VMC, tmp);
		item->type = FILE_TYPE_PS1;
		item->title_id = strdup(mcdata[i].saveProdCode);
		item->dir_name =  strdup(mcdata[i].saveName);
		//hack to keep the save block
		asprintf(&item->icon, "%c", i);
		asprintf(&item->path, "%s\n%s", userPath, mcdata[i].saveName);
		free(tmp);

		if(strlen(item->title_id) == 10 && item->title_id[4] == '-')
			memmove(&item->title_id[4], &item->title_id[5], 6);

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	return list;
}

list_t * ReadVmc2List(const char* userPath)
{
	char filePath[256];
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	mcIcon iconsys;
	int r, dd, fd;

	r = mcio_vmcInit(userPath);
	if (r < 0)
	{
		LOG("Error: no PS2 Memory Card detected! (%d)", r);
		return NULL;
	}

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS2, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->codes = list_alloc();
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Copy Saves to MemCard", CMD_COPY_SAVES_VMC);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to Memory Card", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createMcOptions(1, "Copy Saves to MemCard", CMD_COPY_ALL_SAVES_VMC);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export selected Saves to Storage (.PSV)", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export Saves to Storage", CMD_EXP_SAVES_VMC);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export all Saves to Storage (.PSV)", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createExtOptions(1, "Export Saves to Storage", CMD_EXP_ALL_SAVES_VMC);
	list_append(item->codes, cmd);
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PS2, CHAR_ICON_COPY " Import Saves to Virtual Card");
	item->path = strdup(userPath);
	strchr(item->path, '/')[1] = 0;
	asprintf(&item->title_id, " %s", item->path);
	item->type = FILE_TYPE_MENU;
	list_append(list, item);

	dd = mcio_mcDopen("/");
	if (dd >= 0)
	{
		struct io_dirent dirent;

		do {
			r = mcio_mcDread(dd, &dirent);
			if ((r) && (strcmp(dirent.name, ".")) && (strcmp(dirent.name, "..")))
			{
				snprintf(filePath, sizeof(filePath), "%s/icon.sys", dirent.name);
				LOG("Reading %s...", filePath);

				fd = mcio_mcOpen(filePath, sceMcFileAttrReadable | sceMcFileAttrFile);
				if (fd < 0) {
					LOG("Unable to read from '%s'", filePath);
					continue;
				}

				r = mcio_mcRead(fd, &iconsys, sizeof(mcIcon));
				mcio_mcClose(fd);

				if (r != sizeof(mcIcon))
					continue;

				memcpy(filePath, iconsys.title, sizeof(iconsys.title));
				if (iconsys.nlOffset)
				{
					memmove(&filePath[iconsys.nlOffset+2], &filePath[iconsys.nlOffset], sizeof(iconsys.title) - iconsys.nlOffset);
					memcpy(&filePath[iconsys.nlOffset], "\x81\x50", 2);
				}

				char* tmp = sjis2utf8(filePath);
				item = _createSaveEntry(SAVE_FLAG_PS2 | SAVE_FLAG_VMC, tmp);
				item->type = FILE_TYPE_PS2;
				item->icon = strdup(iconsys.view);
				item->dir_name = strdup(dirent.name);
				asprintf(&item->title_id, "%.10s", dirent.name+((strlen(dirent.name) < 12) ? 0 : 2));
				asprintf(&item->path, "%s\n%s/", userPath, dirent.name);
				free(tmp);

				if(strlen(item->title_id) == 10 && item->title_id[4] == '-')
					memmove(&item->title_id[4], &item->title_id[5], 6);

				LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
				list_append(list, item);
			}
		} while (r);

		mcio_mcDclose(dd);
	}

	return list;
}

int get_save_details(const save_entry_t* save, char **details)
{
	if (save->flags & SAVE_FLAG_PACKED)
	{
		asprintf(details, "%s\n\n----- Packed PS%d Save -----\n"
			"File: %s\n"
			"Title ID: %s\n"
			"Folder: %s\n",
			save->path,
			(save->flags & SAVE_FLAG_PS1) ? 1 : 2,
			save->name,
			save->title_id,
			save->dir_name);

		return 1;
	}

	if(save->type == FILE_TYPE_VMC)
	{
		asprintf(details, "%s\n----- Virtual Memory Card -----\n"
			"File: %s\n"
			"Folder: %s\n",
			save->path,
			strrchr(save->path, '/')+1,
			save->dir_name);

		return 1;
	}

	if(save->flags & SAVE_FLAG_PS1)
	{
		asprintf(details, "%s\n\n----- PS1 Save -----\n"
			"Game: %s\n"
			"Title ID: %s\n"
			"File: %s\n",
			save->path,
			save->name,
			save->title_id,
			save->dir_name);

		return 1;
	}

	if (save->flags & SAVE_FLAG_PS2)
	{
		asprintf(details, "%s\n----- PS2 Save -----\n"
			"Game: %s\n"
			"Title ID: %s\n"
			"Folder: %s\n"
			"Icon: %s\n",
			save->path,
			save->name,
			save->title_id,
			save->dir_name,
			save->icon);

		return 1;
	}

	asprintf(details, "%s\n----- Save -----\n"
		"Title: %s\n"
		"Title ID: %s\n"
		"Dir Name: %s\n",
		save->path, save->name,
		save->title_id,
		save->dir_name);

	return 1;
}
