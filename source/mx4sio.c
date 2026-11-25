/*
 * MX4SIO: SD card adapter in Memory Card slot 2
 *
 * iop/mx4sio_bd.irx is ps2sdk's driver at commit edf475f4 (2023-02-02), with
 * the SanDisk SD card fixes the ps2sdk we build with lacks:
 *   https://github.com/ps2dev/ps2sdk/tree/edf475f45af9b80377a62a4797936d20efca6646/iop/sio/mx4sio_bd
 * (see CMakeLists.txt for why that version).
 *
 * mx4sio_bd.irx is a BDM block device driver, like
 * usbmass_bd: the SD card is mounted by bdmfs_fatfs (loaded at start-up, with
 * the USB stack) as one more "massN:" unit. This ps2sdk's bdmfs_fatfs only
 * registers "mass" (no "mx4sio:" device), numbered in connection order, so the
 * SD card's unit is found by asking each one for its block device driver name
 * ("sdc"), or, if that query isn't answered, as the unit that showed up after
 * the driver was loaded.
 *
 * The driver is only loaded when the MX4SIO source is selected:
 *  - by then a USB drive already got "mass0:", so "mass:/" stays the USB drive
 *  - it takes Memory Card slot 2 for good (sio2man transfers to that port are
 *    blocked until the next IOP reset): mc1: disappears while the app runs.
 *    It isn't loaded if a Memory Card answers in slot 2.
 *
 * The driver checks for the card from a low priority IOP thread, once per
 * second: after loading it, wait a few seconds for the card to be mounted.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <loadfile.h>
// The driver name is an ioctl2 on a fileXio handle, which posix calls can't send.
// Only this file's own handles (opened, queried, closed here) use fileXio directly.
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <usbhdfsd-common.h>

#include "menu.h"
#include "saves.h"
#include "common.h"
#include "mx4sio.h"

extern const uint8_t _binary_mx4sio_bd_irx_start;
extern const uint8_t _binary_mx4sio_bd_irx_size;

#define MX4SIO_DRIVER_NAME	"sdc"
#define MX4SIO_MAX_UNITS	10
#define MX4SIO_WAIT_SECONDS	10

enum mx4sio_state
{
	MX4SIO_NOT_LOADED,
	MX4SIO_LOADED,
	MX4SIO_SLOT2_MEMCARD,	// slot 2 holds a Memory Card: not loaded
	MX4SIO_FAILED,			// the driver didn't load: don't retry
};

static int mx4sio_state = MX4SIO_NOT_LOADED;
static unsigned int units_before = 0;	// "massN:" units mounted before the driver was loaded

/*
 * Mounted "massN:" units (bit N), with their block device driver names
 * ("usb", "sdc"...; empty if the unit doesn't answer the query)
 */
static unsigned int scan_units(char names[MX4SIO_MAX_UNITS][16])
{
	char dev[16];
	unsigned int units = 0;
	int fd;

	for (int i = 0; i < MX4SIO_MAX_UNITS; i++)
	{
		names[i][0] = 0;
		snprintf(dev, sizeof(dev), "mass%d:/", i);
		if ((fd = fileXioDopen(dev)) < 0)
			continue;

		units |= (1 << i);
		memset(names[i], 0, sizeof(names[i]));
		// when the query isn't supported, the buffer holds stale RPC data: ignore it
		if (fileXioIoctl2(fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, names[i], sizeof(names[i]) - 1) < 0)
			names[i][0] = 0;
		fileXioDclose(fd);
	}

	return units;
}

static void log_units(unsigned int units, char names[MX4SIO_MAX_UNITS][16])
{
	if (!units)
		LOG("MX4SIO: no mass units mounted");

	for (int i = 0; i < MX4SIO_MAX_UNITS; i++)
		if (units & (1 << i))
			LOG("MX4SIO: mass%d: driver '%s'%s", i, names[i], (units_before & (1 << i)) ? "" : " (new)");
}

/*
 * "massN:/" of the SD card, if it's mounted: the unit whose driver is "sdc",
 * or else a unit that showed up after the driver was loaded (if the query
 * isn't answered) and isn't a USB drive plugged in meanwhile
 */
static int find_sd_unit(char* path, size_t len, int log)
{
	char names[MX4SIO_MAX_UNITS][16];
	unsigned int units = scan_units(names);
	int found = -1;

	for (int i = 0; i < MX4SIO_MAX_UNITS && found < 0; i++)
		if ((units & (1 << i)) && strcmp(names[i], MX4SIO_DRIVER_NAME) == 0)
			found = i;

	for (int i = 0; i < MX4SIO_MAX_UNITS && found < 0; i++)
		if ((units & ~units_before & (1 << i)) && strcmp(names[i], "usb") != 0)
			found = i;

	if (log)
		log_units(units, names);

	if (found < 0)
		return 0;

	snprintf(path, len, "mass%d:/", found);
	return 1;
}

static int load_driver(void)
{
	char names[MX4SIO_MAX_UNITS][16];
	int id, ret;

	// The driver would take over slot 2: keep a Memory Card there usable
	if (dir_exists(MC1_PATH) == SUCCESS)
	{
		LOG("MX4SIO: Memory Card found in slot 2, driver not loaded");
		return MX4SIO_SLOT2_MEMCARD;
	}

	units_before = scan_units(names);
	LOG("MX4SIO: mass units before loading the driver: 0x%x", units_before);

	id = SifExecModuleBuffer((void*) &_binary_mx4sio_bd_irx_start, (int) &_binary_mx4sio_bd_irx_size, 0, NULL, &ret);
	if (id < 0 || ret == 1)
	{
		LOG("MX4SIO: failed to load mx4sio_bd (%d, %d)", id, ret);
		return MX4SIO_FAILED;
	}

	LOG("MX4SIO: driver loaded");
	return MX4SIO_LOADED;
}

int mx4sio_get_path(char* path, size_t len)
{
	int own_screen;

	path[0] = 0;

	if (mx4sio_state == MX4SIO_LOADED && find_sd_unit(path, len, 0))
		return 1;

	own_screen = init_loading_screen("Waiting for the MX4SIO SD card...");

	// a Memory Card may have been taken out of slot 2 since: check again
	if (mx4sio_state == MX4SIO_NOT_LOADED || mx4sio_state == MX4SIO_SLOT2_MEMCARD)
		mx4sio_state = load_driver();

	if (mx4sio_state == MX4SIO_LOADED)
	{
		for (int i = 0; i < MX4SIO_WAIT_SECONDS * 2 && !find_sd_unit(path, len, 0); i++)
			usleep(500 * 1000);

		// what was found, to tell why the SD card wasn't (or which unit it is)
		find_sd_unit(path, len, 1);
	}

	if (own_screen)
		stop_loading_screen();

	if (mx4sio_state == MX4SIO_SLOT2_MEMCARD)
	{
		show_message("MX4SIO not available:\nthere's a Memory Card in slot 2");
		return 0;
	}

	if (!path[0])
	{
		LOG("MX4SIO: no SD card found");
		show_message("No MX4SIO SD card found in slot 2");
		return 0;
	}

	LOG("MX4SIO: SD card mounted as %s", path);
	return 1;
}
