/* test_host.c -- the shared host (common/host.c): Street Rod 1's audio pinned bit for bit, Street Rod 2's
 * AdLib chip and voice volumes, the mouse space.
 *   test_host <test name>   (each test in its own process: host_init runs once per process)
 * Runs headless with the SDL dummy drivers. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"

static int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
            failures++;                                                                \
        }                                                                              \
    } while (0)

/* What the dump of Street Rod 1's speaker script and band callback below must be. Recorded from the
 * host before it was shared with Street Rod 2. */
#define SR1_AUDIO_FNV 0xe3f3a529ef9c4501ull
#define SR1_AUDIO_LEN 132340u

static const char *test_name;
static const char *wav_path(void)
{
    static char path[1024];
    if (!path[0]) SDL_snprintf(path, sizeof path, "%stest_host_%s.wav", SDL_GetBasePath(), test_name);
    return path;
}

static unsigned char *read_file(const char *path, size_t *len)
{
    return SDL_LoadFile(path, len);
}

static unsigned long long fnv1a(const unsigned char *p, size_t n)
{
    unsigned long long h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; }
    return h;
}

static unsigned le(const unsigned char *p, int n)
{
    unsigned v = 0;
    for (int i = n - 1; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

/* A fixed, deterministic band signal: a slow ramp on the left, its negative on the right. */
static int synth_t;
static void synth(float *lr, int frames)
{
    for (int i = 0; i < frames; i++, synth_t++) {
        float v = (float)(synth_t % 441) / 441.0f - 0.5f;
        lr[2 * i] = v * 0.5f;
        lr[2 * i + 1] = -v * 0.25f;
    }
}

static bool start(void)
{
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    if (!host_init(".", 1, false)) { printf("  FAIL host_init\n"); return false; }
    host_set_realtime(false);
    if (!host_audio_dump_begin(wav_path())) { printf("  FAIL dump\n"); return false; }
    return true;
}

static unsigned char *finish(size_t *len)
{
    host_audio_dump_end();
    unsigned char *wav = read_file(wav_path(), len);
    SDL_RemovePath(wav_path());
    return wav;
}

static void sr1_audio_unchanged(void)
{
    if (!start()) { failures++; return; }
    host_speaker(1193, true);
    host_busy_wait_us(200000);
    host_speaker(2386, true);
    host_busy_wait_us(150000);
    host_speaker(0, false);
    host_busy_wait_us(50000);
    host_set_volumes(40, 100);
    host_speaker_source(true);
    host_speaker(1500, true);
    host_busy_wait_us(100000);
    host_speaker_source(false);
    host_busy_wait_us(100000);
    host_set_music_synth(synth);
    host_busy_wait_us(100000);
    host_set_music_synth(NULL);
    host_speaker(0, false);
    host_busy_wait_us(50000);
    size_t len = 0;
    unsigned char *wav = finish(&len);
    host_shutdown();
    CHECK(wav && len > 44);
    if (!wav) return;
    CHECK(le(wav + 22, 2) == 2);                 /* stereo */
    CHECK(le(wav + 24, 4) == 44100);
    unsigned long long h = fnv1a(wav, len);
    if (h != SR1_AUDIO_FNV || len != SR1_AUDIO_LEN)
        printf("  got SR1_AUDIO_FNV 0x%016llxull, SR1_AUDIO_LEN %uu\n", h, (unsigned)len);
    CHECK(h == SR1_AUDIO_FNV);
    CHECK(len == SR1_AUDIO_LEN);
    SDL_free(wav);
}


/* ---- Street Rod 2's AdLib chip and voice volumes */

static void opl(u8 reg, u8 value) { host_opl_write(reg, value); }

/* A plain sine note on melodic voice v (operators from the OPL2 slot layout). */
static void opl_note(int v)
{
    static const u8 mod_off[9] = { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 };
    u8 m = mod_off[v], c = (u8)(m + 3);
    opl((u8)(0x20 + m), 0x01); opl((u8)(0x20 + c), 0x01);
    opl((u8)(0x40 + m), 0x10); opl((u8)(0x40 + c), 0x00);
    opl((u8)(0x60 + m), 0xF0); opl((u8)(0x60 + c), 0xF0);
    opl((u8)(0x80 + m), 0x77); opl((u8)(0x80 + c), 0x77);
    opl((u8)(0xE0 + m), 0x00); opl((u8)(0xE0 + c), 0x00);
    opl((u8)(0xC0 + v), 0x00);
    opl((u8)(0xA0 + v), 0x98);
    opl((u8)(0xB0 + v), 0x31);
}

static bool start_sr2(void)
{
    host_configure(&(HostConfig){ "test", 49716, true });
    if (!start()) return false;
    host_opl_enable(true);
    return true;
}

/* RMS of the second half of a stereo s16 WAV (the output lags the timeline by a tick); *same = every
 * frame has left == right. */
static double rms_tail(const unsigned char *wav, size_t len, bool *same)
{
    size_t frames = (len - 44) / 4, from = frames / 2;
    double sum = 0;
    if (same) *same = true;
    for (size_t i = from; i < frames; i++) {
        short l = (short)le(wav + 44 + 4 * i, 2), r = (short)le(wav + 46 + 4 * i, 2);
        if (same && l != r) *same = false;
        sum += (double)l * l;
    }
    return frames > from ? SDL_sqrt(sum / (double)(frames - from)) : 0;
}

static void opl_renders(void)
{
    if (!start_sr2()) { failures++; return; }
    opl_note(0);
    host_busy_wait_us(100000);
    size_t len = 0;
    unsigned char *wav = finish(&len);
    host_shutdown();
    CHECK(wav && len > 44);
    if (!wav) return;
    CHECK(le(wav + 22, 2) == 2);
    CHECK(le(wav + 24, 4) == 49716);
    bool same = false;
    double r = rms_tail(wav, len, &same);
    printf("  rms %.1f\n", r);
    CHECK(r > 500);
    CHECK(same);
    SDL_free(wav);
}

static void opl_voice_gain(void)
{
    if (!start_sr2()) { failures++; return; }
    host_set_volumes(0, 100);
    host_opl_voice_music(0, true);
    host_opl_voice_music(3, false);
    opl_note(0);
    opl_note(3);
    host_busy_wait_us(100000);
    size_t len = 0;
    unsigned char *wav = finish(&len);
    double effect = wav ? rms_tail(wav, len, NULL) : 0;
    SDL_free(wav);
    host_audio_dump_begin(wav_path());
    host_opl_voice_music(3, true);              /* now both are music, at volume 0 */
    host_busy_wait_us(100000);
    wav = finish(&len);
    double music = wav ? rms_tail(wav, len, NULL) : 1e9;
    SDL_free(wav);
    host_shutdown();
    printf("  effect rms %.1f, music rms %.1f\n", effect, music);
    CHECK(effect > 500);
    CHECK(music < 5);
}

/* While a band plays the music (host_set_music_synth), the chip's music voices are silent and its effect
 * voices are not. */
static void silent_band(float *lr, int frames) { SDL_memset(lr, 0, sizeof(float) * 2 * (size_t)frames); }

static double opl_segment(void)
{
    host_audio_dump_begin(wav_path());
    host_busy_wait_us(100000);
    size_t len = 0;
    unsigned char *wav = finish(&len);
    double r = wav ? rms_tail(wav, len, NULL) : -1;
    SDL_free(wav);
    return r;
}

static void opl_music_muted_by_synth(void)
{
    if (!start_sr2()) { failures++; return; }
    host_audio_dump_end();
    host_set_volumes(100, 100);
    host_opl_voice_music(0, true);
    host_opl_voice_music(3, false);
    host_set_music_synth(silent_band);
    opl_note(0);
    double music = opl_segment();                   /* the music voice under the band */
    opl_note(3);
    double effect = opl_segment();                  /* and an effect beside it */
    opl(0xB3, 0x11);                                /* the effect ends */
    host_busy_wait_us(300000);
    host_set_music_synth(NULL);
    double back = opl_segment();                    /* the band gone: the chip's music again */
    host_shutdown();
    printf("  music %.1f, effect %.1f, back %.1f\n", music, effect, back);
    CHECK(music >= 0 && music < 1);
    CHECK(effect > 100);
    CHECK(back > 100);
}

/* A key-off and a key-on written at the same moment restart the note, as on the PC, where the driver's own
 * waits between writes let the chip see the key-off. (Street Rod 2's songs end and start each note so.) */
static void opl_key_retrigger(void)
{
    if (!start_sr2()) { failures++; return; }
    host_audio_dump_end();
    host_set_volumes(100, 100);
    /* voice 0: a note that dies away within a second (attack 15, decay 4, not sustained) */
    opl(0x20, 0x01); opl(0x23, 0x01);
    opl(0x40, 0x10); opl(0x43, 0x00);
    opl(0x60, 0xF4); opl(0x63, 0xF4);
    opl(0x80, 0x77); opl(0x83, 0x77);
    opl(0xC0, 0x00);
    opl(0xA0, 0x98);
    opl(0xB0, 0x31);
    host_busy_wait_us(2000000);
    double faded = opl_segment();
    opl(0xB0, 0x11);                                /* key off and on again, at once */
    opl(0xB0, 0x31);
    double again = opl_segment();
    host_shutdown();
    printf("  faded %.1f, restarted %.1f\n", faded, again);
    CHECK(faded >= 0 && faded < 50);
    CHECK(again > 500);
}

/* ---- The mouse space */

static bool compose(u32 *xrgb) { (void)xrgb; return false; }

static void mouse_space(void)
{
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    if (!host_init(".", 1, false)) { failures++; return; }
    host_set_frame_source(compose, 640, 400);   /* SR1's enhanced view at scale 2 */
    s16 x = -1, y = -1;
    host_window_to_screen(0, 0, &x, &y);
    CHECK(x == 0 && y == 0);
    host_window_to_screen(1e6f, 1e6f, &x, &y);
    CHECK(x == 319 && y == 199);
    host_shutdown();
}

static void mouse_space_set(void)
{
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    if (!host_init(".", 1, false)) { failures++; return; }
    host_set_frame_source(compose, 640, 200);   /* SR2's text menus */
    host_set_mouse_space(640, 200);
    s16 x = -1, y = -1;
    host_window_to_screen(1e6f, 1e6f, &x, &y);
    CHECK(x == 639 && y == 199);
    host_shutdown();
}

/* --fps: auto, 0..1000 (clamped), anything else refused (HOST_FPS_BAD). */
static void fps_parse(void)
{
    CHECK(host_parse_fps("auto") == HOST_FPS_AUTO);
    CHECK(host_parse_fps("0") == 0);
    CHECK(host_parse_fps("144") == 144);
    CHECK(host_parse_fps("5000") == 1000);
    CHECK(host_parse_fps("-3") == 0);
    CHECK(host_parse_fps("60x") == HOST_FPS_BAD);
    CHECK(host_parse_fps("") == HOST_FPS_BAD);
    CHECK(host_parse_fps("fast") == HOST_FPS_BAD);
}

/* The present grid: 0 = every changed frame (VSync waits, or uncapped). With VSync a cap at or above the
 * refresh rate is left to the VSync; automatic without VSync is the refresh rate, unknown: 60 when VSync was
 * asked for and refused, else 165. */
static void fps_interval(void)
{
    const uint64_t s = 1000000000ull;
    CHECK(host_frame_interval_ns(HOST_FPS_AUTO, 120, true, true) == 0);
    CHECK(host_frame_interval_ns(60, 120, true, true) == s / 60);
    CHECK(host_frame_interval_ns(144, 144, true, true) == 0);
    CHECK(host_frame_interval_ns(240, 144, true, true) == 0);
    CHECK(host_frame_interval_ns(HOST_FPS_AUTO, 144, false, false) == s / 144);
    CHECK(host_frame_interval_ns(HOST_FPS_AUTO, 0, false, true) == s / 60);
    CHECK(host_frame_interval_ns(HOST_FPS_AUTO, 0, false, false) == s / 165);
    CHECK(host_frame_interval_ns(0, 144, false, false) == 0);
    CHECK(host_frame_interval_ns(30, 0, false, false) == s / 30);
}

/* Presents due on a fixed grid: the first one starts it, a late frame is followed at once. */
static void fps_next_present(void)
{
    CHECK(host_next_present_ns(0, 1000, 100) == 1100);
    CHECK(host_next_present_ns(1100, 1150, 100) == 1200);
    CHECK(host_next_present_ns(1100, 1500, 100) == 1500);
}

static int presents;
static bool compose_changed(u32 *xrgb) { (void)xrgb; presents++; return true; }

/* host_pump with a frame that always changed, for about 0.5 s: with --fps 40 about 20 presents; uncapped
 * without VSync far more than the old floor of one present per 8 ms (62). */
static int pump_presents(int fps)
{
    presents = 0;
    host_set_fps(fps);
    host_set_vsync(false);
    host_set_frame_source(compose_changed, 320, 200);
    uint64_t t0 = SDL_GetTicksNS();
    while (SDL_GetTicksNS() - t0 < 500000000ull) host_pump();
    return presents;
}

static void fps_pump_cap(void)
{
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    if (!host_init(".", 1, false)) { failures++; return; }
    int capped = pump_presents(40), uncapped = pump_presents(0);
    printf("  presents in 0.5 s: --fps 40 %d, uncapped %d\n", capped, uncapped);
    CHECK(capped >= 16 && capped <= 22);
    CHECK(uncapped > 100);
    host_shutdown();
}

int main(int argc, char **argv)
{
    static const struct { const char *name; void (*fn)(void); } tests[] = {
        { "sr1_audio_unchanged", sr1_audio_unchanged },
        { "opl_renders", opl_renders },
        { "opl_voice_gain", opl_voice_gain },
        { "opl_music_muted_by_synth", opl_music_muted_by_synth },
        { "opl_key_retrigger", opl_key_retrigger },
        { "mouse_space", mouse_space },
        { "mouse_space_set", mouse_space_set },
        { "fps_parse", fps_parse },
        { "fps_interval", fps_interval },
        { "fps_next_present", fps_next_present },
        { "fps_pump_cap", fps_pump_cap },
    };
    if (argc != 2) { fprintf(stderr, "usage: %s <test>\n", argv[0]); return 2; }
    for (size_t i = 0; i < SDL_arraysize(tests); i++)
        if (!strcmp(argv[1], tests[i].name)) {
            test_name = tests[i].name;
            tests[i].fn();
            printf("%s %s\n", failures ? "FAIL" : "ok  ", tests[i].name);
            return failures ? 1 : 0;
        }
    fprintf(stderr, "no test %s\n", argv[1]);
    return 2;
}
