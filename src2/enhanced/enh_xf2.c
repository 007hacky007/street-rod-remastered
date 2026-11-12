/* An image under an affine map (enh_internal.h: enh_draw_image_xf): the Aqueduct's cuts turning with the view. */
#include <math.h>

#include "enhanced/enh_internal.h"

void enh_draw_image_xf(const EnhImage *img, double x0, double y0, const double m[6])
{
    if (!img || img->w <= 0 || img->h <= 0) return;
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
    int i0 = SDL_max(b->cx0, (int)ceil(lx * sa - 0.5)), i1 = SDL_min(b->cx1, (int)ceil(hx * sa - 0.5));
    int j0 = SDL_max(b->cy0, (int)ceil((ly - ENH_VIEW_Y0) * sa - 0.5));
    int j1 = SDL_min(b->cy1, (int)ceil((hy - ENH_VIEW_Y0) * sa - 0.5));
    /* the inverse map, view to frame */
    double ia = m[4] / det, ib = -m[1] / det, ic = -m[3] / det, id = m[0] / det;
    for (int j = j0; j < j1; j++) {
        double Y = ENH_VIEW_Y0 + (j + 0.5) / sa - m[5];
        u8 *out = b->px + (size_t)j * b->w;
        for (int i = i0; i < i1; i++) {
            double X = (i + 0.5) / sa - m[2];
            double u = floor(ia * X + ib * Y - x0), v = floor(ic * X + id * Y - y0);
            if (u < 0 || u >= img->w || v < 0 || v >= img->h) continue;
            u8 c = img->px[(size_t)v * img->w + (size_t)u];
            if (c != ENH_CLEAR) out[i] = c;
        }
    }
}
