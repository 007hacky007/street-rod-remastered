/* Street Rod 2 Enhanced - entry point (the faithful port's, see ENGINE2.md).
 *
 * usage: streetrod2-enhanced [--game-dir DIR] [--scale N] [--fullscreen] [--check] [--run-ms N] [switches of SR.EXE]
 *   --game-dir    folder with the original game files (default: "Game" in the working directory)
 *   --scale       initial window scale: 320x240 times N (default 3)
 *   --fullscreen  start in full screen (Alt+Enter switches)
 *   --check       load and verify the original executable, print a summary and exit (no window)
 *   --run-ms N    developer aid: exit after N ms (headless snapshot runs)
 *   --music-volume N  ENH: volume of the music, percent 0..100 (default 100)
 *   --sfx-volume N    ENH: volume of the sound effects and the engine, 0..100 (default 100)
 *   adlib | speaker  PORT (launcher): choose the sound card and skip the sound options menu
 *   the original's command-line switches are passed on as they are: 1, nomouse, demo, nouemem, e c v h t (force EGA / CGA / VGA / Hercules / Tandy: skips the video menu)
 */
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game.h"
#include "host.h"
#include "mem.h"
#include "platform/ega.h"

#define APP_TITLE "Street Rod 2 Enhanced"

static int usage(const char *prog)
{
    fprintf(stderr, "usage: %s [--game-dir DIR] [--scale N] [--fullscreen] [--check] [--run-ms N] "
                    "[--music-volume N] [--sfx-volume N] "
                    "[1] [nomouse] [demo] [nouemem] [e|c|v|h|t] [adlib|speaker]\n", prog);
    return 2;
}

/* The program file in `dir`, whatever the case of the name. */
static void find_exe(const char *dir, char *path, size_t n)
{
    int count = 0;
    char **entries = SDL_GlobDirectory(dir, NULL, 0, &count);
    snprintf(path, n, "%s/%s", dir, SR_EXE_NAME);
    for (int i = 0; entries && i < count; i++)
        if (SDL_strcasecmp(entries[i], SR_EXE_NAME) == 0) { snprintf(path, n, "%s/%s", dir, entries[i]); break; }
    SDL_free(entries);
}

int main(int argc, char **argv)
{
    const char *dir = "Game";
    char *game_argv[64];
    int game_argc = 0;
    game_argv[game_argc++] = argv[0];
    int scale = 3;
    bool check = false, fullscreen = false;
    int music_volume = 100, sfx_volume = 100;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--game-dir") && v) { dir = v; i++; }
        else if (!strcmp(a, "--scale") && v) { scale = atoi(v); i++; }
        else if (!strcmp(a, "--run-ms") && v) { game_run_ms = atoi(v); i++; }
        else if (!strcmp(a, "--fullscreen")) fullscreen = true;
        else if (!strcmp(a, "--check")) check = true;
        else if (!strcmp(a, "--music-volume") && v) { music_volume = atoi(v); i++; }   /* ENH */
        else if (!strcmp(a, "--sfx-volume") && v) { sfx_volume = atoi(v); i++; }       /* ENH */
        else if (a[0] == '-' && a[1] == '-') return usage(argv[0]);
        else if (game_argc < 63) game_argv[game_argc++] = argv[i];   /* a switch of SR.EXE */
    }

    char exe_path[1024], err[256];
    find_exe(dir, exe_path, sizeof exe_path);
    if (!mem_load_exe(exe_path, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_TITLE, err, NULL);
        return 1;
    }
    if (check) {
        printf("%s ok: image %u bytes at %04X:0000, DGROUP %04X\n", SR_EXE_NAME, mem_image_size, LOAD_SEG, DGROUP);
        return 0;
    }

    host_configure(&(HostConfig){ APP_TITLE, 49716, true });   /* ENH: the shared host (common/host.h) */
    if (!host_init(dir, scale, fullscreen)) return 1;
    host_set_volumes(music_volume, sfx_volume);                  /* ENH */
    ega_init();                     /* 16-colour planar display: frame source, planes */
    game_argv[game_argc] = NULL;
    return game_main(game_argc, game_argv);     /* the original's switches (1, nomouse, demo, nouemem, e c v h t) are read there */
}
