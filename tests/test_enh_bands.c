/* test_enh_bands.c -- the enhanced view's bands (common/enhanced/enh_bands.c) on their own: a scene of the shared
 * drawing calls (sky rows, polygons, rectangles, plain and remastered images, clip and palette switches) drawn in
 * bands is the scene drawn directly, sample for sample and in the resolved frame, whatever the band count, the
 * scale, a full command list, a failed worker, a cache reset mid-frame, a stop with commands pending or a restart.
 * Usage: test_enh_bands <case>. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enh_draw.h"
#include "mem.h"
#include "platform/ega.h"
#include "platform/vga.h"
#include "remaster/rm_pack.h"
#include "remaster/rm_tags.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define ART  "0123456789abcdef0123456789abcdef01234567"
#define AW 16
#define AH 8

static u32 pal_a[16], pal_b[16];
static u8 art_px[AW * AH], plain_px[12 * 6];
static u32 art_rm[AW * AH];
static EnhImage art_img = { AW, AH, art_px, art_rm }, plain_img = { 12, 6, plain_px, NULL };
static u32 xrgb[1280 * 800];

static u32 rnd(u32 *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s >> 8;
}

static double frnd(u32 *s, double lo, double hi) { return lo + (hi - lo) * (rnd(s) & 0xFFFF) / 65536.0; }

/* a tiny pack: picture ART (16 x 8, index (i + j) % 16) with art whose texels vary, some transparent */
static bool setup_pack(void)
{
    char dir[512], path[1024];
    snprintf(dir, sizeof dir, "sre-bands-%d", (int)SDL_GetTicks());       /* in the working directory */
    SDL_CreateDirectory(dir);
    snprintf(path, sizeof path, "%s/pics", dir);
    SDL_CreateDirectory(path);
    snprintf(path, sizeof path, "%s/pack.json", dir);
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "{\"format\": 1, \"pictures\": {\"%s\": {\"ref\": [\"000000\",\"0000AA\",\"00AA00\",\"00AAAA\","
               "\"AA0000\",\"AA00AA\",\"AA5500\",\"AAAAAA\",\"555555\",\"5555FF\",\"55FF55\",\"55FFFF\",\"FF5555\","
               "\"FF55FF\",\"FFFF55\",\"FFFFFF\"]}}, \"shapes\": {}}", ART);
    fclose(f);
    for (int j = 0; j < AH; j++)
        for (int i = 0; i < AW; i++) art_px[j * AW + i] = (u8)((i + j) % 16);
    for (int pass = 0; pass < 2; pass++) {
        SDL_Surface *s = SDL_CreateSurface(AW * 4, AH * 4, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < AH * 4; y++)
            for (int x = 0; x < AW * 4; x++) {
                u32 c = art_px[(y / 4) * AW + x / 4], v;
                if (pass) v = 0xFF000000u | c << 16 | c << 8 | c;
                else v = ((x * 7 + y * 3) % 11 == 0 ? 0x10000000u : 0xFF000000u) | (u32)(x * 4) << 16 |
                         (u32)(y * 8) << 8 | (u32)((x ^ y) * 4 & 0xFF);
                ((u32 *)((u8 *)s->pixels + y * s->pitch))[x] = v;
            }
        snprintf(path, sizeof path, "%s/pics/%s%s", dir, ART, pass ? ".idx.png" : ".png");
        SDL_SavePNG(s, path);
        SDL_DestroySurface(s);
    }
    if (!rm_pack_open(dir)) return false;
    rm_slot_register_raw(1, ART, art_px, AW, AH);
    RmPic *p = rm_pic(1);                          /* loaded on the main thread, as the games do */
    if (!p || p->art != 1) return false;
    for (int j = 0; j < AH; j++)
        for (int i = 0; i < AW; i++) art_rm[j * AW + i] = (i * 5 + j) % 7 == 0 ? 0 : rm_tag(1, i, j);
    for (int i = 0; i < 12 * 6; i++) plain_px[i] = i % 9 == 0 ? ENH_CLEAR : (u8)(i % 16);
    for (int i = 0; i < 16; i++) {
        pal_a[i] = ega_palette[i];
        pal_b[i] = ega_palette[15 - i] ^ 0x102030;
    }
    /* page 0's picture (enh_image_vram), seeded */
    u32 s = 99;
    for (int k = 0; k < 4; k++)
        for (int o = 0; o < 200 * EGA_BYTES_PER_LINE; o++) ega_plane(k)[o] = (u8)rnd(&s);
    return true;
}

/* the scene; extra > 0: that many distinct page-0 rectangles too */
static void draw_scene(int extra)
{
    u32 s = 2024;
    enh_cur_rgb = pal_a;
    enh_clip_view();
    enh_fill_rows(ENH_VIEW_Y0, 0x30, 3);
    enh_fill_rows(0x30, ENH_VIEW_Y1, 6);
    for (int i = 0; i < 200; i++) {
        int n = 3 + (int)(rnd(&s) % 6);
        double cx = frnd(&s, -40, 360), cy = frnd(&s, ENH_VIEW_Y0 - 10, ENH_VIEW_Y1 + 10), r = frnd(&s, 2, 40);
        double xs[8], ys[8], a0 = frnd(&s, 0, 6.28);
        for (int k = 0; k < n; k++) {
            double a = a0 + 6.2831853 * k / n;
            xs[k] = cx + r * cos(a);
            ys[k] = cy + r * 0.6 * sin(a);
        }
        enh_fill_poly(xs, ys, n, (u8)(rnd(&s) % 16));
        if (i % 40 == 20) {
            if (i / 40 & 1) enh_clip_view();
            else enh_clip(0xF0, 0x12, 0x140, 0x2B);
        }
        if (i % 50 == 25) enh_cur_rgb = i / 50 & 1 ? pal_a : pal_b;
        if (i % 25 == 10) {
            double x = frnd(&s, -20, 300), y = frnd(&s, ENH_VIEW_Y0 - 5, ENH_VIEW_Y1 - 10), w = frnd(&s, 8, 90);
            double h = frnd(&s, 4, 40);
            if (rnd(&s) & 1) enh_draw_image(&art_img, x, y, x + w, y + h);
            else enh_draw_image(&art_img, x + w, y, x, y + h);          /* mirrored */
        }
        if (i == 150) enh_fill_rect(frnd(&s, 0, 100), 0x20, frnd(&s, 150, 320), frnd(&s, 0x30, 0x60), 12);
    }
    enh_clip_view();
    enh_cur_rgb = NULL;                            /* drawn without the art */
    enh_draw_image(&art_img, 10.3, 0x40, 80.7, 0x60);
    enh_cur_rgb = pal_b;
    enh_draw_image(&art_img, 200.2, 0x14, 310.1, 0x3A);
    enh_draw_image(&plain_img, 30, 0x20, 90, 0x40);
    enh_draw_image(&plain_img, 160, 0x50, 100, 0x64);
    enh_clip(0xF0, 0x12, 0x140, 0x2B);
    enh_draw_image(&art_img, 230, 0x10, 330, 0x30);
    enh_clip_view();
    int nv = extra > 0 ? extra : 6;
    for (int i = 0; i < nv; i++) {
        const EnhImage *im = enh_image_vram(0, (i * 13) % 300, 0x20 + (i * 7) % 60, 6 + i % 5, 4 + i % 3);
        double x = (i * 37) % 300 + 0.25, y = ENH_VIEW_Y0 + (i * 11) % 70;
        enh_draw_image(im, x, y, x + 18 + i % 9, y + 9 + i % 4);
    }
    enh_cur_rgb = pal_a;
}

typedef struct {
    u8 *px;
    u32 *rgb, *out;
    size_t n, nout;
    int rgb_samples;
    EnhBandStats st;
} Frame;

static void frame_free(Frame *f)
{
    SDL_free(f->px);
    SDL_free(f->rgb);
    SDL_free(f->out);
    memset(f, 0, sizeof *f);
}

/* one frame under the current band configuration */
static void frame_now(int S, int aa, int extra, Frame *f)
{
    frame_free(f);
    enh_images_reset();
    enh_opt.res_scale = S;
    enh_opt.aa = aa;
    enh_buf_setup();
    EnhBuf *b = &enh_buf;
    size_t n = (size_t)b->w * b->h;
    memset(b->px, 0xFD, n);
    for (size_t i = 0; i < n; i++) b->rgb[i] = 0xDEADBEEF;
    memset(xrgb, 0, sizeof xrgb);
    draw_scene(extra);
    enh_resolve(xrgb, pal_a, 16, 0, ENH_VIEW_Y1);
    enh_bands_stats(&f->st);
    size_t W = 320 * (size_t)S, y0 = (size_t)ENH_VIEW_Y0 * S, y1 = (size_t)ENH_VIEW_Y1 * S;
    f->n = n;
    f->nout = (y1 - y0) * W;
    f->px = SDL_malloc(n);
    f->rgb = SDL_calloc(n, sizeof *f->rgb);
    f->out = SDL_malloc(f->nout * sizeof *f->out);
    memcpy(f->px, b->px, n);
    for (size_t i = 0; i < n; i++)
        if (b->px[i] == ENH_RGB) { f->rgb[i] = b->rgb[i]; f->rgb_samples++; }
    memcpy(f->out, xrgb + y0 * W, f->nout * sizeof *f->out);
}

static void frame(int bands, int cmd_max, size_t arena, int fail_after, int S, int aa, int extra, Frame *f)
{
    enh_bands_config(bands, cmd_max, arena, fail_after);
    frame_now(S, aa, extra, f);
}

static bool same(const Frame *a, const Frame *b)
{
    if (a->n != b->n || a->nout != b->nout) return false;
    long dpx = 0, drgb = 0, dout = 0;
    for (size_t i = 0; i < a->n; i++) { dpx += a->px[i] != b->px[i]; drgb += a->rgb[i] != b->rgb[i]; }
    for (size_t i = 0; i < a->nout; i++) dout += a->out[i] != b->out[i];
    if (dpx || drgb || dout) printf("  differing: samples %ld art %ld output %ld\n", dpx, drgb, dout);
    return !dpx && !drgb && !dout;
}

/* the direct frame, with art drawn */
static void reference(int S, int aa, int extra, Frame *f)
{
    frame(1, 0, 0, -1, S, aa, extra, f);
    CHECK(f->st.bands == 1);
    CHECK(f->rgb_samples > 0);
}

static void bands_identity_counts(void)
{
    static const int scales[3][2] = { { 1, 1 }, { 3, 3 }, { 4, 2 } };
    static const int counts[4] = { 2, 3, 7, 16 };
    Frame one = { 0 }, f = { 0 };
    for (int s = 0; s < 3; s++) {
        reference(scales[s][0], scales[s][1], 0, &one);
        for (int c = 0; c < 4; c++) {
            frame(counts[c], 0, 0, -1, scales[s][0], scales[s][1], 0, &f);
            printf("  scale %d aa %d bands %d: ran in %d, %d commands\n", scales[s][0], scales[s][1], counts[c],
                   f.st.bands, f.st.cmds);
            CHECK(same(&one, &f));
            CHECK(f.st.bands == counts[c]);
        }
    }
    frame_free(&one);
    frame_free(&f);
}

static void bands_full_list(void)
{
    Frame one = { 0 }, f = { 0 };
    reference(4, 2, 0, &one);
    frame(4, 5, 0, -1, 4, 2, 0, &f);
    printf("  full %ld flushes %d\n", f.st.full, f.st.flushes);
    CHECK(same(&one, &f));
    CHECK(f.st.full > 0 && f.st.flushes > 0);
    frame_free(&one);
    frame_free(&f);
}

static void bands_arena_full(void)
{
    Frame one = { 0 }, f = { 0 };
    reference(4, 2, 0, &one);
    frame(4, 0, 2048, -1, 4, 2, 0, &f);
    printf("  flushes %d\n", f.st.flushes);
    CHECK(same(&one, &f));
    CHECK(f.st.flushes > 0);
    frame_free(&one);
    frame_free(&f);
}

static void bands_thread_fail(void)
{
    Frame one = { 0 }, f = { 0 };
    reference(4, 2, 0, &one);
    frame(8, 0, 0, 2, 4, 2, 0, &f);
    CHECK(same(&one, &f));
    CHECK(f.st.bands == 3);
    frame(8, 0, 0, 0, 4, 2, 0, &f);
    CHECK(same(&one, &f));
    CHECK(f.st.bands == 1);
    frame_free(&one);
    frame_free(&f);
}

static void bands_cache_overflow(void)
{
    Frame one = { 0 }, f = { 0 };
    reference(2, 2, 50, &one);
    frame(4, 0, 0, -1, 2, 2, 50, &f);
    printf("  flushes %d\n", f.st.flushes);
    CHECK(same(&one, &f));
    CHECK(f.st.flushes >= 1);
    frame_free(&one);
    frame_free(&f);
}

static void bands_shutdown_runs_pending(void)
{
    Frame one = { 0 }, f = { 0 };
    reference(4, 2, 0, &one);
    enh_bands_config(4, 0, 0, -1);
    enh_buf_setup();
    memset(enh_buf.px, 0, (size_t)enh_buf.w * enh_buf.h);
    enh_clip_view();
    enh_fill_rows(0x20, 0x30, 9);
    enh_bands_shutdown();
    CHECK(enh_buf.px[(size_t)(0x28 - ENH_VIEW_Y0) * enh_buf.sa * enh_buf.w + 5] == 9);
    enh_bands_shutdown();                          /* harmless twice */
    frame_now(4, 2, 0, &f);
    CHECK(f.st.bands == 1);
    CHECK(same(&one, &f));
    frame_free(&one);
    frame_free(&f);
}

static void bands_restart(void)
{
    static const int counts[3] = { 4, 7, 4 };
    Frame one = { 0 }, f = { 0 };
    reference(2, 2, 0, &one);
    for (int i = 0; i < 3; i++) {
        frame(counts[i], 0, 0, -1, 2, 2, 0, &f);
        CHECK(same(&one, &f));
        CHECK(f.st.bands == counts[i]);
    }
    for (int i = 0; i < 30; i++) {                 /* restarts: no hang */
        enh_bands_config(2 + i % 6, 0, 0, -1);
        enh_fill_rows(0x20, 0x30, (u8)i);
        enh_flush();
    }
    frame(5, 0, 0, -1, 2, 2, 0, &f);
    CHECK(same(&one, &f));
    CHECK(f.st.bands == 5);
    frame_free(&one);
    frame_free(&f);
}

static const struct { const char *name; void (*fn)(void); } cases[] = {
    { "bands_identity_counts", bands_identity_counts },
    { "bands_full_list", bands_full_list },
    { "bands_arena_full", bands_arena_full },
    { "bands_thread_fail", bands_thread_fail },
    { "bands_cache_overflow", bands_cache_overflow },
    { "bands_shutdown_runs_pending", bands_shutdown_runs_pending },
    { "bands_restart", bands_restart },
};

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <case>\n", argv[0]); return 2; }
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    memset(mem, 0, MEM_SIZE);
    vga_mode_0d();
    ega_init();
    if (!setup_pack()) { printf("FAIL pack\n"); return 1; }
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        if (strcmp(argv[1], cases[i].name)) continue;
        cases[i].fn();
        printf("%s %s\n", failures ? "FAIL" : "ok  ", cases[i].name);
        enh_bands_config(1, 0, 0, -1);
        return failures != 0;
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
