/* music_rm.h -- the remastered music (spec: Remastered music): the background tune played by a small
 * synthesised band from the game's own sequencer. The sequencer runs exactly as before; while this is on,
 * its speaker output is silent and its notes are handed to the band (sound.c music_tick). */
#ifndef SND_MUSIC_RM_H
#define SND_MUSIC_RM_H

#include "types.h"

/* Reads the tune from the program image and arranges it; with_host: the host renders the band into the
 * audio output (false in tests, which render it themselves). false: the tune was not found. */
bool music_rm_enable(bool with_host);
void music_rm_disable(void);
extern bool music_rm_on;

/* From the sequencer, for the background tune only: a note starts (its duration word at song offset off),
 * a rest starts, the vibrato moved the sounding note. */
void music_rm_note(u16 off, u16 divisor);
void music_rm_rest(void);
void music_rm_bend(u16 divisor);
/* Every timer tick (snd_music_tick): notes end when their time is up; all quiet when the tune stops. */
void music_rm_tick(void);
int music_rm_last_event(void);          /* index of the last event played (tests), -1 none */

#endif
