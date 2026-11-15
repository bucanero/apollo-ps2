#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <curl/curl.h>

#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <netman.h>
#include <ps2ip.h>

#include "common.h"
#include "saves.h"

#define HTTP_SUCCESS 	1
#define HTTP_FAILED	 	0
#define HTTP_USER_AGENT "Mozilla/5.0 (PLAYSTATION 2; 1.00)"

// Network IOP modules, embedded from the ps2sdk build (see CMakeLists.txt)
extern const uint8_t _binary_ps2dev9_irx_start;
extern const uint8_t _binary_ps2dev9_irx_size;
extern const uint8_t _binary_netman_irx_start;
extern const uint8_t _binary_netman_irx_size;
extern const uint8_t _binary_smap_irx_start;
extern const uint8_t _binary_smap_irx_size;

#define NET_WAIT_SECONDS	10
#define NET_RETRY_SECONDS	30	// after a failed connection, don't wait again before this

enum net_state_enum
{
	NET_STATE_DOWN,
	NET_STATE_UP,
	NET_STATE_NO_ADAPTER,	// DEV9/SMAP didn't load: don't retry
};

static int net_state = NET_STATE_DOWN;
static int net_stack = 0;
static time_t net_fail_time = 0;

static int _load_irx(const char* name, const void* irx, int size)
{
	int ret, mod_res;

	ret = SifExecModuleBuffer((void*) irx, size, 0, NULL, &mod_res);
	if (ret < 0 || mod_res == 1)
	{
		// mod_res 1 = NO_RESIDENT_END: the module unloaded itself (no hardware)
		LOG("Failed to load %s (%d, %d)", name, ret, mod_res);
		return 0;
	}

	return 1;
}

static void _wait_alarm_cb(s32 alarm_id, u16 time, void *common)
{
	iWakeupThread(*(int*)common);
}

// Polls check() once per second, for up to NET_WAIT_SECONDS
static int _wait_net_state(int (*check)(void))
{
	int thread_id = GetThreadId();

	for (int i = 0; check() <= 0; i++)
	{
		if (i >= NET_WAIT_SECONDS)
			return 0;

		// ~1 s: the alarm counts HSYNC ticks
		SetAlarm(1000 * 16, &_wait_alarm_cb, &thread_id);
		SleepThread();
	}

	return 1;
}

static int _link_is_up(void)
{
	return (NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP);
}

static int _dhcp_is_bound(void)
{
	t_ip_info ip_info;

	// SMAP is registered as the "sm0" device to the TCP/IP stack
	if (ps2ip_getconfig("sm0", &ip_info) < 0 || !ip_info.dhcp_enabled)
		return -1;

	return (ip_info.dhcp_status == DHCP_STATE_BOUND || ip_info.dhcp_status == DHCP_STATE_OFF);
}

static int _enable_dhcp(void)
{
	t_ip_info ip_info;

	if (ps2ip_getconfig("sm0", &ip_info) < 0)
		return 0;

	if (ip_info.dhcp_enabled)
		return 1;

	ip_info.dhcp_enabled = 1;
	return (ps2ip_setconfig(&ip_info) >= 0);
}

static int _start_network(void)
{
	struct ip4_addr ip, nm, gw;
	t_ip_info ip_info;

	if (!_load_irx("DEV9", &_binary_ps2dev9_irx_start, (int) &_binary_ps2dev9_irx_size) ||
		!_load_irx("NETMAN", &_binary_netman_irx_start, (int) &_binary_netman_irx_size) ||
		!_load_irx("SMAP", &_binary_smap_irx_start, (int) &_binary_smap_irx_size))
	{
		net_state = NET_STATE_NO_ADAPTER;
		return 0;
	}

	NetManInit();
	NetManSetLinkMode(NETMAN_NETIF_ETH_LINK_MODE_AUTO);

	// TCP/IP runs on the EE (lwIP); the address comes from DHCP
	ip4_addr_set_zero(&ip);
	ip4_addr_set_zero(&nm);
	ip4_addr_set_zero(&gw);
	ps2ipInit(&ip, &nm, &gw);
	net_stack = 1;

	if (!_enable_dhcp())
	{
		LOG("Failed to enable DHCP");
		return 0;
	}

	return 1;
}

int network_up(void)
{
	t_ip_info ip_info;
	struct timeval now;
	int ret = HTTP_FAILED, own_screen;

	if (net_state == NET_STATE_UP)
		return HTTP_SUCCESS;

	if (net_state == NET_STATE_NO_ADAPTER)
		return HTTP_FAILED;

	// One failed attempt (no cable, no DHCP) shouldn't make every download wait again
	gettimeofday(&now, NULL);
	if (net_fail_time && now.tv_sec - net_fail_time < NET_RETRY_SECONDS)
		return HTTP_FAILED;

	// Callers may already show a loading screen: only close the one we open
	own_screen = init_loading_screen("Connecting to the network...");

	if (!net_stack && !_start_network())
		LOG("Network adapter not available");

	else if (LOG("Waiting for network link..."), !_wait_net_state(&_link_is_up))
		LOG("Network link is down");

	else if (LOG("Waiting for DHCP lease..."), !_wait_net_state(&_dhcp_is_bound))
		LOG("DHCP failed");

	else
	{
		if (ps2ip_getconfig("sm0", &ip_info) >= 0)
			LOG("Network up: IP %s", ip4addr_ntoa((const ip4_addr_t*) &ip_info.ipaddr));

		net_state = NET_STATE_UP;
		ret = HTTP_SUCCESS;
	}

	if (own_screen)
		stop_loading_screen();

	if (ret != HTTP_SUCCESS)
	{
		gettimeofday(&now, NULL);
		net_fail_time = now.tv_sec;
	}

	return ret;
}

int network_get_ip(char* ip, size_t len)
{
	t_ip_info ip_info;

	if (net_state != NET_STATE_UP || ps2ip_getconfig("sm0", &ip_info) < 0)
		return 0;

	snprintf(ip, len, "%s", ip4addr_ntoa((const ip4_addr_t*) &ip_info.ipaddr));
	return 1;
}

int http_init(void)
{
	// The network itself is only brought up when it's first needed
	curl_global_init(CURL_GLOBAL_ALL);

	return HTTP_SUCCESS;
}

/* follow the CURLOPT_XFERINFOFUNCTION callback definition */
static int update_progress(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	LOG("DL: %lld / %lld", dlnow, dltotal);
	update_progress_bar(dlnow, dltotal, (const char*) p);

	return 0;
}

/*
 * libcurl makes its sockets non-blocking with fcntl(), but here sockets are
 * lwIP's (curl calls lwip_socket/lwip_recv directly) and the SDK's fcntl
 * doesn't reach them. A blocking socket hangs curl when it closes a TLS
 * connection the server keeps open, so set it non-blocking the lwIP way.
 */
static int sockopt_callback(void *clientp, curl_socket_t fd, curlsocktype purpose)
{
	unsigned long on = 1;

	if (lwip_ioctl(fd, FIONBIO, &on) < 0)
		LOG("lwip_ioctl(FIONBIO) failed on socket %d", fd);

	return CURL_SOCKOPT_OK;
}

static int is_network_url(const char* url)
{
	return (strncasecmp(url, "http://", 7) == 0 || strncasecmp(url, "https://", 8) == 0 ||
			strncasecmp(url, "ftp://", 6) == 0 || strncasecmp(url, "ftps://", 7) == 0);
}

int http_download(const char* url, const char* filename, const char* local_dst, int show_progress)
{
	char full_url[1024];
	CURL *curl;
	CURLcode res;
	FILE* fd;

	if (!filename) filename = "";
	snprintf(full_url, sizeof(full_url), "%s%s", url, filename);
	LOG("URL: %s >> %s", full_url, local_dst);

	// A local database (e.g. "cdfs:/APOLLO/SAVES/" on an Apollo disc) is just copied
	if (!is_network_url(full_url))
	{
		if (file_exists(full_url) != SUCCESS)
			return HTTP_FAILED;

		return (copy_file(full_url, local_dst) == SUCCESS);
	}

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
		curl_easy_cleanup(curl);
		return HTTP_FAILED;
	}

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
	// lwIP sockets need to be set non-blocking by hand
	curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, sockopt_callback);
	// Give up on a stalled transfer (less than 1 byte/s for 30 seconds)
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);

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

	if (net_stack)
	{
		ps2ipDeinit();
		NetManDeinit();
	}
}
