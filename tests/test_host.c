/* test_host.c -- the shared host (common/host.c): Street Rod 1's audio pinned bit for bit.
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

int main(int argc, char **argv)
{
    static const struct { const char *name; void (*fn)(void); } tests[] = {
        { "sr1_audio_unchanged", sr1_audio_unchanged },
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
