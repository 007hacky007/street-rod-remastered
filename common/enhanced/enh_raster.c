/* Sample buffer of the enhanced road view: palette-index samples at (res scale x aa) per original pixel,
 * convex polygon fill at sample centres, and the resolve (linear-light average of each output pixel's
 * samples through the current palette) laid over the scaled EGA picture where the coverage allows. */
#include "enh_draw.h"
#include "enh_bands.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "platform/ega.h"
#include "remaster/rm_pack.h"

EnhBuf enh_buf;

void enh_buf_setup(void)
{
    int sa = enh_opt.res_scale * enh_opt.aa;
    int w = 320 * sa, h = (ENH_VIEW_Y1 - ENH_VIEW_Y0) * sa;
    if (enh_buf.px && enh_buf.sa == sa) return;
    enh_flush();                   /* the commands recorded for the old buffer */
    SDL_free(enh_buf.px);
    enh_buf.px = SDL_malloc((size_t)w * h);
    SDL_free(enh_buf.rgb);
    enh_buf.rgb = rm_pack_active() ? SDL_malloc((size_t)w * h * sizeof *enh_buf.rgb) : NULL;
    enh_buf.w = w;
    enh_buf.h = h;
    enh_buf.sa = sa;
    enh_clip_view();
}

void enh_clip(double x0, double y0, double x1, double y1)
{
    EnhBuf *b = &enh_buf;
    b->cx0 = SDL_clamp((int)ceil(x0 * b->sa - 0.5), 0, b->w);
    b->cx1 = SDL_clamp((int)ceil(x1 * b->sa - 0.5), 0, b->w);
    b->cy0 = SDL_clamp((int)ceil((y0 - ENH_VIEW_Y0) * b->sa - 0.5), 0, b->h);
    b->cy1 = SDL_clamp((int)ceil((y1 - ENH_VIEW_Y0) * b->sa - 0.5), 0, b->h);
}

void enh_clip_view(void)
{
    EnhBuf *b = &enh_buf;
    b->cx0 = 0; b->cy0 = 0; b->cx1 = b->w; b->cy1 = b->h;
}

typedef struct { double y0, y1; u8 c; } FillRows;
static void fill_rows_run(const EnhCtx *k, const void *arg)
{
    const FillRows *a = arg;
    EnhBuf *b = &enh_buf;
    int j0 = (int)ceil((a->y0 - ENH_VIEW_Y0) * b->sa - 0.5), j1 = (int)ceil((a->y1 - ENH_VIEW_Y0) * b->sa - 0.5);
    if (j0 < k->cy0) j0 = k->cy0;
    if (j1 > k->cy1) j1 = k->cy1;
    if (k->cx1 <= k->cx0) return;
    for (int j = j0; j < j1; j++) memset(b->px + (size_t)j * b->w + k->cx0, a->c, (size_t)(k->cx1 - k->cx0));
}

void enh_fill_rows(double y0, double y1, u8 c)
{
    FillRows a = { y0, y1, c };
    enh_submit(fill_rows_run, &a, sizeof a);
}

/* Convex polygon, vertices in original screen coordinates (any winding). A sample is covered when its
 * centre is inside (left and top edges inclusive). */
typedef struct { int n; u8 c; double xs[8], ys[8]; } FillPoly;
static void fill_poly_run(const EnhCtx *k, const void *arg)
{
    const FillPoly *a = arg;
    EnhBuf *b = &enh_buf;
    int n = a->n;
    double sx[8], sy[8], ymin = 1e30, ymax = -1e30;
    for (int i = 0; i < n; i++) {
        sx[i] = a->xs[i] * b->sa;
        sy[i] = (a->ys[i] - ENH_VIEW_Y0) * b->sa;
        if (sy[i] < ymin) ymin = sy[i];
        if (sy[i] > ymax) ymax = sy[i];
    }
    int j0 = (int)ceil(ymin - 0.5), j1 = (int)ceil(ymax - 0.5);
    if (j0 < k->cy0) j0 = k->cy0;
    if (j1 > k->cy1) j1 = k->cy1;
    for (int j = j0; j < j1; j++) {
        double yc = j + 0.5, xl = 1e30, xr = -1e30;
        for (int i = 0; i < n; i++) {
            int m = (i + 1) % n;
            double ya = sy[i], yb = sy[m];
            if ((yc < ya) == (yc < yb)) continue;                   /* edge does not cross this row */
            double x = sx[i] + (sx[m] - sx[i]) * (yc - ya) / (yb - ya);
            if (x < xl) xl = x;
            if (x > xr) xr = x;
        }
        if (xl > xr) continue;
        int i0 = (int)ceil(xl - 0.5), i1 = (int)ceil(xr - 0.5);
        if (i0 < k->cx0) i0 = k->cx0;
        if (i1 > k->cx1) i1 = k->cx1;
        if (i1 > i0) memset(b->px + (size_t)j * b->w + i0, a->c, (size_t)(i1 - i0));
    }
}

void enh_fill_poly(const double *xs, const double *ys, int n, u8 c)
{
    if (n > 8) return;
    FillPoly a;
    a.n = n < 0 ? 0 : n;
    a.c = c;
    memcpy(a.xs, xs, (size_t)a.n * sizeof *xs);
    memcpy(a.ys, ys, (size_t)a.n * sizeof *ys);
    enh_submit(fill_poly_run, &a, sizeof a);
}

/* sRGB <-> linear light tables */
static float to_lin[256];
static u8 to_srgb[4096];

static void tables(void)
{
    static bool done;
    if (done) return;
    done = true;
    for (int i = 0; i < 256; i++) {
        double v = i / 255.0;
        to_lin[i] = (float)(v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4));
    }
    for (int i = 0; i < 4096; i++) {
        double v = i / 4095.0;
        double s = v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1 / 2.4) - 0.055;
        to_srgb[i] = (u8)SDL_clamp((int)(s * 255 + 0.5), 0, 255);
    }
}

void enh_resolve_rows(const EnhResolveJob *q, int r0, int r1)
{
    EnhBuf *b = &enh_buf;
    int S = enh_opt.res_scale, aa = enh_opt.aa, W = 320 * S;
    const float *lr = q->lr, *lg = q->lg, *lb = q->lb;
    const u8 *wr = ega_written(), *fg = ega_fg();
    float inv = 4095.0f / (float)(aa * aa);
    if (r1 > (q->y_end - ENH_VIEW_Y0) * S) r1 = (q->y_end - ENH_VIEW_Y0) * S;
    for (int r = r0; r < r1; r++) {
        int y = ENH_VIEW_Y0 + r / S, oy = r % S;
        u16 row = (u16)(q->page + y * EGA_BYTES_PER_LINE);
        {
            int Y = y * S + oy;
            u32 *out = q->xrgb + (size_t)Y * W;
            const u8 *src = b->px + (size_t)((y - ENH_VIEW_Y0) * S + oy) * aa * b->w;
            const u32 *srgb = b->rgb ? b->rgb + (size_t)((y - ENH_VIEW_Y0) * S + oy) * aa * b->w : NULL;
            for (int x = 0; x < 320; x++) {
                u16 o = (u16)(row + (x >> 3));
                u8 bit = (u8)(0x80 >> (x & 7));
                if ((wr[o] | fg[o]) & bit) continue;                /* the original's pixel */
                for (int ox = 0; ox < S; ox++) {
                    int X = x * S + ox;
                    float r_ = 0, g = 0, bl = 0;
                    for (int j = 0; j < aa; j++) {
                        const u8 *s = src + (size_t)j * b->w + X * aa;
                        for (int i = 0; i < aa; i++) {
                            if (s[i] == ENH_RGB && srgb) {           /* ENH: remastered art */
                                u32 c = srgb[(size_t)j * b->w + X * aa + i];
                                r_ += to_lin[c >> 16 & 0xFF]; g += to_lin[c >> 8 & 0xFF]; bl += to_lin[c & 0xFF];
                            } else { r_ += lr[s[i]]; g += lg[s[i]]; bl += lb[s[i]]; }
                        }
                    }
                    out[X] = (u32)to_srgb[(int)(r_ * inv)] << 16 | (u32)to_srgb[(int)(g * inv)] << 8 |
                             to_srgb[(int)(bl * inv)];
                }
            }
        }
    }
}

void enh_resolve(u32 *xrgb, const u32 *pal, int npal, u16 page, int y_end)
{
    tables();
    EnhResolveJob q = { xrgb, page, y_end, { 0 }, { 0 }, { 0 } };
    for (int i = 0; i < npal && i < 256; i++) {
        q.lr[i] = to_lin[pal[i] >> 16 & 0xFF];
        q.lg[i] = to_lin[pal[i] >> 8 & 0xFF];
        q.lb[i] = to_lin[pal[i] & 0xFF];
    }
    enh_bands_run(&q);
}
