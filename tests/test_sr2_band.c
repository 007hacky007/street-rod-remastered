/* test_sr2_band.c -- the band plays Street Rod 2's music (src2/sound/band.c): the timbre map, the title song
 * through the AdLib sequencer, stopping, one note per voice, and the music after a police siren.
 *   test_sr2_band <SR2 game dir>
 * Runs headless with the SDL dummy drivers; the band is rendered here (band_enable(false)). */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "host.h"
#include "mem.h"
#include "sound/band.h"
#include "sound/snd_int.h"
#include "sound/sound.h"
#include "arrange.h"
#include "synth.h"

static int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
            failures++;                                                                \
        }                                                                              \
    } while (0)
#define RUN(t) do { int b = failures; t(); printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)

static void wait_ms(int ms) { host_busy_wait_us((u32)ms * 1000); }

/* RMS of `ms` of the band's output. */
static double band_rms(int ms)
{
    static float lr[2 * 4096];
    int frames = host_audio_rate() * ms / 1000;
    double sum = 0;
    long n = 0;
    while (frames > 0) {
        int k = frames < 4096 ? frames : 4096;
        synth_render(lr, k);
        for (int i = 0; i < 2 * k; i++) sum += (double)lr[i] * lr[i];
        n += 2 * k;
        frames -= k;
    }
    return n ? sqrt(sum / (double)n) : 0;
}

static bool is_note(BandPart p, int inst) { return p.kind == BAND_NOTE && p.inst == inst; }
static bool is_drum(BandPart p, int drum) { return p.kind == BAND_DRUM && p.drum == drum; }

static void sr2_band_parts(void)
{
    const char *bass[] = { "bass1", "bass2" }, *guitar[] = { "jazzguit", "guitar1", "elguit1", "fuzguit2" },
               *piano[] = { "piano1", "elpiano1" }, *vibes[] = { "moogsynt", "syn3" },
               *tom[] = { "tom1", "logdrum1", "logdrum2" }, *snare[] = { "snare1", "crash", "cymbal1" },
               *brush[] = { "hihat1", "hihat2" }, *none[] = { "police", "motor", "turn1", "" };
    for (size_t i = 0; i < SDL_arraysize(bass); i++) CHECK(is_note(band_part_of(bass[i]), INST_BASS));
    for (size_t i = 0; i < SDL_arraysize(guitar); i++) CHECK(is_note(band_part_of(guitar[i]), INST_GUITAR));
    for (size_t i = 0; i < SDL_arraysize(piano); i++) CHECK(is_note(band_part_of(piano[i]), INST_EPIANO));
    for (size_t i = 0; i < SDL_arraysize(vibes); i++) CHECK(is_note(band_part_of(vibes[i]), INST_VIBES));
    CHECK(is_drum(band_part_of("bdrum1"), DRUM_KICK));
    for (size_t i = 0; i < SDL_arraysize(tom); i++) CHECK(is_drum(band_part_of(tom[i]), DRUM_TOM));
    for (size_t i = 0; i < SDL_arraysize(snare); i++) CHECK(is_drum(band_part_of(snare[i]), DRUM_SNARE));
    for (size_t i = 0; i < SDL_arraysize(brush); i++) CHECK(is_drum(band_part_of(brush[i]), DRUM_BRUSH));
    for (size_t i = 0; i < SDL_arraysize(none); i++) CHECK(band_part_of(none[i]).kind == BAND_NONE);
    CHECK(is_drum(band_perc_part(6), DRUM_KICK));
    CHECK(is_drum(band_perc_part(7), DRUM_SNARE));
    CHECK(is_drum(band_perc_part(8), DRUM_TOM));
    CHECK(is_drum(band_perc_part(9), DRUM_SNARE));
    CHECK(is_drum(band_perc_part(10), DRUM_BRUSH));
}

static void sr2_band_plays_title(void)
{
    snd_bank_and_intro_load();                      /* MSR.SND and the title song, MSRBEG */
    adlib_music_init();
    mus_play(1);
    wait_ms(3000);
    printf("  notes %d\n", band_notes_started());
    CHECK(band_notes_started() > 20);
    double r = band_rms(500);
    printf("  rms %.4f\n", r);
    CHECK(r > 0.01);
}

static void sr2_band_stop_silences(void)
{
    mus_stop();                                     /* music off, a race starts */
    band_rms(200);                                  /* silenced: only the reverb's tail; ringing notes stay louder */
    double r = band_rms(250);
    printf("  rms after stop %.5f\n", r);
    CHECK(r < 0.001);
}

static void sr2_band_retrigger(void)
{
    band_adlib_program(0, 22);                      /* bass2: a string that rings for seconds */
    band_adlib_note_on(0, 40, 95);
    band_adlib_note_on(0, 40, 95);                  /* the chip replaces the note: so must the band */
    band_adlib_note_off(0);
    band_rms(300);                                  /* the released note has faded; a stacked one rings on */
    double r = band_rms(250);
    printf("  rms after note off %.5f\n", r);
    CHECK(r < 0.005);
}

static void sr2_band_siren(void)
{
    mus_play(1);
    wait_ms(2000);
    mus_stop();                                     /* the race: the music stops, the siren takes voice 2 */
    adlib_play_timbre(2, 0x7F, "police");
    adlib_note_on(2, 0x50);
    wait_ms(500);
    adlib_voice_off(2);
    int before = band_notes_started();
    mus_resume();                                   /* after the race: the same song carries on */
    wait_ms(4000);
    printf("  notes before %d after %d\n", before, band_notes_started());
    CHECK(band_notes_started() > before + 10);
    CHECK(host_opl_voice_is_music(2));
}

/* The PC speaker's background tune, in Street Rod 1's (duration, divisor) format: 14 sections of notes. */
static void sr2_band_speaker_tune(void)
{
    static Song song;
    bool ok = song_parse(DGROUP, DS_spk_bg_song, &song);
    int with_notes = 0;                             /* the marker before the end opens an empty last one */
    for (int sec = 0; sec < song.nsec; sec++)
        for (int i = 0; i < song.n; i++)
            if (song.ev[i].section == sec) { with_notes++; break; }
    printf("  events %d, sections %d (%d with notes)\n", song.n, song.nsec, with_notes);
    CHECK(ok);
    CHECK(with_notes == 14);
    CHECK(song.n > 0);
}

/* A speaker tune event to play by hand: the first note of the tune. */
static const SongEvent *first_note(void)
{
    static Song song;
    if (!song_parse(DGROUP, DS_spk_bg_song, &song)) return NULL;
    for (int i = 0; i < song.n; i++)
        if (song.ev[i].note) return &song.ev[i];
    return NULL;
}

/* Ctrl: the sound off. The speaker sequencer keeps running; the band stays silent. */
static void sr2_band_speaker_sound_off(void)
{
    const SongEvent *e = first_note();
    CHECK(e != NULL);
    if (!e) return;
    band_adlib_all_off();
    band_rms(500);
    DSB(DS_sound_on) = 0;
    band_spk_note(e->off, e->note);
    double off = band_rms(300);
    DSB(DS_sound_on) = 1;
    band_spk_note(e->off, e->note);
    double on = band_rms(300);
    printf("  rms sound off %.5f, on %.5f\n", off, on);
    CHECK(off < 0.0005);
    CHECK(on > 0.005);
}

/* A drive starts: the race ISR takes the timer, the speaker's tune stops at once, and so does the band. */
static void sr2_band_speaker_race_enter(void)
{
    const SongEvent *e = first_note();
    if (!e) { CHECK(e != NULL); return; }
    band_adlib_all_off();
    band_rms(500);
    DSW(DS_snd_device) = 1;
    band_spk_note(e->off, e->note);
    snd_race_enter();
    band_rms(400);                                  /* the reverb's tail of a note that had just started */
    double r = band_rms(250);
    snd_race_leave();
    printf("  rms after the drive starts %.5f\n", r);
    CHECK(r < 0.001);
}

/* In percussion mode a drum voice plays the drum its timbre is (MSR2 puts tom1 on voice 6). */
static void sr2_band_perc_timbre(void)
{
    band_adlib_perc(true);
    band_adlib_program(6, 2);                       /* tom1 */
    band_adlib_note_on(6, 48, 95);
    CHECK(band_last_drum() == DRUM_TOM);
    band_adlib_program(6, 3);                       /* bdrum1 */
    band_adlib_note_on(6, 48, 95);
    CHECK(band_last_drum() == DRUM_KICK);
    band_adlib_program(10, 9);                      /* turn1, an effect's timbre: the voice's own drum */
    band_adlib_note_on(10, 24, 95);
    CHECK(band_last_drum() == DRUM_BRUSH);
    band_adlib_all_off();
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <SR2 game dir>\n", argv[0]); return 2; }
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    char exe[1024], err[256];
    SDL_snprintf(exe, sizeof exe, "%s/%s", argv[1], SR_EXE_NAME);
    if (!mem_load_exe(exe, err, sizeof err)) { printf("FAIL %s\n", err); return 1; }
    host_configure(&(HostConfig){ "test", 49716, true });
    if (!host_init(argv[1], 1, false)) { printf("FAIL host_init\n"); return 1; }
    host_set_realtime(false);
    if (!band_enable(false)) { printf("FAIL band_enable\n"); return 1; }
    RUN(sr2_band_speaker_tune);
    RUN(sr2_band_parts);
    RUN(sr2_band_plays_title);
    RUN(sr2_band_stop_silences);
    RUN(sr2_band_retrigger);
    RUN(sr2_band_siren);
    RUN(sr2_band_speaker_sound_off);
    RUN(sr2_band_speaker_race_enter);
    RUN(sr2_band_perc_timbre);
    host_shutdown();
    return failures ? 1 : 0;
}
