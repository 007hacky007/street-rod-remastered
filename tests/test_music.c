/* test_music.c -- the remastered music: the tune read from memory (song_parse), its arrangement (voices,
 * holds, drums) and the synthesiser. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "sound/arrange.h"
#include "sound/synth.h"
#include "sound/music_rm.h"
#include "sound/snd_int.h"
#include "sound/sound.h"
#include "symbols.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define SEGM 0x6000
static u16 wpos;
static void w(u16 v) { wr16(SEGM, wpos, v); wpos += 2; }
static void ev(u16 dur, u16 note) { w(dur); w(note); }

/* PIT divisor of MIDI note m. */
static u16 div_of(double m) { return (u16)lround(1193182.0 / (440.0 * pow(2.0, (m - 69) / 12))); }

static Song song;
static Arrangement arr;

static void parse_sections_and_end(void)
{
    wpos = 4;
    ev(5, div_of(60));
    ev(10, 0);                       /* a rest */
    w(0); w(1);                      /* section marker */
    ev(7, div_of(72));
    w(0); w(0);                      /* end */
    CHECK(song_parse(SEGM, 0, &song));
    CHECK(song.n == 3);
    CHECK(song.nsec == 2);
    CHECK(song.ev[0].off == 4 && song.ev[0].dur == 5 && song.ev[0].section == 0);
    CHECK(song.ev[1].note == 0);
    CHECK(song.ev[2].off == 16 && song.ev[2].section == 1);    /* the marker's 4 bytes at 12 */
    CHECK(song_find(&song, 16) == 2);
    CHECK(song_find(&song, 6) == -1);
}

/* Bass F2 between chord notes F4 / A4 / C5, as the boogie tune interleaves them. */
static void build_boogie(void)
{
    wpos = 4;
    for (int i = 0; i < 8; i++) {
        ev(5, div_of(41));           /* F2 */
        ev(11, div_of(i & 1 ? 69 : 65));
        ev(5, div_of(41));
        ev(11, div_of(72));
    }
    w(0); w(0);
    song_parse(SEGM, 0, &song);
    arrange(&song, &arr);
}

static void split_bass_from_upper(void)
{
    build_boogie();
    CHECK(arr.note[0].voice == ARR_BASS);
    CHECK(arr.note[1].voice == ARR_UPPER);
    CHECK(arr.note[3].voice == ARR_UPPER);
    CHECK(fabs(arr.note[1].midi - 65) < 0.05);
}

static void bass_holds_until_next_bass(void)
{
    build_boogie();
    CHECK(arr.note[0].hold == 16);                      /* the next F2 starts 5 + 11 ticks later */
}

static void upper_notes_overlap(void)
{
    build_boogie();
    CHECK(arr.note[1].hold > 11);                       /* rings into the next upper note: a chord */
}

static void kick_and_snare_alternate_on_bass(void)
{
    build_boogie();
    int kicks = 0, snares = 0, last = 0;
    for (int i = 0; i < song.n; i++) {
        u8 d = arr.note[i].drums;
        if (d & ARR_KICK) { kicks++; CHECK(last != ARR_KICK); last = ARR_KICK; }
        if (d & ARR_SNARE) { snares++; CHECK(last != ARR_SNARE); last = ARR_SNARE; }
        if (arr.note[i].voice == ARR_UPPER) CHECK(!(d & (ARR_KICK | ARR_SNARE)));
    }
    CHECK(kicks >= 6 && snares >= 6);
}

/* A high single line (the jingle): one upper voice, no kick or snare. */
static void high_line_is_one_voice(void)
{
    wpos = 4;
    double m[] = { 84, 81, 79, 77, 76, 72 };
    for (int i = 0; i < 6; i++) ev(5, div_of(m[i]));
    w(0); w(0);
    song_parse(SEGM, 0, &song);
    arrange(&song, &arr);
    for (int i = 0; i < 6; i++) {
        CHECK(arr.note[i].voice == ARR_UPPER);
        CHECK(!(arr.note[i].drums & (ARR_KICK | ARR_SNARE)));
    }
    CHECK(arr.sec_inst[0] == INST_VIBES);
}

/* A low single line (the bass riff) is the bass. */
static void low_line_is_bass(void)
{
    wpos = 4;
    double m[] = { 36, 40, 43, 40, 36, 40, 43, 45 };
    for (int i = 0; i < 8; i++) ev(6, div_of(m[i]));
    w(0); w(0);
    song_parse(SEGM, 0, &song);
    arrange(&song, &arr);
    for (int i = 0; i < 8; i++) CHECK(arr.note[i].voice == ARR_BASS);
}

/* ---- synthesiser */

#define RATE 44100
static float buf[RATE * 2 * 2];

/* Frequency of the left channel over n frames from the strongest autocorrelation lag. */
static double pitch_of(const float *lr, int n)
{
    static double c[2000];
    double bv = 0;
    for (int lag = 20; lag < 2000; lag++) {
        double s = 0;
        for (int i = 0; i + lag < n; i++) s += lr[2 * i] * lr[2 * (i + lag)];
        c[lag] = s / (n - lag);
        if (c[lag] > bv) bv = c[lag];
    }
    for (int lag = 21; lag < 1999; lag++)          /* the shortest lag near the best: no octave errors */
        if (c[lag] >= 0.9 * bv && c[lag] >= c[lag - 1] && c[lag] >= c[lag + 1]) return (double)RATE / lag;
    return 0;
}

static double peak(const float *lr, int frames)
{
    double p = 0;
    for (int i = 0; i < 2 * frames; i++) p = fmax(p, fabs(lr[i]));
    return p;
}

static void pluck_has_its_pitch(void)
{
    synth_init(RATE);
    int v = synth_note(INST_BASS, 45, 1.0f, 0);         /* A2 = 110 Hz */
    CHECK(v >= 0);
    synth_render(buf, RATE / 2);
    double f = pitch_of(buf + 2 * 2000, 8000);
    CHECK(fabs(f - 110) < 2);
    CHECK(peak(buf, RATE / 2) > 0.05);
}

static void guitar_and_piano_have_their_pitch(void)
{
    int inst[] = { INST_GUITAR, INST_EPIANO, INST_VIBES };
    for (int k = 0; k < 3; k++) {
        synth_init(RATE);
        synth_note(inst[k], 69, 1.0f, 0);               /* A4 = 440 Hz */
        synth_render(buf, RATE / 4);
        double f = pitch_of(buf + 2 * 1000, 4000);
        if (fabs(f - 440) >= 6) printf("  inst %d: %.1f Hz\n", inst[k], f);
        CHECK(fabs(f - 440) < 6);
    }
}

static void bend_moves_the_pitch(void)
{
    synth_init(RATE);
    int v = synth_note(INST_GUITAR, 69, 1.0f, 0);
    synth_render(buf, 2000);
    synth_bend(v, 71);                                  /* B4 = 493.9 Hz */
    synth_render(buf, RATE / 4);
    double f = pitch_of(buf + 2 * 1000, 4000);
    if (fabs(f - 493.9) >= 8) printf("  bent: %.1f Hz\n", f);
    CHECK(fabs(f - 493.9) < 8);
}

static void all_off_goes_quiet(void)
{
    synth_init(RATE);
    for (int i = 0; i < 6; i++) synth_note(INST_EPIANO, 60 + 2 * i, 1.0f, 0);
    synth_drum(DRUM_KICK, 1.0f);
    synth_render(buf, 4000);
    synth_all_off();
    synth_render(buf, RATE);                            /* reverb tail and release */
    synth_render(buf, RATE / 2);
    CHECK(peak(buf, RATE / 2) < 0.002);
}

static void output_stays_bounded(void)
{
    synth_init(RATE);
    for (int i = 0; i < 24; i++) synth_note(i % 2 ? INST_GUITAR : INST_BASS, 30 + 3 * i, 1.0f, (i % 3) - 1.0f);
    for (int i = 0; i < 3; i++) synth_drum(i, 1.0f);
    synth_render(buf, RATE);
    bool finite = true;
    for (int i = 0; i < 2 * RATE; i++) finite &= isfinite(buf[i]);
    CHECK(finite);
    CHECK(peak(buf, RATE) <= 1.0);
}

static void drums_sound(void)
{
    for (int k = 0; k < 3; k++) {
        synth_init(RATE);
        synth_drum(k, 1.0f);
        synth_render(buf, RATE / 10);
        CHECK(peak(buf, RATE / 10) > 0.02);
    }
}

/* ---- the band driven by the game's own sequencer */

/* A boogie at the background tune's place, started as the game starts it; the band plays while the
 * sequencer does and goes quiet when the music is switched off. */
static void band_follows_the_sequencer(void)
{
    u16 seg = SEG(SEG_SONGS_BG);
    for (int i = 0; i < 16; i++) {
        wr16(seg, (u16)(4 + 4 * i), 6);
        wr16(seg, (u16)(6 + 4 * i), div_of(i & 1 ? 69 : 41));
    }
    wr16(seg, 4 + 4 * 16, 0); wr16(seg, 6 + 4 * 16, 0);
    snd_song_defaults();
    DSB(DS_sound_on) = 1;
    DSW(DS_music_enabled) = 1;
    DSW(DS_music_cur) = DS_bg_song;
    CHECK(music_rm_enable(false));
    synth_init(RATE);
    tune_start(2, 0);
    double loud = 0;
    for (int t = 0; t < 40; t++) {                       /* 40 ticks */
        snd_music_tick();
        synth_render(buf, RATE / 73);
        loud = fmax(loud, peak(buf, RATE / 73));
    }
    CHECK(loud > 0.05);
    if (music_rm_last_event() < 4) printf("  last event %d\n", music_rm_last_event());
    CHECK(music_rm_last_event() >= 4);
    music_toggle();                                      /* "Squelch it!" */
    for (int t = 0; t < 2 * 73; t++) { snd_music_tick(); synth_render(buf, RATE / 73); }
    CHECK(peak(buf, RATE / 73) < 0.002);
    music_rm_disable();
}

int main(void)
{
#define RUN(t) do { memset(mem, 0, MEM_SIZE); int b = failures; t(); printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)
    RUN(parse_sections_and_end);
    RUN(split_bass_from_upper);
    RUN(bass_holds_until_next_bass);
    RUN(upper_notes_overlap);
    RUN(kick_and_snare_alternate_on_bass);
    RUN(high_line_is_one_voice);
    RUN(low_line_is_bass);
    RUN(pluck_has_its_pitch);
    RUN(guitar_and_piano_have_their_pitch);
    RUN(bend_moves_the_pitch);
    RUN(all_off_goes_quiet);
    RUN(output_stays_bounded);
    RUN(drums_sound);
    RUN(band_follows_the_sequencer);
    return failures != 0;
}
