/* Street Rod 2 Enhanced - entry point (the faithful port's, see ENGINE2.md).
 *
 * usage: streetrod2-enhanced [--game-dir DIR] [--scale N] [--fullscreen] [--check] [--run-ms N] [switches of SR.EXE]
 *   --game-dir    folder with the original game files (default: "Game" in the working directory)
 *   --scale       initial window scale: 320x240 times N (default 3)
 *   --fullscreen  start in full screen (Alt+Enter switches)
 *   --check       load and verify the original executable, print a summary and exit (no window)
 *   --run-ms N    developer aid: exit after N ms (headless snapshot runs)
 *   --dump-pictures DIR  ENH: write every picture of LIB1/LIB2/LIB3 to DIR for the remaster and exit (no window)
 *   --remaster-dir DIR   ENH: show the remastered pictures of the art pack in DIR (output scale 4 even with
 *                        --classic, unless --res-scale is given)
 *   --music-volume N  ENH: volume of the music, percent 0..100 (default 100)
 *   --sfx-volume N    ENH: volume of the sound effects and the engine, 0..100 (default 100)
 *   --music original|remastered  ENH: the music as the game plays it (default), or played by a small band
 *   --classic        ENH: the original's race picture only (else the enhanced race view, ENHANCED.md)
 *   --res-scale N    ENH: the picture is 320x200 times N, 1..8 (default 4; 1 with --classic and no art pack)
 *   --aa N           ENH: anti-aliasing of the enhanced view, N x N samples per pixel, 1..4 (default 2)
 *   --motion-delay P ENH: how far the smooth view runs behind the game, percent of a frame (default 100)
 *   --town on|off    ENH: the drives through town enhanced too (default on)
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
#include "enhanced/enhanced.h"
#include "remaster/rm_dump.h"
#include "remaster/rm_pack.h"
#include "sound/band.h"

#define APP_TITLE "Street Rod 2 Enhanced"

static int usage(const char *prog)
{
    fprintf(stderr, "usage: %s [--game-dir DIR] [--scale N] [--fullscreen] [--check] [--run-ms N] [--dump-pictures DIR] [--remaster-dir DIR] "
                    "[--music-volume N] [--sfx-volume N] [--music original|remastered] "
                    "[--classic] [--res-scale N] [--aa N] [--motion-delay P] [--town on|off] "
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
    const char *dump_dir = NULL, *remaster_dir = NULL;           /* ENH: the remaster's picture dump, art pack */
    int music_volume = 100, sfx_volume = 100;
    bool band = false;
    bool classic = false, town = true;                           /* ENH: the enhanced race view */
    int res_scale = -1, aa = ENH_DEFAULT_AA, motion_delay = ENH_DEFAULT_MOTION_DELAY;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--game-dir") && v) { dir = v; i++; }
        else if (!strcmp(a, "--scale") && v) { scale = atoi(v); i++; }
        else if (!strcmp(a, "--run-ms") && v) { game_run_ms = atoi(v); i++; }
        else if (!strcmp(a, "--fullscreen")) fullscreen = true;
        else if (!strcmp(a, "--check")) check = true;
        else if (!strcmp(a, "--dump-pictures") && v) { dump_dir = v; i++; }          /* ENH */
        else if (!strcmp(a, "--remaster-dir") && v) { remaster_dir = v; i++; }       /* ENH */
        else if (!strcmp(a, "--music-volume") && v) { music_volume = atoi(v); i++; }   /* ENH */
        else if (!strcmp(a, "--sfx-volume") && v) { sfx_volume = atoi(v); i++; }       /* ENH */
        else if (!strcmp(a, "--music") && v && (!strcmp(v, "original") || !strcmp(v, "remastered"))) {   /* ENH */
            band = !strcmp(v, "remastered");
            i++;
        }
        else if (!strcmp(a, "--classic")) classic = true;                             /* ENH */
        else if (!strcmp(a, "--res-scale") && v) { res_scale = atoi(v); i++; }        /* ENH */
        else if (!strcmp(a, "--aa") && v) { aa = atoi(v); i++; }                      /* ENH */
        else if (!strcmp(a, "--motion-delay") && v) { motion_delay = atoi(v); i++; }  /* ENH */
        else if (!strcmp(a, "--town") && v) { town = strcmp(v, "off") != 0; i++; }   /* ENH */
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
    if (dump_dir) {                                              /* ENH */
        int n = rm_dump_pictures(dir, dump_dir);
        if (n < 0) return 1;
        printf("%d pictures written to %s\n", n, dump_dir);
        return 0;
    }

    if (remaster_dir && !rm_pack_open(remaster_dir)) remaster_dir = NULL;   /* ENH: runs as without it */
    if (res_scale < 0) res_scale = classic && !remaster_dir ? 1 : ENH_DEFAULT_RES_SCALE;   /* ENH: as Street Rod 1 */
    enh_init(!classic, res_scale, aa, motion_delay, town);
    host_configure(&(HostConfig){ APP_TITLE, 49716, true });   /* ENH: the shared host (common/host.h) */
    if (!host_init(dir, scale, fullscreen)) return 1;
    host_set_volumes(music_volume, sfx_volume);                  /* ENH */
    if (band && !band_enable(true))                              /* ENH: the remastered music (band.c) */
        fprintf(stderr, "remastered music: the PC speaker tune was not found\n");
    ega_init();                     /* 16-colour planar display: frame source, planes */
    game_argv[game_argc] = NULL;
    atexit(rm_pack_report);                                      /* ENH: the remaster's self-test line */
    return game_main(game_argc, game_argv);     /* the original's switches (1, nomouse, demo, nouemem, e c v h t) are read there */
}
