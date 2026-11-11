/* synth.h -- the remastered music's synthesiser: plucked strings (Karplus-Strong bass and guitar), FM
 * electric piano and vibraphone, three drums, stereo with a short reverb. Float stereo frames, no
 * allocation after synth_init. */
#ifndef SND_SYNTH_H
#define SND_SYNTH_H

enum { DRUM_KICK, DRUM_SNARE, DRUM_BRUSH };

void synth_init(int rate);
/* A note of instrument inst (INST_*, arrange.h) at MIDI pitch midi, velocity 0..1, pan -1..1. Returns the
 * voice, -1 if none. */
int synth_note(int inst, float midi, float vel, float pan);
void synth_release(int voice);          /* let it fade (dampened) */
void synth_bend(int voice, float midi);
void synth_drum(int kind, float vel);
void synth_all_off(void);               /* everything fades out quickly */
/* frames stereo frames (L, R interleaved) into lr, each sample in -1..1. */
void synth_render(float *lr, int frames);

#endif
