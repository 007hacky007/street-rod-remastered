/* arrange.h -- the remastered music's score: the background tune read from memory as it is (song_parse)
 * and an arrangement of it for a small band (arrange): the notes split into a bass and an upper voice
 * again (the PC speaker interleaves them), how long each note rings, and the drum hits that follow the
 * original's own onsets. Pure functions; music_rm.c plays it. */
#ifndef SND_ARRANGE_H
#define SND_ARRANGE_H

#include "types.h"

#define SONG_MAX_EVENTS 2048
#define SONG_MAX_SECTIONS 64

typedef struct {
    u16 off;                /* offset of the event's duration word in the song segment */
    u16 dur;                /* ticks */
    u16 note;               /* PIT divisor, 0 = rest */
    u8 section;
} SongEvent;

typedef struct {
    int n, nsec;
    SongEvent ev[SONG_MAX_EVENTS];
} Song;

/* The events from seg:base + 4 (the sequencer's layout, sound.c music_tick): (dur, note) pairs, a 0
 * duration followed by 0 = end, by anything else = the next section. false: no end found. */
bool song_parse(u16 seg, u16 base, Song *out);
/* Index of the event whose duration word is at off, -1 if none. */
int song_find(const Song *s, u16 off);

enum { ARR_BASS, ARR_UPPER };
enum { ARR_KICK = 1, ARR_SNARE = 2, ARR_BRUSH = 4 };
enum { INST_BASS, INST_GUITAR, INST_EPIANO, INST_VIBES, INST_COUNT };

typedef struct {
    float midi;             /* pitch (MIDI note, fractional) */
    u8 voice;               /* ARR_BASS / ARR_UPPER */
    u8 drums;               /* ARR_* hits at the event's start */
    u16 hold;               /* ticks the note rings (0 for rests) */
    float vel;              /* 0..1 */
} ArrNote;

typedef struct {
    ArrNote note[SONG_MAX_EVENTS];
    u8 sec_inst[SONG_MAX_SECTIONS];    /* instrument of the upper voice per section */
} Arrangement;

void arrange(const Song *s, Arrangement *a);
/* MIDI pitch of a PIT divisor. */
float arr_midi(u16 divisor);

#endif
