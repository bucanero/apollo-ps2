/*
# based on orbisPad from ORBISDEV Open Source Project.
# Copyright 2010-2020, orbisdev - http://orbisdev.github.io
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <dbglogger.h>
#include <SDL2/SDL.h>
#include "ps2pad.h"

#define LOG dbglogger_log

static Ps2PadConfig ps2PadConf;
static int orbispad_initialized = 0;
static uint64_t g_time;
static int sCurrentGameControllerIndex = -1;
static SDL_GameController *sCurrentGameController = NULL;


static SDL_GameController *getGameController(void)
{
    // The game controller is not valid anymore, invalidate.
    if (sCurrentGameControllerIndex != -1
        && SDL_IsGameController(sCurrentGameControllerIndex) == 0)
    {
        sCurrentGameController = NULL;
    }

    if (sCurrentGameController != NULL)
    {
        return sCurrentGameController;
    }

    int numberOfJoysticks = SDL_NumJoysticks();

    for (int i = 0; i < numberOfJoysticks; ++i)
    {
        if (SDL_IsGameController(i))
        {
            sCurrentGameController = SDL_GameControllerOpen(i);
            sCurrentGameControllerIndex = i;
            break;
        }
    }

    return sCurrentGameController;
}

static uint64_t timeInMilliseconds(void)
{
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((uint64_t)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void ps2PadFinish(void)
{
	int ret;

	if(orbispad_initialized)
	{
		LOG("scePadClose");
	}
	orbispad_initialized=0;

	LOG("ORBISPAD finished");
}

Ps2PadConfig *ps2PadGetConf(void)
{
	if(orbispad_initialized)
	{
		return (&ps2PadConf);
	}

	return NULL;
}

static int ps2PadInitConf(void)
{
	if(orbispad_initialized)
	{
		return orbispad_initialized;
	}

	memset(&ps2PadConf, 0, sizeof(Ps2PadConfig));

	return 0;
}

unsigned int ps2PadGetCurrentButtonsPressed(void)
{
	return ps2PadConf.buttonsPressed;
}

void ps2PadSetCurrentButtonsPressed(unsigned int buttons)
{
	ps2PadConf.buttonsPressed=buttons;
}

unsigned int ps2PadGetCurrentButtonsReleased(void)
{
	return ps2PadConf.buttonsReleased;
}

void ps2PadSetCurrentButtonsReleased(unsigned int buttons)
{
	ps2PadConf.buttonsReleased=buttons;
}

bool ps2PadGetButtonHold(unsigned int filter)
{
	uint64_t time = timeInMilliseconds();
	uint64_t delta = time - g_time;

	if((ps2PadConf.buttonsHold&filter)==filter && delta > 0x4000)
	{
		g_time = time;
		return 1;
	}

	return 0;
}

bool ps2PadGetButtonPressed(unsigned int filter)
{
	if((ps2PadConf.buttonsPressed&filter)==filter)
	{
		ps2PadConf.buttonsPressed ^= filter;
		return 1;
	}

	return 0;
}

bool ps2PadGetButtonReleased(unsigned int filter)
{
 	if((ps2PadConf.buttonsReleased&filter)==filter)
	{
		if(~(ps2PadConf.padDataLast)&filter)
		{
			return 0;
		}
		return 1;
	}

	return 0;
}

int ps2PadUpdate(void)
{
	unsigned int actualButtons=0;
	unsigned int lastButtons=0;

	ps2PadConf.padDataLast = ps2PadConf.padDataCurrent;
	SDL_GameControllerUpdate();

/*
	ret = SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_A);
	if (ret) LOG("SDL_CONTROLLER_BUTTON_A: %d", ret);
	ret = SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_B);
	if (ret) LOG("SDL_CONTROLLER_BUTTON_B: %d", ret);
*/
	ps2PadConf.padDataCurrent = 0;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_DPAD_UP))
		ps2PadConf.padDataCurrent |= PAD_UP;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
		ps2PadConf.padDataCurrent |= PAD_DOWN;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
		ps2PadConf.padDataCurrent |= PAD_LEFT;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
		ps2PadConf.padDataCurrent |= PAD_RIGHT;

	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_A))
		ps2PadConf.padDataCurrent |= PAD_CROSS;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_B))
		ps2PadConf.padDataCurrent |= PAD_CIRCLE;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_X))
		ps2PadConf.padDataCurrent |= PAD_SQUARE;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_Y))
		ps2PadConf.padDataCurrent |= PAD_TRIANGLE;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_START))
		ps2PadConf.padDataCurrent |= PAD_START;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_BACK))
		ps2PadConf.padDataCurrent |= PAD_SELECT;

	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
		ps2PadConf.padDataCurrent |= PAD_L1;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
		ps2PadConf.padDataCurrent |= PAD_R1;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_LEFTSTICK))
		ps2PadConf.padDataCurrent |= PAD_L3;
	if (SDL_GameControllerGetButton(sCurrentGameController, SDL_CONTROLLER_BUTTON_RIGHTSTICK))
		ps2PadConf.padDataCurrent |= PAD_R3;
	if (SDL_GameControllerGetAxis(sCurrentGameController, SDL_CONTROLLER_AXIS_TRIGGERLEFT))
		ps2PadConf.padDataCurrent |= PAD_L2;
	if (SDL_GameControllerGetAxis(sCurrentGameController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
		ps2PadConf.padDataCurrent |= PAD_R2;

/*
		if (buttons.ljoy_v < ANALOG_MIN)
			ps2PadConf.padDataCurrent |= PAD_UP;

		if (buttons.ljoy_v > ANALOG_MAX)
			ps2PadConf.padDataCurrent |= PAD_DOWN;

		if (buttons.ljoy_h < ANALOG_MIN)
			ps2PadConf.padDataCurrent |= PAD_LEFT;

		if (buttons.ljoy_h > ANALOG_MAX)
			ps2PadConf.padDataCurrent |= PAD_RIGHT;
*/

	actualButtons=ps2PadConf.padDataCurrent;
	lastButtons=ps2PadConf.padDataLast;
	ps2PadConf.buttonsPressed=(actualButtons)&(~lastButtons);
	if(actualButtons!=lastButtons)
	{
		ps2PadConf.buttonsReleased=(~actualButtons)&(lastButtons);
		ps2PadConf.idle=0;
	}
	else
	{
		ps2PadConf.buttonsReleased=0;
		if (actualButtons == 0)
		{
			ps2PadConf.idle++;
		}
	}
	ps2PadConf.buttonsHold=actualButtons&lastButtons;

	return 0;
}

int ps2PadInit(void)
{
	int ret;

	if(ps2PadInitConf()==1)
	{
		LOG("ORBISPAD already initialized!");
		return orbispad_initialized;
	}

	ret = SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	if (ret < 0)
	{
		LOG("SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed: %d", ret);
		return -1;
	}
	getGameController();

	orbispad_initialized=1;
	g_time = timeInMilliseconds();
	LOG("ORBISPAD initialized: 0x%X", ret);

	return orbispad_initialized;
}
