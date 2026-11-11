/* test_sr2_music.c -- Street Rod 2's AdLib voices take the music or the effects volume: after an effect
 * (the police siren on voice 2) the music sequencer, resumed in the same song, takes the voice back.
 *   test_sr2_music <SR2 game dir>
 * Runs headless with the SDL dummy drivers, the emulated clock driven by busy waits. */
#include <SDL3/SDL.h>
#include <stdio.h>

#include "host.h"
#include "mem.h"
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

    host_shutdown();
    printf("%s sr2_music_voice_ownership\n", failures ? "FAIL" : "ok  ");
    return failures ? 1 : 0;
}
