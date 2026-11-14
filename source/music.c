/*
 * Background music: the AHX song is decoded on the EE (ahx_player.c, from
 * ps2sdk's IOP AHX player) and played through SDL audio, i.e. audsrv on the
 * IOP. Running the player on the IOP (ahx.irx) costs ~600 KB of its 2 MB
 * RAM; this way the IOP only carries libsd + audsrv (~110 KB), and decoding
 * takes ~2% of the EE.
 *
 * SDL's PS2 audio thread runs one priority step above the thread that opened
 * the device, so it keeps the music going while the main loop is busy.
 */

#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "ahx_player.h"
#include "music.h"
#include "types.h"

// The player's output: mono, signed 16-bit, 48 kHz, 2 frames of 1/50 s per chunk
#define AHX_RATE		48000
#define AHX_CHUNK		(AHX_RATE / 50 * 2)

static short chunk[AHX_CHUNK];
static int chunk_pos = AHX_CHUNK;	// next sample to hand out; == AHX_CHUNK: empty
static void* song_data = NULL;
static int audio_open = 0;
static int out_channels = 1;
static volatile unsigned int fill_calls = 0;
static volatile int fill_peak = 0;

// Runs on SDL's audio thread
static void music_fill(void* userdata, Uint8* stream, int len)
{
	short* out = (short*) stream;
	int frames = len / (sizeof(short) * out_channels);

	(void) userdata;
	fill_calls++;

	while (frames > 0)
	{
		int n;

		if (chunk_pos >= AHX_CHUNK)
		{
			AHXOutput_MixBuffer(chunk);
			chunk_pos = 0;
		}

		n = AHX_CHUNK - chunk_pos;
		if (n > frames)
			n = frames;

		// the player is mono: copy each sample to every output channel
		for (int i = 0; i < n; i++)
		{
			short s = chunk[chunk_pos + i];

			for (int ch = 0; ch < out_channels; ch++)
				*out++ = s;

			if (abs(s) > fill_peak)
				fill_peak = abs(s);
		}
		chunk_pos += n;
		frames -= n;
	}
}

int music_init(const void* song, int size)
{
	SDL_AudioSpec want, have;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		LOG("SDL audio init failed: %s", SDL_GetError());
		return 0;
	}

	// the player parses the module in place and keeps pointers into it
	if ((song_data = malloc(size)) == NULL)
		return 0;

	memcpy(song_data, song, size);

	// AHXPlayer_LoadSongBuffer() returns the subsong count, which is 0 both for
	// an error and for a valid song without extra subsongs (like Apollo's), so
	// check the header the way it does instead of trusting the result
	const unsigned char* hdr = song_data;
	if (size < 14 || hdr[0] != 'T' || hdr[1] != 'H' || hdr[2] != 'X' || hdr[3] > 1)
	{
		LOG("Invalid AHX song");
		return 0;
	}

	AHXPlayer_Init();
	AHXPlayer_LoadSongBuffer(song_data, size);
	AHXPlayer_InitSubsong(0);

	memset(&want, 0, sizeof(want));
	want.freq = AHX_RATE;
	want.format = AUDIO_S16;
	want.channels = 1;
	want.samples = 512;
	want.callback = music_fill;

	if (SDL_OpenAudio(&want, &have) != 0)
	{
		LOG("SDL_OpenAudio failed: %s", SDL_GetError());
		return 0;
	}

	// SDL hands over its own format (the PS2 backend opens stereo): no
	// conversion happens, so the callback has to write what it was given
	if (have.freq != AHX_RATE || have.format != AUDIO_S16 || have.channels < 1 || have.channels > 2)
	{
		LOG("Unsupported audio format %d Hz, %d ch, fmt %X", have.freq, have.channels, have.format);
		SDL_CloseAudio();
		return 0;
	}
	out_channels = have.channels;

	audio_open = 1;
	LOG("Music ready (%d Hz, %d samples per buffer)", have.freq, have.samples);
	return 1;
}

void music_play(void)
{
	if (audio_open)
		SDL_PauseAudio(0);
}

void music_pause(void)
{
	if (audio_open)
		SDL_PauseAudio(1);
}

void music_end(void)
{
	if (audio_open)
		SDL_CloseAudio();

	audio_open = 0;
}

void music_get_stats(unsigned int* calls, int* peak)
{
	*calls = fill_calls;
	*peak = fill_peak;
}
