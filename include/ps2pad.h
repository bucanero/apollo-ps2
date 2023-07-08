/*
# based on orbisPad from ORBISDEV Open Source Project.
# Copyright 2010-2020, orbisdev - http://orbisdev.github.io
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <libpad.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANALOG_CENTER       0x78
#define ANALOG_THRESHOLD    0x68
#define ANALOG_MIN          (ANALOG_CENTER - ANALOG_THRESHOLD)
#define ANALOG_MAX          (ANALOG_CENTER + ANALOG_THRESHOLD)


typedef struct Ps2PadConfig
{
	uint32_t padDataCurrent;
	uint32_t padDataLast;
	unsigned int buttonsPressed;
	unsigned int buttonsReleased;
	unsigned int buttonsHold;
	unsigned int idle;
} Ps2PadConfig;

int ps2PadInit(void);
void ps2PadFinish(void);
Ps2PadConfig *ps2PadGetConf(void);
bool ps2PadGetButtonHold(unsigned int filter);
bool ps2PadGetButtonPressed(unsigned int filter);
bool ps2PadGetButtonReleased(unsigned int filter);
unsigned int ps2PadGetCurrentButtonsPressed(void);
unsigned int ps2PadGetCurrentButtonsReleased(void);
void ps2PadSetCurrentButtonsPressed(unsigned int buttons);
void ps2PadSetCurrentButtonsReleased(unsigned int buttons);
int ps2PadUpdate(void);

#ifdef __cplusplus
}
#endif
