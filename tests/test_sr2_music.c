/* test_sr2_music.c -- Street Rod 2's AdLib voices take the music or the effects volume: after an effect
 * (the police siren on voice 2) the music sequencer, resumed in the same song, takes the voice back. The band
 * plays the AdLib songs as loud as the chip plays them, so the effects keep their balance with the music.
 *   test_sr2_music <SR2 game dir>
 * Runs headless with the SDL dummy drivers, the emulated clock driven by busy waits. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>

#include "host.h"
#include "mem.h"
#include "sound/band.h"
#include "sound/snd_int.h"

static int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
            failures++;                                                                \
        }                                                                              \
    } while (0)

static void wait_ms(int ms) { host_busy_wait_us((u32)ms * 1000); }

/* The mean power of the host's output (left channel) over the first `ms` of each AdLib song (slots 1..4). */
static double songs_power(int ms)
{
    char path[1024];
    SDL_snprintf(path, sizeof path, "%stest_sr2_music.wav", SDL_GetBasePath());
    double sum = 0;
    long n = 0;
    for (int slot = 1; slot <= 4; slot++) {
        if (!host_audio_dump_begin(path)) { printf("  FAIL dump\n"); return 0; }
        mus_play(slot);
        wait_ms(ms);
        mus_stop();
        host_audio_dump_end();
        size_t len;
        Sint16 *w = SDL_LoadFile(path, &len);
        for (size_t i = 22; w && 2 * i + 1 < len; i += 2) { sum += (double)w[i] * w[i]; n++; }
        SDL_free(w);
        wait_ms(500);                               /* the band's tails */
    }
    SDL_RemovePath(path);
    return n ? sum / (double)n : 0;
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

    snd_bank_and_intro_load();                      /* MSR.SND and the title song, MSRBEG */
    adlib_music_init();
    mus_play(1);
    wait_ms(3000);
    CHECK(host_opl_voice_is_music(2));              /* the title song plays on voice 2 */

    mus_stop();                                     /* the race: the music stops, the siren takes voice 2 */
    adlib_play_timbre(2, 0x7F, "police");
    adlib_note_on(2, 0x50);
    wait_ms(500);
    CHECK(!host_opl_voice_is_music(2));
    adlib_voice_off(2);

    mus_resume();                                   /* after the race: the same song carries on */
    wait_ms(8000);
    CHECK(host_opl_voice_is_music(2));

    printf("%s sr2_music_voice_ownership\n", failures ? "FAIL" : "ok  ");

    /* The band in place of the chip's music: as loud (within 1.5 dB) as the chip, which the effects are
     * mixed against. Louder, the quiet effects (the putt, a bolt, the air wrench) went under the music. */
    int before = failures;
    mus_songs_load();
    double chip = songs_power(20000);
    band_enable(true);
    double band = songs_power(20000);
    band_disable();
    double db = 10 * log10(band / chip);
    printf("  chip rms %.0f, band rms %.0f: %+.1f dB\n", sqrt(chip), sqrt(band), db);
    CHECK(fabs(db) <= 1.5);
    printf("%s sr2_music_band_level\n", failures != before ? "FAIL" : "ok  ");

    host_shutdown();
    return failures ? 1 : 0;
}
