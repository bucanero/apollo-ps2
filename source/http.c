#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

//#include <pspnet.h>
//#include <pspnet_inet.h>
//#include <pspnet_apctl.h>
//#include <psputility.h>

#include "common.h"
#include "saves.h"

#define HTTP_SUCCESS 	1
#define HTTP_FAILED	 	0
#define HTTP_USER_AGENT "Mozilla/5.0 (PLAYSTATION PORTABLE; 1.00)"

static int net_up = 0;

void drawDialogBackground();

/*
char * basename (const char *filename)
{
	char *p = strrchr (filename, '/');
	return p ? p + 1 : (char *) filename;
}

static int Net_DisplayNetDialog(void)
{
	int ret = 0, done = 0;
	pspUtilityNetconfData data;
	struct pspUtilityNetconfAdhoc adhocparam;

	memset(&adhocparam, 0, sizeof(adhocparam));
	memset(&data, 0, sizeof(pspUtilityNetconfData));

	data.base.size = sizeof(pspUtilityNetconfData);
	data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	data.base.graphicsThread = 17;
	data.base.accessThread = 19;
	data.base.fontThread = 18;
	data.base.soundThread = 16;
	data.action = PSP_NETCONF_ACTION_CONNECTAP;
	data.hotspot = 0;
	data.adhocparam = &adhocparam;

	if ((ret = sceUtilityNetconfInitStart(&data)) < 0) {
		LOG("sceUtilityNetconfInitStart() failed: 0x%08x\n", ret);
		return ret;
	}

	while(!done)
	{
		switch(sceUtilityNetconfGetStatus()) {
			case PSP_UTILITY_DIALOG_NONE:
				done = 1;
				break;

			case PSP_UTILITY_DIALOG_VISIBLE:
				if ((ret = sceUtilityNetconfUpdate(1)) < 0) {
					LOG("sceUtilityNetconfUpdate(1) failed: 0x%08x\n", ret);
					done = 1;
				}
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				if ((ret = sceUtilityNetconfShutdownStart()) < 0) {
					LOG("sceUtilityNetconfShutdownStart() failed: 0x%08x\n", ret);
					done = 1;
				}
				break;

			case PSP_UTILITY_DIALOG_FINISHED:
				done = 1;
				break;

			default:
				break;
		}

		drawDialogBackground();
	}

	return 0;
}

static int Net_isConnected(void)
{
	int ret = 0, state = PSP_NET_APCTL_STATE_DISCONNECTED;

	if ((ret  = sceNetApctlGetState(&state)) < 0) {
		LOG("sceNetApctlGetState() failed: 0x%08x\n", ret);
		return 0;
	}

	return (state == PSP_NET_APCTL_STATE_GOT_IP);
}
*/

int network_up(void)
{
	if (net_up)
		return HTTP_SUCCESS;

//	Net_DisplayNetDialog();
//	if (!Net_isConnected())
//		return HTTP_FAILED;

	net_up = 1;

	return HTTP_SUCCESS;
}

//	scePowerLock(0);
//	scePowerUnlock(0);

int http_init(void)
{
	int ret = 0;
/*
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

	if ((ret = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024)) < 0) {
		LOG("sceNetInit() failed: 0x%08x\n", ret);
		return HTTP_FAILED;
	}

	if ((ret = sceNetInetInit()) < 0) {
		LOG("sceNetInetInit() failed: 0x%08x\n", ret);
		return HTTP_FAILED;
	}

	if ((ret = sceNetApctlInit(0x8000, 48)) < 0) {
		LOG("sceNetApctlInit() failed: 0x%08x\n", ret);
		return HTTP_FAILED;
	}
*/
	curl_global_init(CURL_GLOBAL_ALL);

	return HTTP_SUCCESS;
}

/* follow the CURLOPT_XFERINFOFUNCTION callback definition */
static int update_progress(void *p, int64_t dltotal, int64_t dlnow, int64_t ultotal, int64_t ulnow)
{
	LOG("DL: %lld / %lld", dlnow, dltotal);
	update_progress_bar(dlnow, dltotal, (const char*) p);

	return 0;
}

int http_download(const char* url, const char* filename, const char* local_dst, int show_progress)
{
	char full_url[1024];
	CURL *curl;
	CURLcode res;
	FILE* fd;

	if (network_up() != HTTP_SUCCESS)
		return HTTP_FAILED;

	curl = curl_easy_init();
	if(!curl)
	{
		LOG("ERROR: CURL INIT");
		return HTTP_FAILED;
	}

	fd = fopen(local_dst, "wb");
	if (!fd) {
		LOG("fopen Error: File path '%s'", local_dst);
		return HTTP_FAILED;
	}

	if (!filename) filename = "";
	snprintf(full_url, sizeof(full_url), "%s%s", url, filename);
	LOG("URL: %s >> %s", full_url, local_dst);

	curl_easy_setopt(curl, CURLOPT_URL, full_url);
	// Set user agent string
	curl_easy_setopt(curl, CURLOPT_USERAGENT, HTTP_USER_AGENT);
	// not sure how to use this when enabled
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	// not sure how to use this when enabled
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	// Set SSL VERSION to TLS 1.2
	curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
	// Set timeout for the connection to build
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	// Follow redirects (?)
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	// The function that will be used to write the data
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
	// The data filedescriptor which will be written to
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
	// maximum number of redirects allowed
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 20L);
	// Fail the request if the HTTP code returned is equal to or larger than 400
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	// request using SSL for the FTP transfer if available
	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);

	if (show_progress)
	{
		init_progress_bar("Downloading...");
		/* pass the struct pointer into the xferinfo function */
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &update_progress);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void*)&full_url[0]);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	}

	// Perform the request
	res = curl_easy_perform(curl);
	// close file descriptor
	fclose(fd);
	// cleanup
	curl_easy_cleanup(curl);

	if (show_progress)
		end_progress_bar();

	if(res != CURLE_OK)
	{
		LOG("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		unlink_secure(local_dst);
		return HTTP_FAILED;
	}

	return HTTP_SUCCESS;
}

void http_end(void)
{
	curl_global_cleanup();

//	sceNetApctlTerm();
//	sceNetInetTerm();
//	sceNetTerm();

//	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
//	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}
