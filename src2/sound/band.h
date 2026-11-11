/* band.h -- ENH: Street Rod 2's remastered music (spec: Remastered music). Street Rod 1's small band
 * (common/band) plays the game's songs as its own sequencers play them: the AdLib songs note for note from
 * mus.c, the PC speaker tune through Street Rod 1's arrangement. The sequencers run exactly as before; while
 * the band plays, the host keeps the chip's music voices and the speaker's music silent
 * (host_set_music_synth) and the effects sound as they did. */
#ifndef SR2_BAND_H
#define SR2_BAND_H

#include "types.h"

enum { BAND_NONE, BAND_NOTE, BAND_DRUM };
typedef struct { int kind; int inst; int drum; } BandPart;   /* inst: INST_* (arrange.h), drum: DRUM_* (synth.h) */

/* What plays a timbre of MSR.SND: an instrument, a drum, or nothing (the sound effects' timbres). */
BandPart band_part_of(const char *timbre);
/* What plays AdLib percussion voice 6..10 (bass drum, snare, tom-tom, cymbal, hi-hat). */
BandPart band_perc_part(int voice);

/* with_host: the host renders the band into the output at the music volume (false in tests, which render
 * it themselves with synth_render). */
bool band_enable(bool with_host);
void band_disable(void);
extern bool band_on;

/* From the AdLib sequencer (mus.c); each returns at once unless band_on. */
void band_adlib_program(int v, int index);          /* the bank's timbre `index` on voice v */
void band_adlib_note_on(int v, int note, int vel);  /* note: MIDI pitch (60 = middle C), vel 1..127 */
void band_adlib_note_off(int v);
void band_adlib_bend(int v, u16 bend);              /* 0..0x3FFF, 0x2000 = none */
void band_adlib_bend_range(int semitones);          /* the song's (header byte 0x3B) */
void band_adlib_perc(bool on);                      /* voices 6..10 are drums */
void band_adlib_all_off(void);                      /* the song stopped or starts over */

/* Every timer tick: all quiet while the sound is off. */
void band_tick(void);

int band_notes_started(void);                       /* tests */

#endif
