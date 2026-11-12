/* An image under an affine map (enh_internal.h: enh_draw_image_xf): the Aqueduct's cuts turning with the view. */
#include <math.h>

#include "enhanced/enh_internal.h"
#include "remaster/rm_pack.h"
#include "remaster/rm_tags.h"

typedef struct { EnhImage img; double x0, y0, m[6]; } DrawXf;
static void draw_xf_run(const EnhCtx *ctx, const void *arg)
{
    const DrawXf *q = arg;
    const EnhImage *img = &q->img;
    double x0 = q->x0, y0 = q->y0;
    const double *m = q->m;
    const u32 *cur_rgb = ctx->rgb;
    double det = m[0] * m[4] - m[1] * m[3];
    if (det == 0) return;
    EnhBuf *b = &enh_buf;
    double sa = b->sa;
    /* the samples whose centres can land in the image: the box of its mapped corners */
    double lx = INFINITY, hx = -INFINITY, ly = INFINITY, hy = -INFINITY;
    for (int k = 0; k < 4; k++) {
        double x = x0 + (k & 1 ? img->w : 0), y = y0 + (k & 2 ? img->h : 0);
        double X = m[0] * x + m[1] * y + m[2], Y = m[3] * x + m[4] * y + m[5];
        lx = fmin(lx, X); hx = fmax(hx, X);
        ly = fmin(ly, Y); hy = fmax(hy, Y);
    }
    int i0 = SDL_max(ctx->cx0, (int)ceil(lx * sa - 0.5)), i1 = SDL_min(ctx->cx1, (int)ceil(hx * sa - 0.5));
    int j0 = SDL_max(ctx->cy0, (int)ceil((ly - ENH_VIEW_Y0) * sa - 0.5));
    int j1 = SDL_min(ctx->cy1, (int)ceil((hy - ENH_VIEW_Y0) * sa - 0.5));
    /* the inverse map, view to frame */
    double ia = m[4] / det, ib = -m[1] / det, ic = -m[3] / det, id = m[0] / det;
    bool art = img->rm && b->rgb && cur_rgb;     /* ENH: remastered art, 4x the texels (as enh_draw_image) */
    int st_mode = art ? rm_selftest() : 0;
    for (int j = j0; j < j1; j++) {
        double Y = ENH_VIEW_Y0 + (j + 0.5) / sa - m[5];
        u8 *out = b->px + (size_t)j * b->w;
        u32 *orgb = art ? b->rgb + (size_t)j * b->w : NULL;
        for (int i = i0; i < i1; i++) {
            double X = (i + 0.5) / sa - m[2];
            double uf = ia * X + ib * Y - x0, vf = ic * X + id * Y - y0, u = floor(uf), v = floor(vf);
            if (u < 0 || u >= img->w || v < 0 || v >= img->h) continue;
            size_t k = (size_t)v * img->w + (size_t)u;
            u8 c = img->px[k];
            RmTag t = art ? img->rm[k] : 0;
            RmPic *pic = t ? rm_pic(rm_tag_slot(t)) : NULL;
            if (!pic) {
                if (c != ENH_CLEAR) out[i] = c;
                continue;
            }
            int a, fu = (int)((uf - u) * RM_ART_SCALE), fv = (int)((vf - v) * RM_ART_SCALE);
            u32 col = rm_texel(pic, rm_tag_u(t) * RM_ART_SCALE + fu, rm_tag_v(t) * RM_ART_SCALE + fv, cur_rgb, &a);
            if (st_mode) rm_selftest_race(a < 128 || col != cur_rgb[c]);
            if (a < 128) continue;
            if (st_mode == 2) col = (col >> 1 & 0x7F7F7F) + 0x400060;
            out[i] = ENH_RGB;
            orgb[i] = col;
        }
    }
}

void enh_draw_image_xf(const EnhImage *img, double x0, double y0, const double m[6])
{
    if (!img || img->w <= 0 || img->h <= 0) return;
    DrawXf a = { *img, x0, y0, { m[0], m[1], m[2], m[3], m[4], m[5] } };
    enh_submit(draw_xf_run, &a, sizeof a);
}
