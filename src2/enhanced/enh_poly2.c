/* The enhanced view's polygons as Street Rod 2's road_poly_fill draws them (enh_poly2.h). */
#include "enhanced/enh_poly2.h"

#include <math.h>
#include <stddef.h>
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

typedef struct { int n; u8 col; double xy[128]; } PolyIncl;   /* x[n], then y[n]; only those copied */
static void poly_incl_run(const EnhCtx *k, const void *arg)
{
    const PolyIncl *a = arg;
    int n = a->n;
    u8 col = a->col;
    const double *x = a->xy, *y = a->xy + n;
    EnhBuf *b = &enh_buf;
    double ymin = y[0], ymax = y[0];
    for (int i = 1; i < n; i++) { ymin = fmin(ymin, y[i]); ymax = fmax(ymax, y[i]); }
    /* the sample rows whose centre is within half an original row of the polygon */
    int j0 = (int)ceil((ymin - 0.5 - ENH_VIEW_Y0) * b->sa - 0.5), j1 = (int)floor((ymax + 0.5 - ENH_VIEW_Y0) * b->sa - 0.5);
    if (j0 < k->cy0) j0 = k->cy0;
    if (j1 > k->cy1 - 1) j1 = k->cy1 - 1;
    for (int j = j0; j <= j1; j++) {
        double yc = ENH_VIEW_Y0 + (j + 0.5) / b->sa;
        double lo = fmax(yc - 0.5, ymin), hi = fmin(yc + 0.5, ymax), xl = 1e30, xr = -1e30;
        if (lo > hi) continue;
        for (int i = 0, m = n - 1; i < n; m = i++) {
            double x0 = x[m], y0 = y[m], x1 = x[i], y1 = y[i];
            double ea = fmax(lo, fmin(y0, y1)), eb = fmin(hi, fmax(y0, y1));
            if (ea > eb) continue;
            double xa = y0 == y1 ? x0 : x0 + (ea - y0) * (x1 - x0) / (y1 - y0);
            double xb = y0 == y1 ? x1 : x0 + (eb - y0) * (x1 - x0) / (y1 - y0);
            xl = fmin(xl, fmin(xa, xb));
            xr = fmax(xr, fmax(xa, xb));
        }
        if (xl > xr) continue;
        int i0 = (int)ceil(xl * b->sa - 0.5), i1 = (int)ceil((xr + 1.0) * b->sa - 0.5);
        if (i0 < k->cx0) i0 = k->cx0;
        if (i1 > k->cx1) i1 = k->cx1;
        if (i1 > i0) memset(b->px + (size_t)j * b->w + i0, col, (size_t)(i1 - i0));
    }
}

void enh_fill_poly_incl(const double *x, const double *y, int n, u8 col)
{
    if (n < 1) return;
    if (n > 64) n = 64;
    PolyIncl a;
    a.n = n;
    a.col = col;
    memcpy(a.xy, x, (size_t)n * sizeof *x);
    memcpy(a.xy + n, y, (size_t)n * sizeof *y);
    enh_submit(poly_incl_run, &a, offsetof(PolyIncl, xy) + (size_t)2 * n * sizeof *x);
}

bool enh_clip_line_i(const s16 in[4], s16 out[4], int xmin, int xmax, int ymin, int ymax)
{
    s16 x1 = in[0], y1 = in[1], x2 = in[2], y2 = in[3], t;
    if ((x1 < xmin && x2 < xmin) || (x1 > xmax && x2 > xmax) || (y1 < ymin && y2 < ymin) || (y1 > ymax && y2 > ymax))
        return false;
    if (x2 < x1) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (x1 < xmin) { y1 = line_y_at_x(x1, y1, x2, y2, (s16)xmin); x1 = (s16)xmin; }
    if (x2 > xmax) { y2 = line_y_at_x(x1, y1, x2, y2, (s16)xmax); x2 = (s16)xmax; }
    if (y2 < y1) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y1 < ymin) { x1 = line_x_at_y(x1, y1, x2, y2, (s16)ymin); y1 = (s16)ymin; }
    if (y2 > ymax) { x2 = line_x_at_y(x1, y1, x2, y2, (s16)ymax); y2 = (s16)ymax; }
    if (!(x1 >= xmin && x1 <= xmax && x2 >= xmin && x2 <= xmax && y1 >= ymin && y1 <= ymax && y2 >= ymin && y2 <= ymax))
        return false;
    out[0] = x1; out[1] = y1; out[2] = x2; out[3] = y2;
    return true;
}

/* the samples whose centre (in original pixels: lo0 + (k + 0.5) / sa) lies in lo..hi, both bounds as `incl_hi`
 * says: (lo, hi] when true, [lo, hi) when false */
static void sample_span(double lo, double hi, double o, int sa, bool incl_hi, int *k0, int *k1)
{
    if (incl_hi) { *k0 = (int)floor((lo - o) * sa - 0.5) + 1; *k1 = (int)floor((hi - o) * sa - 0.5); }
    else { *k0 = (int)ceil((lo - o) * sa - 0.5); *k1 = (int)ceil((hi - o) * sa - 0.5) - 1; }
}

typedef struct { double x0, y0, x1, y1; u8 col; } Line1;
static void line1_run(const EnhCtx *k, const void *arg)
{
    const Line1 *a = arg;
    double x0 = a->x0, y0 = a->y0, x1 = a->x1, y1 = a->y1;
    u8 col = a->col;
    EnhBuf *b = &enh_buf;
    if (x1 < x0) { double t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }   /* ega_line: from the left end */
    double dx = x1 - x0, dy = y1 - y0;
    if (fabs(dx) >= fabs(dy)) {        /* one original pixel tall over each column; a tie towards the far end's y */
        int i0, i1;
        sample_span(x0, x1 + 1, 0, b->sa, false, &i0, &i1);
        if (i0 < k->cx0) i0 = k->cx0;
        if (i1 > k->cx1 - 1) i1 = k->cx1 - 1;
        for (int i = i0; i <= i1; i++) {
            double xs = (i + 0.5) / b->sa;
            double yl = y0 + 0.5 + (dx != 0 ? (xs - x0 - 0.5) * dy / dx : 0);
            int j0, j1;
            sample_span(yl - 0.5, yl + 0.5, ENH_VIEW_Y0, b->sa, dy > 0, &j0, &j1);
            if (j0 < k->cy0) j0 = k->cy0;
            if (j1 > k->cy1 - 1) j1 = k->cy1 - 1;
            for (int j = j0; j <= j1; j++) b->px[(size_t)j * b->w + i] = col;
        }
        return;
    }
    if (y1 < y0) { double t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    dx = x1 - x0;
    dy = y1 - y0;
    int j0, j1;                        /* one original pixel wide over each row; a tie towards the larger x */
    sample_span(y0, y1 + 1, ENH_VIEW_Y0, b->sa, false, &j0, &j1);
    if (j0 < k->cy0) j0 = k->cy0;
    if (j1 > k->cy1 - 1) j1 = k->cy1 - 1;
    for (int j = j0; j <= j1; j++) {
        double ys = ENH_VIEW_Y0 + (j + 0.5) / b->sa;
        double xl = x0 + 0.5 + (ys - y0 - 0.5) * dx / dy;
        int i0, i1;
        sample_span(xl - 0.5, xl + 0.5, 0, b->sa, true, &i0, &i1);
        if (i0 < k->cx0) i0 = k->cx0;
        if (i1 > k->cx1 - 1) i1 = k->cx1 - 1;
        for (int i = i0; i <= i1; i++) b->px[(size_t)j * b->w + i] = col;
    }
}
void enh_line1(double x0, double y0, double x1, double y1, u8 col)
{
    Line1 a = { x0, y0, x1, y1, col };
    enh_submit(line1_run, &a, sizeof a);
}

void enh_fill_poly_edge(const double *x, const double *y, int n, u8 fill, u8 edge)
{
    if (n < 1) return;
    if (n > 64) n = 64;
    double yc[64];
    for (int i = 0; i < n; i++) yc[i] = y[i] + 0.5;  /* the pixel's centre: ROW_TOP (enh_geo2.h) */
    enh_fill_poly_incl(x, yc, n, fill);
    if (edge == fill) return;
    for (int i = 0, k = n - 1; i < n; k = i++) enh_line1(x[k], y[k], x[i], y[i], edge);
}

/* clip_stage in doubles: the new vertex exactly on the clip line */
static int clip_stage_f(const double *ix, const double *iy, int n, double *ox, double *oy, int edge, double e)
{
    int cnt = 0;
    for (int i = 0, pi = n - 1; i < n; pi = i++) {
        double P[2] = { ix[pi], iy[pi] }, C[2] = { ix[i], iy[i] };
        int k = edge < 2 ? 0 : 1;
        bool pin = (edge & 1) ? P[k] <= e : P[k] >= e, cin = (edge & 1) ? C[k] <= e : C[k] >= e;
        if (pin || cin) {
            bool keep = true;
            if (!pin || !cin) {
                double t = (e - P[k]) / (C[k] - P[k]);
                if (edge < 2) { ox[cnt] = e; oy[cnt] = P[1] + t * (C[1] - P[1]); }
                else { oy[cnt] = e; ox[cnt] = P[0] + t * (C[0] - P[0]); }
                cnt++;
                keep = cin;
            }
            if (keep) { ox[cnt] = C[0]; oy[cnt] = C[1]; cnt++; }
        }
        if (cnt >= 60) break;
    }
    return cnt;
}

int enh_clip_poly_f(const double *x, const double *y, int n, double *ox, double *oy,
                    double xmin, double xmax, double ymin, double ymax)
{
    double ax[64], ay[64], bx[64], by[64], cx[64], cy[64];
    if (n > 60) n = 60;
    int na = clip_stage_f(x, y, n, ax, ay, 0, xmin);
    int nb = clip_stage_f(ax, ay, na, bx, by, 1, xmax);
    int nc = clip_stage_f(bx, by, nb, cx, cy, 2, ymin);
    return clip_stage_f(cx, cy, nc, ox, oy, 3, ymax);
}

/* poly_fill's edge walker (race_gfx.c poly_edge_setup, poly_step, poly_edge_advance) in pixel coordinates */
typedef struct { int x, y, cur, next, dir, count, step; s16 err, d1, d2; } Walk;
typedef struct { const s16 *p; int n, remaining; u8 edge; } WalkPoly;

static void px1(int x, int y, u8 col) { enh_fill_rect(x, y, x + 1, y + 1, col); }

static void walk_setup(Walk *e, const WalkPoly *f)
{
    s16 bx = (s16)(f->p[2 * e->next] - f->p[2 * e->cur]);
    s16 ax = (s16)-(s16)(f->p[2 * e->cur + 1] - f->p[2 * e->next + 1]);
    s16 a = bx >= 0 ? bx : (s16)-bx;
    bool xmaj = !(ax > a);
    e->count = xmaj ? a : ax;
    e->err = xmaj ? (s16)(a - 2 * ax) : (s16)(ax - 2 * a);
    e->d1 = xmaj ? (s16)(-2 * ax) : (s16)(-2 * a);
    e->d2 = xmaj ? (s16)(2 * a - 2 * ax) : (s16)(2 * ax - 2 * a);
    e->step = bx >= 0 ? (xmaj ? 0 : 1) : (xmaj ? 3 : 2);
}

/* one step; true when the walker went down a row */
static bool walk_step(Walk *e)
{
    bool neg = e->err < 0;
    s16 add = neg ? e->d2 : e->d1;
    switch (e->step) {
    case 0: case 3:                    /* x-major: along the row, down on a carry */
        e->x += e->step == 0 ? 1 : -1;
        if (neg) e->y++;
        e->err = (s16)(e->err + add);
        return neg;
    default:                           /* y-major: down a row, sideways on a carry */
        e->y++;
        if (neg) e->x += e->step == 1 ? 1 : -1;
        e->err = (s16)(e->err + add);
        return true;
    }
}

/* true when the polygon's vertices are used up */
static bool walk_advance(Walk *e, WalkPoly *f)
{
    for (;;) {
        if (--e->count < 0) {
            if (--f->remaining < 0) return true;
            e->cur = e->next;
            e->next = (e->cur + e->dir + f->n) % f->n;
            walk_setup(e, f);
            continue;
        }
        bool row = walk_step(e);
        px1(e->x, e->y, f->edge);
        if (row) return false;
    }
}

void enh_poly_fill_i(const s16 *pts, int n, u8 fill, u8 edge)
{
    if (n < 1) return;
    int top = 0;
    for (int i = 1; i < n; i++) if (pts[2 * i + 1] < pts[2 * top + 1]) top = i;
    WalkPoly f = { pts, n, n - 2, edge };
    Walk a = { pts[2 * top], pts[2 * top + 1], top, (top - 1 + n) % n, -1, 0, 0, 0, 0, 0 };
    Walk b = { pts[2 * top], pts[2 * top + 1], top, (top + 1) % n, +1, 0, 0, 0, 0, 0 };
    walk_setup(&a, &f);
    walk_setup(&b, &f);
    px1(a.x, a.y, edge);
    for (;;) {
        if (walk_advance(&a, &f)) { walk_advance(&b, &f); break; }
        if (walk_advance(&b, &f)) { walk_advance(&a, &f); break; }
        /* poly_span_fill: strictly between the two walkers (on b's row, by byte and bit as the game orders them) */
        const Walk *l = &a, *r = &b;
        if (a.y * 320 + a.x > b.y * 320 + b.x) { l = &b; r = &a; }
        if (r->x - l->x > 1 && l->y == r->y) enh_fill_rect(l->x + 1, l->y, r->x, l->y + 1, fill);
    }
}
