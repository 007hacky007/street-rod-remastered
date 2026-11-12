/* The enhanced view's frame in bands (enh_draw.h): the drawing calls' command list, run and resolved in horizontal
 * bands on worker threads. After an idea by bsvdoom (kylofon/streetrod-enhanced, "Enhanced view: rasterize and
 * resolve on worker threads"). */
#include "enh_bands.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "remaster/rm_pack.h"

#define MAX_BANDS 16
#define CMD_MAX   32768
#define ARENA     (4u << 20)

typedef struct { EnhCmdFn fn; u32 off; int cx0, cy0, cx1, cy1; const u32 *rgb; } Cmd;

static Cmd *cmds;
static int ncmd;
static u8 *arena;
static size_t used;

static int nbands;                 /* 0: not started yet */
static bool direct;                /* this frame draws directly (one band, a serial dev aid, the check's first pass) */
static SDL_Thread *threads[MAX_BANDS];
static SDL_Semaphore *sem_go[MAX_BANDS], *sem_done[MAX_BANDS];
static int nworkers;
static bool quit;
static const EnhResolveJob *job;

/* band k's output rows (counted from the view's top) */
static void band_rows(int k, int n, int *r0, int *r1)
{
    int rows = enh_buf.h / enh_opt.aa;
    *r0 = rows * k / n;
    *r1 = rows * (k + 1) / n;
}

static void run_band(int k, int n)
{
    int r0, r1, aa = enh_opt.aa;
    band_rows(k, n, &r0, &r1);
    int by0 = r0 * aa, by1 = r1 * aa;
    for (int i = 0; i < ncmd; i++) {
        const Cmd *c = &cmds[i];
        EnhCtx ctx = { c->cx0, SDL_max(c->cy0, by0), c->cx1, SDL_min(c->cy1, by1), by0, by1, c->rgb };
        c->fn(&ctx, arena + c->off);
    }
    if (job) enh_resolve_rows(job, r0, r1);
}

static int SDLCALL worker(void *data)
{
    int k = (int)(intptr_t)data;
    for (;;) {
        SDL_WaitSemaphore(sem_go[k]);
        if (quit) break;
        run_band(k + 1, nworkers + 1);
        SDL_SignalSemaphore(sem_done[k]);
    }
    return 0;
}

void enh_bands_shutdown(void)
{
    if (nworkers == 0) return;
    quit = true;
    for (int i = 0; i < nworkers; i++) SDL_SignalSemaphore(sem_go[i]);
    for (int i = 0; i < nworkers; i++) {
        SDL_WaitThread(threads[i], NULL);
        SDL_DestroySemaphore(sem_go[i]);
        SDL_DestroySemaphore(sem_done[i]);
    }
    nworkers = 0;
    nbands = 1;                    /* anything drawn after: directly */
    ncmd = 0;
    used = 0;
}

/* SR_ENH_THREADS=N: N bands (1: no worker, the single-threaded drawing); default one per logical core. */
static void start(void)
{
    if (nbands) return;
    nbands = 1;
    const char *t = SDL_getenv("SR_ENH_THREADS");
    int n = t && *t ? SDL_atoi(t) : SDL_GetNumLogicalCPUCores();
    n = SDL_clamp(n, 1, MAX_BANDS);
    if (n == 1) return;
    cmds = SDL_malloc(CMD_MAX * sizeof *cmds);
    arena = SDL_malloc(ARENA);
    if (!cmds || !arena) { SDL_free(cmds); SDL_free(arena); cmds = NULL; arena = NULL; return; }
    for (int i = 0; i < n - 1; i++) {
        sem_go[i] = SDL_CreateSemaphore(0);
        sem_done[i] = SDL_CreateSemaphore(0);
        char name[24];
        SDL_snprintf(name, sizeof name, "enh_band%d", i + 1);
        threads[i] = sem_go[i] && sem_done[i] ? SDL_CreateThread(worker, name, (void *)(intptr_t)i) : NULL;
        if (!threads[i]) {
            if (sem_go[i]) SDL_DestroySemaphore(sem_go[i]);
            if (sem_done[i]) SDL_DestroySemaphore(sem_done[i]);
            fprintf(stderr, "enhanced view: %d of %d worker threads started: %s\n", i, n - 1, SDL_GetError());
            break;
        }
        nworkers++;
    }
    nbands = nworkers + 1;
    if (nworkers) {
        host_set_shutdown_handler(enh_bands_shutdown);  /* the workers end before SDL_Quit */
        atexit(enh_bands_shutdown);                     /* and on any other way out (it runs once) */
    }
}

int enh_bands(void)
{
    start();
    return nbands;
}

/* the remaster's self-test counts and palette log keep their order on one thread */
static bool serial(void)
{
    static int log = -1;
    if (log < 0) { const char *p = SDL_getenv("SR_REMASTER_PALETTE_LOG"); log = p && *p; }
    return rm_selftest() || log;
}

static void run_all(const EnhResolveJob *q)
{
    job = q;
    for (int i = 0; i < nworkers; i++) SDL_SignalSemaphore(sem_go[i]);
    run_band(0, nworkers + 1);
    for (int i = 0; i < nworkers; i++) SDL_WaitSemaphore(sem_done[i]);
    job = NULL;
    ncmd = 0;
    used = 0;
}

void enh_flush(void)
{
    if (ncmd) run_all(NULL);
}

void enh_submit(EnhCmdFn fn, const void *arg, size_t size)
{
    EnhBuf *b = &enh_buf;
    start();
    if (direct || nbands == 1 || serial()) {
        enh_flush();
        EnhCtx k = { b->cx0, b->cy0, b->cx1, b->cy1, 0, b->h, enh_cur_rgb };
        fn(&k, arg);
        return;
    }
    size_t at = (used + 15) & ~(size_t)15;
    if (ncmd == CMD_MAX || at + size > ARENA) {
        static bool warned;
        if (!warned) fprintf(stderr, "enhanced view: the frame's command list is full, drawn in parts\n");
        warned = true;
        enh_flush();
        at = 0;
    }
    memcpy(arena + at, arg, size);
    used = at + size;
    cmds[ncmd++] = (Cmd){ fn, (u32)at, b->cx0, b->cy0, b->cx1, b->cy1, enh_cur_rgb };
}

void enh_bands_run(const EnhResolveJob *q)
{
    start();
    if (nworkers == 0 || direct) {
        enh_flush();
        enh_resolve_rows(q, 0, enh_buf.h / enh_opt.aa);
        return;
    }
    run_all(q);
}

/* ---------------------------------------------------------------- SR_ENH_THREADS_CHECK */

bool enh_bands_check_on(void)
{
    static int on = -1;
    if (on < 0) { const char *p = SDL_getenv("SR_ENH_THREADS_CHECK"); on = p && *p && *p != '0'; }
    return on;
}

static long frames, bad_frames;
static int check_bands;
static void check_report(void)
{
    fprintf(stderr, "enh threads check: frames %ld bands %d differing %ld (at exit)\n", frames, check_bands, bad_frames);
}

void enh_bands_check(void (*draw)(const void *v), const void *v, u32 *xrgb, const u32 *pal, int npal, u16 page,
                     int y_end)
{
    static u8 *px;
    static u32 *rgb, *orig, *frame;
    static size_t cap;
    enh_buf_setup();
    EnhBuf *b = &enh_buf;
    int S = enh_opt.res_scale, W = 320 * S;
    size_t n = (size_t)b->w * b->h, fy0 = (size_t)ENH_VIEW_Y0 * S * W, fn = (size_t)(y_end - ENH_VIEW_Y0) * S * W;
    if (y_end <= ENH_VIEW_Y0) fn = 0;
    if (cap < n + fn) {
        cap = n + fn;
        SDL_free(px); SDL_free(rgb); SDL_free(orig); SDL_free(frame);
        px = SDL_malloc(n);
        rgb = SDL_malloc(n * sizeof *rgb);
        orig = SDL_malloc((fn + 1) * sizeof *orig);
        frame = SDL_malloc((fn + 1) * sizeof *frame);
    }
    if (fn) memcpy(orig, xrgb + fy0, fn * sizeof *orig);
    enh_flush();
    direct = true;                 /* the single-threaded frame */
    memset(b->px, 0xFD, n);
    draw(v);
    enh_resolve(xrgb, pal, npal, page, y_end);
    direct = false;
    memcpy(px, b->px, n);
    if (b->rgb) memcpy(rgb, b->rgb, n * sizeof *rgb);
    if (fn) { memcpy(frame, xrgb + fy0, fn * sizeof *frame); memcpy(xrgb + fy0, orig, fn * sizeof *orig); }
    memset(b->px, 0xFD, n);        /* the banded one */
    draw(v);
    enh_resolve(xrgb, pal, npal, page, y_end);
    long dpx = 0, drgb = 0, dout = 0;
    for (size_t i = 0; i < n; i++) {
        if (px[i] != b->px[i]) dpx++;
        else if (b->rgb && px[i] == ENH_RGB && rgb[i] != b->rgb[i]) drgb++;
    }
    for (size_t i = 0; i < fn; i++) dout += frame[i] != xrgb[fy0 + i];
    if (frames++ == 0) atexit(check_report);
    check_bands = nbands;
    bool bad = dpx || drgb || dout;
    if (bad) bad_frames++;
    if (bad || frames % 100 == 1)
        fprintf(stderr, "enh threads check: frames %ld bands %d differing %ld (this frame: samples %ld art %ld "
                "output %ld)\n", frames, nbands, bad_frames, dpx, drgb, dout);
}
