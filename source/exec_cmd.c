#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <polarssl/md5.h>

#include "saves.h"
#include "menu.h"
#include "common.h"
#include "utils.h"
#include "ps1card.h"

static char host_buf[256];

static int _set_dest_path(char* path, int dest, const char* folder)
{
	switch (dest)
	{
	case STORAGE_MC1:
		sprintf(path, "%s%s", MC1_PATH, folder);
		break;

	case STORAGE_MC0:
		sprintf(path, "%s%s", MC0_PATH, folder);
		break;

	case STORAGE_MASS:
		sprintf(path, "%s%s", USB_PATH, folder);
		break;

	case STORAGE_HOST:
		sprintf(path, "%s%s", "host:/", folder);
		break;

	default:
		path[0] = 0;
		return 0;
	}

	return 1;
}

static void downloadSave(const save_entry_t* entry, const char* file, int dst)
{
	char path[256];

	_set_dest_path(path, dst, PS2_SAVES_PATH_USB);
	if (dst == STORAGE_MC0)
		snprintf(path, sizeof(path), PSP_SAVES_PATH_HDD);

	if (mkdirs(path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", path);
		return;
	}

	if (!http_download(entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("Error downloading save game from:\n%s%s", entry->path, file);
		return;
	}

	if (extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path))
		show_message("Save game successfully downloaded to:\n%s", path);
	else
		show_message("Error extracting save game!");

	unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");
}

static uint32_t get_filename_id(const char* dir)
{
	char path[128];
	uint32_t tid = 0;

	do
	{
		tid++;
		snprintf(path, sizeof(path), "%s%08d.zip", dir, tid);
	}
	while (file_exists(path) == SUCCESS);

	return tid;
}

static void zipSave(const save_entry_t* entry, int dst)
{
	char exp_path[256];
	char* export_file;
	char* tmp;
	uint32_t fid;

	_set_dest_path(exp_path, dst, EXPORT_PATH);
	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Exporting save game...");

	fid = get_filename_id(exp_path);
	asprintf(&export_file, "%s%08d.zip", exp_path, fid);

	tmp = strdup(entry->path);
	*strrchr(tmp, '/') = 0;
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, entry->path, export_file);

	sprintf(export_file, "%s%08x.txt", exp_path, apollo_config.user_id);
	FILE* f = fopen(export_file, "a");
	if (f)
	{
		fprintf(f, "%08d.zip=[%s] %s\n", fid, entry->title_id, entry->name);
		fclose(f);
	}

	sprintf(export_file, "%s%s", exp_path, OWNER_XML_FILE);
//	save_xml_owner(export_file);

	free(export_file);
	free(tmp);

	stop_loading_screen();
	show_message("Zip file successfully saved to:\n%s%08d.zip", exp_path, fid);
}

static void copySave(const save_entry_t* save, int dev)
{
	char* copy_path;
	char exp_path[256];

	_set_dest_path(exp_path, dev, PS2_SAVES_PATH_USB);
	if (strncmp(save->path, exp_path, strlen(exp_path)) == 0)
	{
		show_message("Copy operation cancelled!\nSame source and destination.");
		return;
	}

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Copying files...");

	asprintf(&copy_path, "%s%s/", exp_path, save->dir_name);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	copy_directory(save->path, save->path, copy_path);

	free(copy_path);

	stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}

static int _copy_save_memcard(const save_entry_t* save, int dev)
{
	char copy_path[256];

	_set_dest_path(copy_path, dev, save->dir_name);
	strcat(copy_path, "/");

	LOG("Copying <%s> to %s...", save->path, copy_path);
	return (copy_directory(save->path, save->path, copy_path) == SUCCESS);
}

static void copySaveMC(const save_entry_t* save, int dev)
{
	//source save is already on HDD
	if (save->flags & SAVE_FLAG_MEMCARD)
	{
		show_message("Copy operation cancelled!\nSame source and destination.");
		return;
	}

	init_loading_screen("Copying save game...");
	int ret = _copy_save_memcard(save, dev);
	stop_loading_screen();

	if (ret)
		show_message("Files successfully copied to:\nmc%d:/%s", dev, save->dir_name);
	else
		show_message("Error! Can't copy Save-game folder:\n%s", save->path);
}

static void copyAllSavesMC(const save_entry_t* save, int dev, int all)
{
	char mc_path[32];
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Copying all saves...");
	_set_dest_path(mc_path, dev, "");

	LOG("Copying all saves from '%s' to mc%d:/...", save->path, dev);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		if ((item->flags & SAVE_FLAG_PS1) || item->type == FILE_TYPE_PS2)
			(_copy_save_memcard(item, dev) ? done++ : err_count++);

		switch (item->type)
		{
		case FILE_TYPE_CBS:
			(importCBS(item->path, mc_path) ? done++ : err_count++);
			break;

		case FILE_TYPE_MAX:
			(importMAX(item->path, mc_path) ? done++ : err_count++);
			break;

		case FILE_TYPE_XPS:
			(importXPS(item->path, mc_path) ? done++ : err_count++);
			break;

		case FILE_TYPE_PSU:
			(importPSU(item->path, mc_path) ? done++ : err_count++);
			break;

		case FILE_TYPE_PSV:
			(importPSV(item->path, mc_path) ? done++ : err_count++);
			break;

		default:
			break;
		}
	}

	end_progress_bar();

	show_message("%d/%d Saves copied to Memory Card %d", done, done+err_count, dev+1);
}

static void copyAllSavesVMC(const save_entry_t* save, int dev, int all)
{
	char mc_path[32];
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Copying all saves...");
	_set_dest_path(mc_path, dev, "");

	LOG("Copying all saves from '%s' to mc%d:/...", save->path, dev);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		(importVMC(item->dir_name, mc_path) ? done++ : err_count++);
	}

	end_progress_bar();

	show_message("%d/%d Saves copied to Memory Card %d", done, done+err_count, dev+1);
}

static void exportAllSavesVMC(const save_entry_t* save, int dev, int all)
{
	char outPath[256];
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Exporting all VMC saves...");
	_set_dest_path(outPath, dev, PS3_SAVES_PATH_USB);
	mkdirs(outPath);

	LOG("Exporting all saves from '%s' to mc%d:/...", save->path, dev);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		(vmc_export_psv(item->dir_name, outPath) ? done++ : err_count++);
	}

	end_progress_bar();

	show_message("%d/%d Saves exported to\n%s", done, done+err_count, outPath);
}

static void extractArchive(const char* file_path)
{
	int ret = 0;
	char exp_path[256];

	strncpy(exp_path, file_path, sizeof(exp_path)-1);
	*strrchr(exp_path, '.') = 0;

	switch (strrchr(file_path, '.')[1])
	{
	case 'z':
	case 'Z':
		/* ZIP */
		strcat(exp_path, "/");
		ret = extract_zip(file_path, exp_path);
		break;

	case 'r':
	case 'R':
		/* RAR */
//		ret = extract_rar(file_path, exp_path);
		break;

	case '7':
		/* 7-Zip */
//		ret = extract_7zip(file_path, exp_path);
		break;

	default:
		break;
	}

	if (ret)
		show_message("All files extracted to:\n%s", exp_path);
	else
		show_message("Error: %s couldn't be extracted", file_path);
}

/*
static void copySavePFS(const save_entry_t* save)
{
	char src_path[256];
	char hdd_path[256];
	char mount[32];
	sfo_patch_t patch = {
		.user_id = apollo_config.user_id,
		.account_id = apollo_config.account_id,
	};

	if (!vita_SaveMount(save, mount))
	{
		LOG("[!] Error: can't create/mount save!");
		return;
	}
	vita_SaveUmount(mount);

	snprintf(src_path, sizeof(src_path), "%s%s", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), "/user/home/%08x/savedata/%s/sdimg_%s", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);
	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("[!] Error: can't copy %s", hdd_path);
		return;
	}

	snprintf(src_path, sizeof(src_path), "%s%s.bin", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), "/user/home/%08x/savedata/%s/%s.bin", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);
	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("[!] Error: can't copy %s", hdd_path);
		return;
	}

	if (!vita_SaveMount(save, mount))
	{
		LOG("[!] Error: can't remount save");
		show_message("Error! Can't mount encrypted save.\n(incompatible save-game firmware version)");
		return;
	}

	snprintf(hdd_path, sizeof(hdd_path), APOLLO_SANDBOX_PATH "sce_sys/param.sfo", mount);
	if (show_dialog(1, "Resign save %s/%s?", save->title_id, save->dir_name))
		patch_sfo(hdd_path, &patch);

//	*strrchr(hdd_path, 'p') = 0;
//	_update_save_details(hdd_path, mount);
	vita_SaveUmount(mount);

	show_message("Encrypted save copied successfully!\n%s/%s", save->title_id, save->dir_name);
	return;
}
*/
/*
static int webReqHandler(dWebRequest_t* req, dWebResponse_t* res, void* list)
{
	list_node_t *node;
	save_entry_t *item;

	// http://ps3-ip:8080/
	if (strcmp(req->resource, "/") == 0)
	{
		uint64_t hash[2];
		md5_context ctx;

		md5_starts(&ctx);
		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
			md5_update(&ctx, (uint8_t*) item->name, strlen(item->name));

		md5_finish(&ctx, (uint8_t*) hash);
		asprintf(&res->data, APOLLO_LOCAL_CACHE "web%016llx%016llx.html", hash[0], hash[1]);

		if (file_exists(res->data) == SUCCESS)
			return 1;

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		fprintf(f, "<html><head><meta charset=\"UTF-8\"><style>h1, h2 { font-family: arial; } img { display: none; } table { border-collapse: collapse; margin: 25px 0; font-size: 0.9em; font-family: sans-serif; min-width: 400px; box-shadow: 0 0 20px rgba(0, 0, 0, 0.15); } table thead tr { background-color: #009879; color: #ffffff; text-align: left; } table th, td { padding: 12px 15px; } table tbody tr { border-bottom: 1px solid #dddddd; } table tbody tr:nth-of-type(even) { background-color: #f3f3f3; } table tbody tr:last-of-type { border-bottom: 2px solid #009879; }</style>");
		fprintf(f, "<script language=\"javascript\">function show(sid,src){var im=document.getElementById('img'+sid);im.src=src;im.style.display='block';document.getElementById('btn'+sid).style.display='none';}</script>");
		fprintf(f, "<title>Apollo Save Tool</title></head><body><h1>.:: Apollo Save Tool</h1><h2>Index of %s</h2><table><thead><tr><th>Name</th><th>Icon</th><th>Title ID</th><th>Folder</th><th>Location</th></tr></thead><tbody>", selected_entry->path);

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & (SAVE_FLAG_PS1|SAVE_FLAG_PSP)))
				continue;

			fprintf(f, "<tr><td><a href=\"/zip/%08d/%s_%s.zip\">%s</a></td>", i, item->title_id, item->dir_name, item->name);
			fprintf(f, "<td><button type=\"button\" id=\"btn%d\" onclick=\"show(%d,'/icon/%08x/ICON0.PNG", i, i, i);
			fprintf(f, "')\">Show Icon</button><img id=\"img%d\" alt=\"%s\" height=\"80\"></td>", i, item->name);
			fprintf(f, "<td>%s</td>", item->title_id);
			fprintf(f, "<td>%s</td>", item->dir_name);
			fprintf(f, "<td>%.4s</td></tr>", item->path);
		}

		fprintf(f, "</tbody></table></body></html>");
		fclose(f);
		return 1;
	}

	// http://psp-ip:8080/PSP/games.txt
	if (wildcard_match(req->resource, "/PSP/games.txt"))
	{
		asprintf(&res->data, "%s%s", APOLLO_LOCAL_CACHE, "PSP_games.txt");

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PSP))
				continue;

			fprintf(f, "%s=%s\n", item->title_id, item->name);
		}

		fclose(f);
		return 1;
	}

	// http://psp-ip:8080/PSP/BLUS12345/saves.txt
	if (wildcard_match(req->resource, "/PSP/\?\?\?\?\?\?\?\?\?/saves.txt"))
	{
		asprintf(&res->data, "%sweb%.9s_saves.txt", APOLLO_LOCAL_CACHE, req->resource + 5);

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PSP) || strncmp(item->title_id, req->resource + 5, 9))
				continue;

			fprintf(f, "%08d.zip=(%s) %s\n", i, item->dir_name, item->name);
		}

		fclose(f);
		return 1;
	}

	// http://ps3-ip:8080/zip/00000000/CUSA12345_DIR-NAME.zip
	// http://psp-ip:8080/PSP/BLUS12345/00000000.zip
	if (wildcard_match(req->resource, "/zip/\?\?\?\?\?\?\?\?/\?\?\?\?\?\?\?\?\?_*.zip") ||
		wildcard_match(req->resource, "/PSP/\?\?\?\?\?\?\?\?\?/*.zip"))
	{
		char *base, *path;
		int id = 0;

		sscanf(req->resource + (strncmp(req->resource, "/PSP", 4) == 0 ? 15 : 5), "%08d", &id);
		item = list_get_item(list, id);
		asprintf(&res->data, "%s%s_%s.zip", APOLLO_LOCAL_CACHE, item->title_id, item->dir_name);

		base = strdup(item->path);
		path = strdup(item->path);
		*strrchr(base, '/') = 0;
		*strrchr(base, '/') = 0;

		id = zip_directory(base, path, res->data);

		free(base);
		free(path);
		return id;
	}

	// http://psp-ip:8080/PSP/BLUS12345/ICON0.PNG
	if (wildcard_match(req->resource, "/PSP/\?\?\?\?\?\?\?\?\?/ICON0.PNG"))
	{
		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PSP) || strncmp(item->title_id, req->resource + 5, 9))
				continue;

			asprintf(&res->data, "%sICON0.PNG", item->path);
			return (file_exists(res->data) == SUCCESS);
		}

		return 0;
	}

	// http://vita-ip:8080/icon/00000000/ICON0.PNG
	if (wildcard_match(req->resource, "/icon/\?\?\?\?\?\?\?\?/ICON0.PNG"))
	{
		int id = 0;

		sscanf(req->resource + 6, "%08x", &id);
		item = list_get_item(list, id);
		asprintf(&res->data, "%sICON0.PNG", item->path);

		return (file_exists(res->data) == SUCCESS);
	}

	return 0;
}
*/
static void enableWebServer(dWebReqHandler_t handler, void* data, int port)
{
/*
	union SceNetApctlInfo ip_info;

	if (!network_up())
		return;

	memset(&ip_info, 0, sizeof(ip_info));
	sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &ip_info);
	LOG("Starting local web server %s:%d ...", ip_info.ip, port);

	if (dbg_webserver_start(port, handler, data))
	{
		show_message("Web Server on http://%s:%d\nPress OK to stop the Server.", ip_info.ip, port);
		dbg_webserver_stop();
	}
	else show_message("Error starting Web Server!");
*/
}

static void importSave(const save_entry_t* save, const char* mc_path)
{
	int ret = 0;

	LOG("Importing save game '%s'... {%s}", save->path, mc_path);
	init_progress_bar("Importing save...");

	switch (save->type)
	{
	case FILE_TYPE_CBS:
		ret = importCBS(save->path, mc_path);
		break;

	case FILE_TYPE_MAX:
		ret = importMAX(save->path, mc_path);
		break;

	case FILE_TYPE_XPS:
		ret = importXPS(save->path, mc_path);
		break;

	case FILE_TYPE_PSU:
		ret = importPSU(save->path, mc_path);
		break;

	case FILE_TYPE_PSV:
		if (save->flags & SAVE_FLAG_PS1)
			ret = importPS1psv(save->path, mc_path, save->dir_name);
		else
			ret = importPSV(save->path, mc_path);
		break;

	case FILE_TYPE_MCS:
		ret = importPS1mcs(save->path, mc_path, save->dir_name);
		break;

	case FILE_TYPE_PSX:
		ret = importPS1psx(save->path, mc_path, save->dir_name);
		break;

	default:
		break;
	}

	end_progress_bar();

	if (ret)
		show_message("Save successfully imported to %s", mc_path);
	else
		show_message("Error: save couldn't be imported");
}

static void copyAllSavesUSB(const save_entry_t* save, int dev, int all)
{
	char dst_path[256];
	char copy_path[256];
	int done = 0, err_count = 0;
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	_set_dest_path(dst_path, dev, (save->flags & SAVE_FLAG_PS1) ? PS1_SAVES_PATH_USB : PS2_SAVES_PATH_USB);
	if (!list || mkdirs(dst_path) != SUCCESS)
	{
		show_message("Error! Folder is not available:\n%s", dst_path);
		return;
	}

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves to '%s'...", dst_path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		snprintf(copy_path, sizeof(copy_path), "%s%s/", dst_path, item->dir_name);
		if (item->flags & SAVE_FLAG_PS1 && !(item->flags & SAVE_FLAG_PS1CARD))
			strchr(copy_path, '2')[0] = '1';
		LOG("Copying <%s> to %s...", item->path, copy_path);

		if (item->flags & SAVE_FLAG_PS1CARD)
		{
			char src_path[256];

			strcat(copy_path, item->dir_name);
			snprintf(src_path, sizeof(src_path), "%s%s", item->path, item->dir_name);
			(copy_file(src_path, copy_path) == SUCCESS) ? done++ : err_count++;
			continue;
		}

		if ((item->flags & SAVE_FLAG_PS1) || item->type == FILE_TYPE_PS2)
			(copy_directory(item->path, item->path, copy_path) == SUCCESS) ? done++ : err_count++;
	}

	end_progress_bar();
	show_message("%d/%d Saves copied to:\n%s", done, done+err_count, dst_path);
}

static void* ps2_host_callback(int id, int* size)
{
	memset(host_buf, 0, sizeof(host_buf));

	switch (id)
	{
	case APOLLO_HOST_TEMP_PATH:
		return APOLLO_LOCAL_CACHE;

	case APOLLO_HOST_LAN_ADDR:
		if (1)//(sceWlanGetEtherAddr(host_buf) < 0)
			LOG("Error getting LAN Ethernet Address");

		if (size) *size = 6;
		return host_buf;
	}

	if (size) *size = 1;
	return host_buf;
}

static int psp_is_decrypted(list_t* list, const char* fname)
{
	list_node_t *node;

	for (node = list_head(list); node; node = list_next(node))
		if (strcmp(list_get(node), fname) == 0)
			return 1;

	return 0;
}

static int apply_cheat_patches(const save_entry_t* entry)
{
	int ret = 1;
	char tmpfile[256];
	char* filename;
	code_entry_t* code;
	list_node_t* node;
	list_t* decrypted_files = list_alloc();
	uint8_t key[16];

	init_loading_screen("Applying changes...");

	for (node = list_head(entry->codes); (code = list_get(node)); node = list_next(node))
	{
		if (!code->activated || (code->type != PATCH_GAMEGENIE && code->type != PATCH_BSD))
			continue;

		LOG("Active code: [%s]", code->name);

		if (strrchr(code->file, '\\'))
			filename = strrchr(code->file, '\\')+1;
		else
			filename = code->file;

		if (strchr(filename, '*'))
			filename = code->options[0].name[code->options[0].sel];

		if (strstr(code->file, "~extracted\\"))
			snprintf(tmpfile, sizeof(tmpfile), "%s[%s]%s", APOLLO_LOCAL_CACHE, entry->title_id, filename);
		else
		{
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", entry->path, filename);
			LOG("Decrypting file '%s'", tmpfile);

if(0)//			if (entry->flags & SAVE_FLAG_PSP && !psp_is_decrypted(decrypted_files, filename))
			{
if(0)//				if (get_psp_save_key(entry, key) && psp_DecryptSavedata(entry->path, tmpfile, key))
				{
					LOG("Decrypted PSP file '%s'", filename);
					list_append(decrypted_files, strdup(filename));
				}
				else
				{
					LOG("Error: failed to decrypt (%s)", filename);
					ret = 0;
					continue;
				}
			}
		}

		if (!apply_cheat_patch_code(tmpfile, entry->title_id, code, &ps2_host_callback))
		{
			LOG("Error: failed to apply (%s)", code->name);
			ret = 0;
		}

		code->activated = 0;
	}

	for (node = list_head(decrypted_files); (filename = list_get(node)); node = list_next(node))
	{
		LOG("Encrypting '%s'...", filename);
if(0)//		if (!get_psp_save_key(entry, key) || !psp_EncryptSavedata(entry->path, filename, key))
		{
			LOG("Error: failed to encrypt (%s)", filename);
			ret = 0;
		}

		free(filename);
	}

	list_free(decrypted_files);
	free_patch_var_list();
	stop_loading_screen();

	return ret;
}

static void resignSave(save_entry_t* entry)
{
    LOG("Resigning save '%s'...", entry->name);

    LOG("Applying cheats to '%s'...", entry->name);
    if (!apply_cheat_patches(entry))
        show_message("Error! Cheat codes couldn't be applied");

    show_message("Save %s successfully modified!", entry->title_id);
}

static int deleteSave(const save_entry_t* save)
{
	char path[256];
	int ret = 0;

	if (!show_dialog(DIALOG_TYPE_YESNO, "Do you want to delete %s?", save->dir_name))
		return 0;

	if (save->flags & SAVE_FLAG_VMC)
		ret = (save->flags & SAVE_FLAG_PS1) ? formatSave(save->icon[0]) : vmc_delete_save(save->dir_name);

	else if (save->flags & SAVE_FLAG_PACKED)
		ret = (remove(save->path) == SUCCESS);

	else if (save->flags & SAVE_FLAG_MEMCARD)
	{
		if (save->flags & SAVE_FLAG_PS1CARD)
		{
			snprintf(path, sizeof(path), "%s%s", save->path, save->dir_name);
			ret = (remove(path) == SUCCESS);
		}
		else
		{
			clean_directory(save->path);
			ret = (remove(save->path) == SUCCESS);
		}
	}
	else if (save->flags & SAVE_FLAG_PS1|SAVE_FLAG_PS2)
	{
		clean_directory(save->path);
		ret = (remove(save->path) == SUCCESS);
	}

	if (ret)
		show_message("Save successfully deleted:\n%s", save->dir_name);
	else
		show_message("Error! Couldn't delete save:\n%s", save->dir_name);

	return ret;
}

static void exportAllSaves(const save_entry_t* save, int dev, int all)
{
	struct tm t;
	char outPath[256];
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Exporting all saves...");
	_set_dest_path(outPath, dev, (save->flags & SAVE_FLAG_PS1) ? PS1_SAVES_PATH_USB : PS2_SAVES_PATH_USB);
	mkdirs(outPath);

	LOG("Exporting all saves from '%s' to %s...", save->path, outPath);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!(all || item->flags & SAVE_FLAG_SELECTED))
			continue;

		gmtime_r(&(time_t){time(NULL)}, &t);
		sprintf(strrchr(outPath, '/'), "/%s_%d-%02d-%02d_%02d%02d%02d.%s", item->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
			(item->type == FILE_TYPE_PS1) ? "mcs" : "psu");

		switch (item->type)
		{
		case FILE_TYPE_PS1:
			(exportMCS(item->path, item->dir_name, outPath) ? done++ : err_count++);
			break;
		
		case FILE_TYPE_PS2:
			(exportPSU(item->path, outPath) ? done++ : err_count++);
			break;
		}

	}

	end_progress_bar();

	*strrchr(outPath, '/') = 0;
	show_message("%d/%d Saves exported to:\n%s", done, done+err_count, outPath);
}

static void export_ps2save(const save_entry_t* save, int type, int dst_id)
{
	int ret = 0;
	char outPath[256];
	struct tm t;

	_set_dest_path(outPath, dst_id, (type == FILE_TYPE_PSV) ? PS3_SAVES_PATH_USB : USER_PATH_USB);
	mkdirs(outPath);
	if (type != FILE_TYPE_PSV)
	{
		// build file path
		gmtime_r(&(time_t){time(NULL)}, &t);
		sprintf(strrchr(outPath, '/'), "/%s_%d-%02d-%02d_%02d%02d%02d.", save->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	}
	init_progress_bar("Exporting save...");

	switch (type)
	{
	case FILE_TYPE_CBS:
		strcat(outPath, "cbs");
		ret = exportCBS(save->path, outPath, save->name);
		break;

	case FILE_TYPE_PSV:
		ret = exportPSV(save->path, outPath);
		break;

	case FILE_TYPE_PSU:
		strcat(outPath, "psu");
		ret = exportPSU(save->path, outPath);
		break;

	default:
		break;
	}
	end_progress_bar();

	if (ret)
		show_message("Save successfully exported to:\n%s", outPath);
	else
		show_message("Error exporting save:\n%s", save->path);
}

static void export_vmcsave(const save_entry_t* save, int type, int dst_id)
{
	int ret = 0;
	char outPath[256];
	struct tm t;

	_set_dest_path(outPath, dst_id, (type == FILE_TYPE_PSV) ? PS3_SAVES_PATH_USB : USER_PATH_USB);
	mkdirs(outPath);
	if (type != FILE_TYPE_PSV)
	{
		// build file path
		gmtime_r(&(time_t){time(NULL)}, &t);
		sprintf(strrchr(outPath, '/'), "/%s_%d-%02d-%02d_%02d%02d%02d.psu", save->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	}

	init_progress_bar("Exporting save ...");
	switch (type)
	{
	case FILE_TYPE_PSV:
		ret = vmc_export_psv(save->dir_name, outPath);
		break;

	case FILE_TYPE_PSU:
		ret = vmc_export_psu(save->dir_name, outPath);
		break;

	default:
		break;
	}
	end_progress_bar();

	if (ret)
		show_message("Save successfully exported to:\n%s", outPath);
	else
		show_message("Error exporting save:\n%s", save->path);
}

static void exportVmc1Save(const save_entry_t* save, int type, int dst_id)
{
	char outPath[256];
	struct tm t;

	_set_dest_path(outPath, dst_id, (type == PS1SAVE_PSV) ? PS3_SAVES_PATH_USB : PS1_SAVES_PATH_USB);
	mkdirs(outPath);
	if (type != PS1SAVE_PSV)
	{
		// build file path
		gmtime_r(&(time_t){time(NULL)}, &t);
		sprintf(strrchr(outPath, '/'), "/%s_%d-%02d-%02d_%02d%02d%02d.%s", save->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
			(type == PS1SAVE_MCS) ? "mcs" : "psx");
	}

	if (saveSingleSave(outPath, save->icon[0], type))
		show_message("Save successfully exported to:\n%s", outPath);
	else
		show_message("Error exporting save:\n%s", save->path);
}

static void export_ps1vmc(const char* vm1_file, int dst, int vmp)
{
	char dstfile[256];
	char dst_path[256];

	_set_dest_path(dst_path, dst, IMP_PS1VMC_PATH_USB);
	if (mkdirs(dst_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", dst_path);
		return;
	}

	snprintf(dstfile, sizeof(dstfile), "%s%s.%s", dst_path, vm1_file, vmp ? "VMP" : "VM1");

	if (saveMemoryCard(dstfile, vmp ? PS1CARD_VMP : PS1CARD_RAW, 0))
		show_message("Memory card successfully exported to:\n%s", dstfile);
	else
		show_message("Error! Failed to export PS1 memory card");
}

static void import_save2vmc(const char* src, int type)
{
	int ret = 0;

	init_progress_bar("Importing save ...");
	switch (type)
	{
	case FILE_TYPE_PSV:
		ret = vmc_import_psv(src);
		break;

	case FILE_TYPE_PSU:
		ret = vmc_import_psu(src);
		break;

	default:
		break;
	}
	end_progress_bar();

	if (ret)
		show_message("Successfully imported to VMC:\n%s", src);
	else
		show_message("Error importing save:\n%s", src);
}

static void copyVmcSave(const save_entry_t* save, const char* mc_path)
{
	init_progress_bar("Copying save game...");
	int ret = (dir_exists(mc_path) == SUCCESS) && importVMC(save->dir_name, mc_path);
	end_progress_bar();

	if (ret)
		show_message("Files successfully copied to:\n%s%s", mc_path, save->dir_name);
	else
		show_message("Error! Can't copy Save-game folder:\n%s", save->path);
}

static int _copy_save_file(const char* src_path, const char* dst_path, const char* filename)
{
	char src[256], dst[256];

	snprintf(src, sizeof(src), "%s%s", src_path, filename);
	snprintf(dst, sizeof(dst), "%s%s", dst_path, filename);

	return (copy_file(src, dst) == SUCCESS);
}

static void exportSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/", entry->title_id);
	mkdirs(path);

	LOG("Decrypt '%s%s' to '%s'...", entry->path, filename, path);

	if (_copy_save_file(entry->path, path, filename))
	{
		strlcat(path, filename, sizeof(path));
if(0)//		if (entry->flags & SAVE_FLAG_PSP && !psp_DecryptSavedata(entry->path, path, key))
			show_message("Error! File %s couldn't be exported", filename);

		show_message("File successfully exported to:\n%s", path);
	}
	else
		show_message("Error! File %s couldn't be exported", filename);
}

static void importSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/%s", entry->title_id, filename);
	if (file_exists(path) != SUCCESS)
	{
		show_message("Error! Can't find save-game file:\n%s", path);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/", entry->title_id);
	LOG("Encrypt '%s%s' to '%s'...", path, filename, entry->path);

	if (_copy_save_file(path, entry->path, filename))
	{
if(0)//		if (entry->flags & SAVE_FLAG_PSP && !psp_EncryptSavedata(entry->path, filename, key))
			show_message("Error! File %s couldn't be imported", filename);

		show_message("File successfully imported to:\n%s%s", entry->path, filename);
	}
	else
		show_message("Error! File %s couldn't be imported", filename);
}

static void downloadLink(const char* path)
{
	char url[256] = "http://";
	char out_path[256];

	if (!osk_dialog_get_text("Download URL", url, sizeof(url)))
		return;

	snprintf(out_path, sizeof(out_path), "%s%s", path, strrchr(url, '/')+1);

	if (http_download(url, NULL, out_path, 1))
		show_message("File successfully downloaded to:\n%s", out_path);
	else
		show_message("Error! File couldn't be downloaded");
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
	switch (codecmd[0])
	{
		case CMD_EXPORT_DATA_FILE:
			exportSaveFile(selected_entry, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_USB:
			downloadSave(selected_entry, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_USB:
			zipSave(selected_entry, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_USB:
			copySave(selected_entry, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_HDD:
			copySaveMC(selected_entry, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_EXP_PS2SAVE:
			export_ps2save(selected_entry, code->options[0].id, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_EXP_VMC2SAVE:
			export_vmcsave(selected_entry, code->options[0].id, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_IMP_VMC2SAVE:
			import_save2vmc(code->file, codecmd[1]);
			selected_entry->flags |= SAVE_FLAG_UPDATED;
			code->activated = 0;
			break;

		case CMD_EXP_PS1SAVE:
		case CMD_EXP_VMC1SAVE:
			exportVmc1Save(selected_entry, code->options[0].id, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_IMP_VMC1SAVE:
			if (openSingleSave(code->file, (int*) host_buf))
				show_message("Save successfully imported:\n%s", code->file);
			else
				show_message("Error! Couldn't import save:\n%s", code->file);

			selected_entry->flags |= SAVE_FLAG_UPDATED;
			code->activated = 0;
			break;

		case CMD_EXP_PS1_VM1:
		case CMD_EXP_PS1_VMP:
			export_ps1vmc(code->file, codecmd[1], codecmd[0] == CMD_EXP_PS1_VMP);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_USB:
		case CMD_COPY_ALL_SAVES_USB:
			copyAllSavesUSB(selected_entry, codecmd[1], codecmd[0] == CMD_COPY_ALL_SAVES_USB);
			code->activated = 0;
			break;

		case CMD_IMP_SAVE_MC:
			importSave(selected_entry, codecmd[1] ? MC1_PATH : MC0_PATH);
			code->activated = 0;
			break;

		case CMD_RESIGN_SAVE:
			resignSave(selected_entry);
			code->activated = 0;
			break;

		case CMD_DELETE_SAVE:
			if (deleteSave(selected_entry))
				selected_entry->flags |= SAVE_FLAG_UPDATED;
			else
				code->activated = 0;
			break;

		case CMD_COPY_SAVE_VMC:
			copyVmcSave(selected_entry, codecmd[1] ? MC1_PATH : MC0_PATH);
			code->activated = 0;
			break;

		case CMD_EXPORT_SAVES:
		case CMD_EXPORT_ALL_SAVES:
			exportAllSaves(selected_entry, codecmd[1], codecmd[0] == CMD_EXPORT_ALL_SAVES);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_HDD:
		case CMD_COPY_ALL_SAVES_HDD:
			copyAllSavesMC(selected_entry, codecmd[1], codecmd[0] == CMD_COPY_ALL_SAVES_HDD);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_VMC:
		case CMD_COPY_ALL_SAVES_VMC:
			copyAllSavesVMC(selected_entry, codecmd[1], codecmd[0] == CMD_COPY_ALL_SAVES_VMC);
			code->activated = 0;
			break;

		case CMD_EXP_SAVES_VMC:
		case CMD_EXP_ALL_SAVES_VMC:
			exportAllSavesVMC(selected_entry, codecmd[1], codecmd[0] == CMD_COPY_ALL_SAVES_VMC);
			code->activated = 0;
			break;

		case CMD_SAVE_WEBSERVER:
//			enableWebServer(webReqHandler, ((void**)selected_entry->dir_name)[0], 8080);
			code->activated = 0;
			break;

		case CMD_IMPORT_DATA_FILE:
			importSaveFile(selected_entry, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_EXTRACT_ARCHIVE:
			extractArchive(code->file);
			code->activated = 0;
			break;

		case CMD_URL_DOWNLOAD:
			downloadLink(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_NET_WEBSERVER:
//			enableWebServer(dbg_simpleWebServerHandler, NULL, 8080);
			code->activated = 0;
			break;

		default:
			break;
	}

	return;
}
