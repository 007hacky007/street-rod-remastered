/* The enhanced view's polygons as Street Rod 2's road_poly_fill draws them (enh_poly2.h). */
#include "enhanced/enh_poly2.h"

#include <math.h>
#include <string.h>

#include "enh_draw.h"
#include "game/race_int.h"

/* one Sutherland-Hodgman stage of clip_polygon (clip_stage): edge 0 xmin, 1 xmax, 2 ymin, 3 ymax */
static int clip_stage(const s16 *in, int n, s16 *out, int edge, s16 e)
{
    int cnt = 0;
    for (int i = 0, pi = n - 1; i < n; pi = i++) {
        const s16 *P = in + 2 * pi, *C = in + 2 * i;
        int k = edge < 2 ? 0 : 1;
        bool pin = (edge & 1) ? P[k] <= e : P[k] >= e, cin = (edge & 1) ? C[k] <= e : C[k] >= e;
        if (pin || cin) {
            bool keep = true;
            if (!pin || !cin) {
                if (edge < 2) { out[2 * cnt] = e; out[2 * cnt + 1] = line_y_at_x(P[0], P[1], C[0], C[1], e); }
                else { out[2 * cnt + 1] = e; out[2 * cnt] = line_x_at_y(P[0], P[1], C[0], C[1], e); }
                cnt++;
                keep = cin;
            }
            if (keep) { out[2 * cnt] = C[0]; out[2 * cnt + 1] = C[1]; cnt++; }
        }
        if (cnt >= 60) break;
    }
    return cnt;
}

int enh_clip_poly_i(const s16 *in, int n, s16 *out, int xmin, int xmax, int ymin, int ymax)
{
    s16 a[128], b[128], c[128];
    int na = clip_stage(in, n, a, 0, (s16)xmin);
    int nb = clip_stage(a, na, b, 1, (s16)xmax);
    int nc = clip_stage(b, nb, c, 2, (s16)ymin);
    return clip_stage(c, nc, out, 3, (s16)ymax);
}

void enh_fill_poly_incl(const double *x, const double *y, int n, u8 col)
{
    EnhBuf *b = &enh_buf;
    if (n < 1) return;
    double ymin = y[0], ymax = y[0];
    for (int i = 1; i < n; i++) { ymin = fmin(ymin, y[i]); ymax = fmax(ymax, y[i]); }
    /* the sample rows whose centre is within half an original row of the polygon */
    int j0 = (int)ceil((ymin - 0.5 - ENH_VIEW_Y0) * b->sa - 0.5), j1 = (int)floor((ymax + 0.5 - ENH_VIEW_Y0) * b->sa - 0.5);
    if (j0 < b->cy0) j0 = b->cy0;
    if (j1 > b->cy1 - 1) j1 = b->cy1 - 1;
    for (int j = j0; j <= j1; j++) {
        double yc = ENH_VIEW_Y0 + (j + 0.5) / b->sa;
        double lo = fmax(yc - 0.5, ymin), hi = fmin(yc + 0.5, ymax), xl = 1e30, xr = -1e30;
        if (lo > hi) continue;
        for (int i = 0, k = n - 1; i < n; k = i++) {
            double x0 = x[k], y0 = y[k], x1 = x[i], y1 = y[i];
            double ea = fmax(lo, fmin(y0, y1)), eb = fmin(hi, fmax(y0, y1));
            if (ea > eb) continue;
            double xa = y0 == y1 ? x0 : x0 + (ea - y0) * (x1 - x0) / (y1 - y0);
            double xb = y0 == y1 ? x1 : x0 + (eb - y0) * (x1 - x0) / (y1 - y0);
            xl = fmin(xl, fmin(xa, xb));
            xr = fmax(xr, fmax(xa, xb));
        }
        if (xl > xr) continue;
        int i0 = (int)ceil(xl * b->sa - 0.5), i1 = (int)ceil((xr + 1.0) * b->sa - 0.5);
        if (i0 < b->cx0) i0 = b->cx0;
        if (i1 > b->cx1) i1 = b->cx1;
        if (i1 > i0) memset(b->px + (size_t)j * b->w + i0, col, (size_t)(i1 - i0));
    }
}
