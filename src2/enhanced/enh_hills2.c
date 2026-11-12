/* Street Rod 2's Mulholland (road mode 10h) in the enhanced view (src2/game/race_road.c): the backdrop
 * (road_draw_sky_mulholland), the hillsides of the front view (front_hillsides) and of the mirror
 * (mirror_hillsides). The hillsides are the game's polygons, vertex for vertex, with its quirks: the screen
 * constants step at the row index, row 1 reads the words before the tables as its "row -1", the front's right side
 * uses the left side's test and a vertex at 140h, the mirror reads its banks forward. In exact mode their integer
 * vertices are clipped by clip_polygon's port; a smooth view fills the float vertices under its own clip. The guard
 * rails, their posts and the bridges' trusses (guard_rail, scenery_draw_truss, mulholland_extras_front and _mirror)
 * are geometry too: the game's polygons and 1-px lines (clip_line's clip in exact mode), the posts' widths and the
 * trusses' thickness stepped at the row index, the posts' parity from the rail phase at the view's base. */
#include "enhanced/enh_geo2.h"

#include "mem.h"

#define BACKDROP_ROWS 0x53

int enh_mulholland_sky_dy(int dy, int fall_dir)
{
    if (dy > 8) return 8;
    if (dy < -8 && fall_dir == 0) return -8;
    return dy;
}

void enh_hill_consts(int row, s16 *w0, s16 *h0, s16 *w1, s16 *h1, s16 *e)
{
    int k = 15 - row;
    *w0 = DSS((u16)(DS_hill_peak_tab + 4 * k));
    *h0 = DSS((u16)(DS_hill_peak_tab + 2 + 4 * k));
    *w1 = DSS((u16)(DS_hill_peak_tab + 4 + 4 * k));
    *h1 = DSS((u16)(DS_hill_peak_tab + 6 + 4 * k));
    *e = DSS((u16)(DS_hill_peak_tab + 0x1C + 4 * k));
}

/* ---------------------------------------------------------------- the backdrop */

double enh_mulholland_dy(const Cam *c)
{
    if (c->exact) return enh_mulholland_sky_dy(i16(c->hpos - hgt_rel(c, c->base - 4)), c->fall_dir);
    double dy = c->hpos - interp(c, c->pos - 4, hgt_rel);
    if (dy > 8) return 8;
    if (dy < -8 && c->fall_dir == 0) return -8;
    return dy;
}

double enh_mulholland_horizon(const Cam *c)
{
    double dy = enh_mulholland_dy(c);
    if (c->exact) return enh_proj_y_i(0xF0, Y_ROAD, (int)dy, (int)c->cam_y, (int)c->bounce);
    return 2.0 * dy + 4.0 * (Y_ROAD - c->cam_y) / 240.0 + c->bounce + 60.0;
}

void enh_draw_mulholland_sky(const Cam *c, const EnhView *v)
{
    (void)v;
    EnhBuf *b = &enh_buf;
    double dy = enh_mulholland_dy(c), shift;
    double h = 199 - (0x6E - dy);
    if (h > BACKDROP_ROWS) h = BACKDROP_ROWS;
    if (c->exact) shift = enh_sky_shift(enh_s16_of(c->heading), 1);
    else shift = 2.0 * ((s16)trk(c, TRK_YAW, c->base) + c->hv);   /* the original's rate, not quantised */
    for (int j = 0; j < b->h; j++) {
        double y = ENH_VIEW_Y0 + (j + 0.5) / b->sa;
        if (y < ENH_VIEW_Y0 + h) enh_sheet_row(j, y + 0x5C - dy, shift);
        else { enh_fill_rows(y - 0.5 / b->sa, ENH_VIEW_Y1, COL_HILL); break; }   /* the original: the stale page */
    }
}

/* ---------------------------------------------------------------- hillsides */

/* A row's values as the game's tables hold them: x, and the line y (without ROW_TOP). */
typedef struct { double xl, xr, xol, xor_, y, ybl, ybr; } HRow;

static double bank_y(const Cam *c, const Row *r, int mirror, u16 arr, int i)
{
    double Y = sub16(Y_ROAD, (s16)trk(c, arr, i));
    return (mirror ? mpy(c, r, Y) : py(c, r, Y)) - ROW_TOP;
}

static HRow front_row(const Cam *c, const Row *r)
{
    HRow h;
    double l, rr;
    edges(trk(c, TRK_C, r->n), &l, &rr);
    h.xl = px(c, r, l);
    h.xr = px(c, r, rr);
    h.xol = px(c, r, x_out(c, r, 0));
    h.xor_ = px(c, r, x_out(c, r, 1));
    h.y = py(c, r, Y_ROAD) - ROW_TOP;
    h.ybl = bank_y(c, r, 0, TRK_BL, r->n);
    h.ybr = bank_y(c, r, 0, TRK_BR, r->n);
    return h;
}

static HRow mirror_row(const Cam *c, const Row *r)
{
    HRow h;
    double l, rr;
    edges(trk(c, TRK_C, r->n), &l, &rr);
    h.xl = mpx(c, r, l);
    h.xr = mpx(c, r, rr);
    h.xol = mpx(c, r, mx_out(c, 0));
    h.xor_ = mpx(c, r, mx_out(c, 1));
    h.y = mpy(c, r, Y_ROAD) - ROW_TOP;
    h.ybl = bank_y(c, r, 1, TRK_BL, c->base + r->k);       /* forward (sic) */
    h.ybr = bank_y(c, r, 1, TRK_BR, c->base + r->k);
    return h;
}

/* row 1's "row -1": the words before the tables */
static HRow before_row(const Cam *c)
{
    HRow h = { c->before[0], c->before[1], 0, 0, c->before[2], c->before[3], c->before[4] };
    return h;
}

typedef struct { const Cam *c; int x0, x1, y0, y1; } Clip;

/* road_poly_fill of n points (x, line y) in colour col */
static void cpoly(const Clip *k, int n, const double *xy, u8 col)
{
    double x[6], y[6];
    for (int i = 0; i < n; i++) { x[i] = xy[2 * i]; y[i] = xy[2 * i + 1] + ROW_TOP; }
    poly_incl(k->c, x, y, n, col, k->x0, k->x1, k->y0, k->y1);
}
static void hpoly(const Clip *k, int n, const double *xy) { cpoly(k, n, xy, COL_HILL); }
#define POLY(k, n, ...) do { const double xy_[] = { __VA_ARGS__ }; hpoly(k, n, xy_); } while (0)

void enh_front_hillsides(const Cam *c, const Row *rows, int row, bool l16, int o)
{
    s16 W0, H0, W1, H1, E;
    enh_hill_consts(row, &W0, &H0, &W1, &H1, &E);
    const HRow P = front_row(c, &rows[row]), Q = front_row(c, &rows[row - 1]);
    const HRow Q1 = row >= 2 ? front_row(c, &rows[row - 2]) : before_row(c);
    const Clip k = { c, 0, 0x13F, ENH_VIEW_Y0, 0x65 };
    double dyaw = c->hv;
    bool bridge_q = trk(c, TRK_C, c->base + row - 1) & 0x80;
    if (dyaw > -100 && Q.y != Q.ybl) {
        if (l16 && P.ybl <= P.y) {
            if (bridge_q) {
                POLY(&k, 6, Q1.xl, 0x65, Q1.xl, Q1.y, Q.xl, Q.y, Q.xol, Q.y, Q.xol - W1, Q.y, E, 0x65);
                if (P.ybl < P.y) POLY(&k, 3, Q.xol, Q.y, Q.xol - W1 / 2, Q.y - H1, Q.xol - W1, Q.y);
                else POLY(&k, 4, Q.xol, Q.y, Q.xol - W1, Q.y, P.xol - W1, P.y, P.xol, P.y);
            }
            if (P.ybl < P.y) POLY(&k, 4, Q.xol, Q.y, Q.xol - W1 / 2, Q.y - H1, P.xol - W0 / 2, P.y - H0, P.xol, P.y);
        }
        if (!l16) {
            if (row == 1 || !bridge_q) {
                if (Q.xol + 0x1E < Q.xor_) {
                    POLY(&k, 4, 0, Q.ybl, Q.xol - o, Q.y, P.xol + o, P.y, 0, P.ybl);
                    if (Q.xol > 0 && ((row != 1 && Q1.y == Q1.ybl) || (row == 1 && Q.y > Q.ybl)))
                        POLY(&k, 3, 0, Q.ybl, Q.xol, Q.y, 0, Q.y);
                }
            } else {
                POLY(&k, 6, 0, Q.ybl, Q.xol, Q.y, Q.xl, Q.y, Q1.xl, Q1.y, Q1.xl, 0x65, 0, 0x65);
                POLY(&k, 4, 0, Q.ybl, Q.xol, Q.y, P.xol, P.y, 0, P.ybl);
            }
        }
    }
    if (dyaw < 100 && Q.y != Q.ybr) {
        if (l16 && P.ybr <= P.y) {
            if (bridge_q) {
                POLY(&k, 6, Q1.xr, 0x65, Q1.xr, Q1.y, Q.xr, Q.y, Q.xor_, Q.y, W1 + Q.xor_, Q.y, 0x13F - E, 0x65);
                if (P.ybr < P.y) POLY(&k, 3, Q.xor_, Q.y, W1 / 2 + Q.xor_, Q.y - H1, W1 + Q.xor_, Q.y);
                else POLY(&k, 4, Q.xor_, Q.y, W1 + Q.xor_, Q.y, W1 + P.xor_, P.y, P.xor_, P.y);
            }
            if (P.ybr < P.y) POLY(&k, 4, Q.xor_, Q.y, W1 / 2 + Q.xor_, Q.y - H1, W0 / 2 + P.xor_, P.y - H0, P.xor_, P.y);
        }
        if (!l16) {
            if (row == 1 || !bridge_q) {
                if (Q.xol + 0x1E < Q.xor_) {                    /* sic: the left side's test */
                    POLY(&k, 4, 0x13F, Q.ybr, Q.xor_ - o, Q.y, P.xor_ + o, P.y, 0x140, P.ybr);   /* sic 140h */
                    if (Q.xor_ < 0x13F && ((row != 1 && Q1.y == Q1.ybr) || (row == 1 && Q.y > Q.ybr)))
                        POLY(&k, 3, 0x13F, Q.ybr, Q.xor_, Q.y, 0x13F, Q.y);
                }
            } else {
                POLY(&k, 6, 0x13F, Q.ybr, Q.xor_, Q.y, Q.xr, Q.y, Q1.xr, Q1.y, Q1.xr, 0x65, 0x13F, 0x65);
                POLY(&k, 4, 0x13F, Q.ybr, Q.xor_, Q.y, P.xor_, P.y, 0x13F, P.ybr);
            }
        }
    }
}

void enh_mirror_hillsides(const Cam *c, const Row *mrows, int row, int o)
{
    enum { XMIN = 0xF0, XMAX = 0x13F, YMAX = 0x2A };
    const HRow P = mirror_row(c, &mrows[row]), Q = mirror_row(c, &mrows[row - 1]);
    const HRow Q1 = row >= 2 ? mirror_row(c, &mrows[row - 2]) : before_row(c);
    const Clip k = { c, XMIN, XMAX, 0x16, YMAX };
    /* the band's flags are those of base - row - 1; the word before them is the next segment back */
    bool bridge = trk(c, TRK_C, c->base - row - 2) & 0x80;
    if (o != 1 && Q.y != Q.ybl) {
        if (row == 1 || !bridge) {
            POLY(&k, 4, XMIN, Q.ybl, Q.xol - o, Q.y, P.xol + o, P.y, XMIN, P.ybl);
            if (row == 1 && P.xol > 0xF0) POLY(&k, 3, XMIN, Q.ybl, P.xol, P.y, XMIN, P.y);
            else if (Q1.y == Q1.ybl && Q.xol > 0xF0) POLY(&k, 3, XMIN, Q.ybl, Q.xol, Q.y, XMIN, Q.y);
        } else
            POLY(&k, 4, XMIN, Q.ybl, Q.xol, Q.y, P.xol, P.y, XMIN, P.ybl);
        if (row != 1 && bridge)
            POLY(&k, 6, XMIN, Q.ybl, Q.xol, Q.y, Q.xl, Q.y, Q1.xl, Q1.y, Q1.xl, YMAX, XMIN, YMAX);
    }
    if (o != -1 && Q.y != Q.ybr) {
        if (row == 1 || !bridge) {
            POLY(&k, 4, XMAX, Q.ybr, Q.xor_ - o, Q.y, P.xor_ + o, P.y, XMAX, P.ybr);
            if (row == 1 && P.xor_ < 0x13F) POLY(&k, 3, XMAX, Q.ybr, P.xor_, P.y, XMAX, P.y);
            else if (Q1.y == Q1.ybr && Q.xor_ < 0x13F) POLY(&k, 3, XMAX, Q.ybr, Q.xor_, Q.y, XMAX, Q.y);
        } else
            POLY(&k, 4, XMAX, Q.ybr, Q.xor_, Q.y, P.xor_, P.y, XMAX, P.ybr);
        if (row != 1 && bridge)
            POLY(&k, 6, XMAX, Q.ybr, Q.xor_, Q.y, Q.xr, Q.y, Q1.xr, Q1.y, Q1.xr, YMAX, XMAX, YMAX);
    }
}

/* ---------------------------------------------------------------- guard rails and trusses */

bool enh_rail_post(int phase, int row, int seg, int n) { return cmod16((s16)(phase - row - seg + n), 2) != 0; }

int enh_rail_phase(const Cam *c, int fallback)             /* road_draw_front */
{
    for (int k = FAR_ROWS; k != 0; k--)
        if ((trk(c, TRK_R, c->base + k) & 0x8000) && !(trk(c, TRK_R, c->base + k - 1) & 0x8000)) return c->base + k;
    return fallback;
}

#define COL_RAIL      5        /* the guard rail and its posts */
#define COL_RAIL_EDGE 2        /* their edges */
#define COL_TRUSS     10

/* clip_line: in exact mode the integer ends clipped as the game clips them; a smooth view's line under the
 * current clip. x, y: the original's pixel (the line y). */
static void line(const Clip *k, double x0, double y0, double x1, double y1, u8 col)
{
    if (k->c->exact) {
        s16 in[4] = { (s16)(int)x0, (s16)(int)floor(y0), (s16)(int)x1, (s16)(int)floor(y1) }, o[4];
        if (enh_clip_line_i(in, o, k->x0, k->x1, k->y0, k->y1)) enh_line1(o[0], o[1], o[2], o[3], col);
        return;
    }
    enh_line1(x0, y0, x1, y1, col);
}

static Clip rail_clip(const Cam *c, bool mirror)              /* clip_set_view, clip_set_mirror */
{
    Clip k = { c, 0, 0x13F, ENH_VIEW_Y0, 0x65 };
    if (mirror) { k.x0 = 0xF0; k.y0 = 0x16; k.y1 = 0x2A; }
    return k;
}

static double ytop(const Cam *c, const RailRow *r)          /* the truss's top: 4 YE - 3 Y (world 640h) */
{
    return c->exact ? (s16)(int)(r->ye * 4 - r->y * 3) : r->ye * 4 - r->y * 3;
}

RailRow enh_rail_row_front(const Cam *c, const Row *r)
{
    RailRow t;
    double l, rr;
    edges(trk(c, TRK_C, r->n), &l, &rr);
    t.xl = px(c, r, l);
    t.xr = px(c, r, rr);
    t.xor_ = px(c, r, x_out(c, r, 1));
    t.y = py(c, r, Y_ROAD) - ROW_TOP;
    t.ye = py(c, r, Y_EYE) - ROW_TOP;
    t.yk = py(c, r, Y_KERB) - ROW_TOP;
    return t;
}

RailRow enh_rail_row_mirror(const Cam *c, const Row *mrows, int k)
{
    RailRow t;
    const Row *r = &mrows[k];
    double l, rr;
    edges(trk(c, TRK_C, r->n), &l, &rr);
    t.xl = mpx(c, r, l);
    t.xr = mpx(c, r, rr);
    t.xor_ = mpx(c, r, mx_out(c, 1));
    t.y = mpy(c, r, Y_ROAD) - ROW_TOP;
    t.ye = mpy(c, r, Y_EYE) - ROW_TOP;
    t.yk = mpy(c, &mrows[(k + 1) / 2], Y_KERB) - ROW_TOP;   /* YK[k] holds row ceil(k / 2)'s (sic) */
    return t;
}

void enh_guard_rail(const Cam *c, const RailRow *p, const RailRow *q, int row, int post, int n, int m, int phase, bool mirror)
{
    const Clip k = rail_clip(c, mirror);
    if (enh_rail_post(phase, row, c->base, n)) {
        /* the post: x from XORq + post / (row + 1) (a screen width by the row index) to XORq, from YKq - 1 down to
         * Yq; clip_polygon's clip of the rectangle is its clamp; poly_fill's edge pixels in colour 2 */
        double xa = q->xor_ + post / (row + 1), xb = q->xor_, ya = q->yk - 1, yb = q->y;
        if (xa > xb) { double t = xa; xa = xb; xb = t; }
        if (ya > yb) { double t = ya; ya = yb; yb = t; }
        xa = SDL_max(xa, k.x0); xb = SDL_min(xb, k.x1); ya = SDL_max(ya, k.y0); yb = SDL_min(yb, k.y1);
        if (xa <= xb && ya <= yb) {
            double x[4] = { xa, xa, xb, xb }, y[4] = { ya + ROW_TOP, yb + ROW_TOP, yb + ROW_TOP, ya + ROW_TOP };
            enh_fill_poly_incl(x, y, 4, COL_RAIL);
            enh_line1(xa, ya, xb, ya, COL_RAIL_EDGE);
            enh_line1(xb, ya, xb, yb, COL_RAIL_EDGE);
            enh_line1(xb, yb, xa, yb, COL_RAIL_EDGE);
            enh_line1(xa, yb, xa, ya, COL_RAIL_EDGE);
        }
    }
    /* the rail: from the eye's height (m higher in the mirror) down to the kerb's, XORp to XORq; its top and bottom
     * edges in colour 2 */
    const double w[8] = { p->xor_, p->ye - m, p->xor_, p->yk, q->xor_, q->yk, q->xor_, q->ye - m };
    cpoly(&k, 4, w, COL_RAIL);
    if (c->exact) {                                         /* drawn only where clip_polygon leaves points */
        s16 in[8], out[128];
        for (int i = 0; i < 8; i++) in[i] = (s16)(int)floor(w[i]);
        if (enh_clip_poly_i(in, 4, out, k.x0, k.x1, k.y0, k.y1) < 1) return;
    }
    line(&k, w[0], w[1], w[6], w[7], COL_RAIL_EDGE);
    line(&k, w[2], w[3], w[4], w[5], COL_RAIL_EDGE);
}

void enh_truss(const Cam *c, const RailRow *p, const RailRow *q, int row, int parity, bool near_flag, int w, bool mirror)
{
    (void)row;
    const Clip k = rail_clip(c, mirror);
    double xl1 = q->xl, xr1 = q->xr, yt1 = ytop(c, q), yb1 = q->y, xl2 = p->xl, xr2 = p->xr, yt2 = ytop(c, p), yb2 = p->y;
    for (int n = w + 1; n > 0; n--) {
        double a = xl1 - n, b = xr1 + n, cc = xl2 - n, e = xr2 + n;
        line(&k, a, yt1, a, yb1, COL_TRUSS);
        line(&k, b, yt1, b, yb1, COL_TRUSS);
        line(&k, xl1, yt1 + n - 1, xr1, yt1 + n - 1, COL_TRUSS);
        if (cmod16((s16)parity, 2) == 0 || !near_flag) { line(&k, a, yb1, cc, yt2, COL_TRUSS); line(&k, b, yb1, e, yt2, COL_TRUSS); }
        else { line(&k, a, yt1, cc, yb2, COL_TRUSS); line(&k, b, yt1, e, yb2, COL_TRUSS); }
        line(&k, a, yt1, cc, yt2, COL_TRUSS);
        line(&k, b, yt1, e, yt2, COL_TRUSS);
        line(&k, a, yb1, cc, yb2, COL_TRUSS);
        line(&k, b, yb1, e, yb2, COL_TRUSS);
    }
}

void enh_truss_end(const Cam *c, const RailRow *p, int w, bool mirror)
{
    const Clip k = rail_clip(c, mirror);
    double yt = ytop(c, p);
    for (int n = w + 1; n > 0; n--) {
        line(&k, p->xl - n, yt, p->xl - n, p->y, COL_TRUSS);
        line(&k, p->xr + n, yt, p->xr + n, p->y, COL_TRUSS);
        line(&k, p->xl, yt + n - 1, p->xr, yt + n - 1, COL_TRUSS);
    }
}

static int dash_w(int i) { return DSS((u16)(DS_dash_width + 2 * i)); }

void enh_extras_front(const Cam *c, const Row *rows, int row, int phase)
{
    int s = c->base + row;
    u16 C = trk(c, TRK_C, s), R = trk(c, TRK_R, s);
    if (C & 1) return;
    const RailRow P = enh_rail_row_front(c, &rows[row]), Q = enh_rail_row_front(c, &rows[row - 1]);
    if ((C & 0x80) && (trk(c, TRK_C, s - 1) & 0x80)) enh_truss(c, &P, &Q, row, c->base - row + 14, row < 7, dash_w(row), false);
    if ((C & 0x80) && !(trk(c, TRK_C, s + 1) & 0x80)) enh_truss_end(c, &P, dash_w(row), false);
    if ((R & 0x8000) && !(C & 0x80)) enh_guard_rail(c, &P, &Q, row, -0x1E, 0x0E, 0, phase, false);
}

void enh_extras_mirror(const Cam *c, const Row *mrows, int row, int phase)
{
    int fs = c->base - row - 1;                             /* the band's flags, one segment off */
    u16 C = trk(c, TRK_C, fs), R = trk(c, TRK_R, fs);
    if (C & 1) return;
    const RailRow P = enh_rail_row_mirror(c, mrows, row), Q = enh_rail_row_mirror(c, mrows, row - 1);
    if ((C & 0x80) && (trk(c, TRK_C, fs + 1) & 0x80) && (trk(c, TRK_C, fs + 2) & 0x80))
        enh_truss(c, &P, &Q, row, c->base - row, row < 4, dash_w(13 + row), true);
    if ((C & 0x80) && !(trk(c, TRK_C, fs - 1) & 0x80)) enh_truss_end(c, &P, dash_w(13 + row), true);
    if ((R & 0x8000) && !(C & 0x80)) enh_guard_rail(c, &P, &Q, row, -10, 7, 1, phase, true);
}
