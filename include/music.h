#ifndef MUSIC_H
#define MUSIC_H

/*
 * Background music: an AHX song decoded on the EE (ahx_player.c) and played
 * through SDL audio. Starts paused.
 */

// Load `song` (an AHX module, copied) and open the audio device. Returns 1 on success.
int music_init(const void* song, int size);

void music_play(void);
void music_pause(void);

// Stop playback and close the audio device
void music_end(void);

// Diagnostics: buffers SDL has asked the player for (48000/512 per second
// while playing) and the loudest sample produced so far
void music_get_stats(unsigned int* calls, int* peak);

#endif
