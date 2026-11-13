#pragma once
/* Street Rod 2's road geometry shared by the enhanced front view (enh_road2.c) and the mirror (enh_mirror2.c): the
 * camera of a view, its rows, the track tables (the drag strip's through enh_track), the front projection (exact and
 * smooth) and the faces drawn as the game's quad_fill draws them. */
#include <math.h>

#include "enhanced/enh_internal.h"
#include "enhanced/enh_poly2.h"
#include "game/race_int.h"
#include "mem.h"

#define SEG_LAST     (TRK_N - 1)
#define FAR_ROWS     14
#define COL_ROAD     1
#define COL_FINISH   15
#define COL_KERB     4
#define COL_GRASS    9
#define COL_WALL_A   2
#define COL_WALL_B   3
#define COL_STOREY   4
#define COL_START    2         /* the strip's wide dash by the start */
#define COL_SHOULDER 2         /* Mulholland's shoulders beside a bank */
#define COL_HILL     0x0E      /* Mulholland's hillsides, and below its backdrop */
#define Y_ROAD       2000
#define Y_EYE        0x76C
#define Y_KERB       0x79E
#define SKY_SCROLL   0.125     /* as Street Rod 1's enhanced view: the sky turns at this fraction of the original's */

typedef struct {
    int base;
    double pos, lat, hv, hpos;
    double cam_y, bounce;
    double heading;            /* the view's DS_heading, unwrapped */
    int mode, w;               /* DS_road_mode; the outer edges' w (road_project_front): 200 in Mulholland, else 400 */
    int start_seg;             /* DS_start_seg: the strip's wide dash and its walls' phase */
    double hoff;               /* DS_cur_height - HGT[seg]: Mulholland's shoulders go once the car falls (< -0Bh) */
    int fall_dir;              /* DS_fall_dir: the cliff fall's side (Mulholland's backdrop and hillsides) */
    s16 before[5];             /* the DS words before T_XL, T_XR, T_Y, T_YBL and T_YBR: row 1's "row -1" (hillsides) */
    bool exact;                /* the game's integer arithmetic (the compare tool, at a game frame's own position) */
} Cam;

static inline int segc(int i) { return i < 0 ? 0 : i > SEG_LAST ? SEG_LAST : i; }
static inline bool on_strip(const Cam *c) { return (c->mode & 6) != 0; }   /* the Grudge drag strip (modes 2, 4) */
static inline u16 trk(const Cam *c, u16 arr, int i) { return enh_track(c->mode, arr, i); }

static inline double yaw_rel(const Cam *c, int i) { return (s16)(trk(c, TRK_YAW, i) - trk(c, TRK_YAW, c->base)); }
/* the strip is drawn flat (road_project_front: dy = 0), whatever its heights */
static inline double hgt_rel(const Cam *c, int i)
{
    return on_strip(c) ? 0 : (s16)(trk(c, TRK_HGT, i) - trk(c, TRK_HGT, c->base));
}

static inline double interp(const Cam *c, double s, double (*f)(const Cam *, int))
{
    double fl = floor(s);
    int i = (int)fl;
    return f(c, i) + (f(c, i + 1) - f(c, i)) * (s - fl);
}

/* edges_of (race_road.c): flags 20h and 40h give zero-width roads in Street Rod 2 (spec §5 quirk) */
static inline void edges(u16 cm, double *xl, double *xr)
{
    cm &= 0xE0;
    if (cm == 0)        { *xl = 0x5DC; *xr = 0xCE4; }
    else if (cm & 0x80) { *xl = 0x7D0; *xr = 0xAF0; }
    else if (cm & 0x20) { *xl = 0xCE4; *xr = 0xCE4; }
    else                { *xl = 0x5DC; *xr = 0x5DC; }
}

typedef struct { double z, dx, dy; int n, k; } Row;    /* k: the row index, -1 for a depth off the rows */

/* The original's line y is pixel row y from its top: the drawn shapes sit half a row lower than the projected
 * value, so that a sample at a row's centre meets the edge where the original's line starts. */
#define ROW_TOP 0.5

static inline int i16(double v) { return (s16)(u16)(int)v; }   /* an integer value as the game's s16 */

static inline int px_i(const Cam *c, const Row *r, double X) { return enh_proj_x_i((int)r->z, (int)X, i16(r->dx), (int)c->lat); }
static inline int py_i(const Cam *c, const Row *r, double Y)
{
    return enh_proj_y_i((int)r->z, (int)Y, i16(r->dy), (int)c->cam_y, (int)c->bounce);
}

static inline double px(const Cam *c, const Row *r, double X)
{
    if (c->exact) return px_i(c, r, X);
    return 160.0 - 4.0 * (c->lat - (X + r->z * r->dx / 4.0)) / r->z;
}
static inline double py(const Cam *c, const Row *r, double Y)
{
    if (c->exact) return py_i(c, r, Y) + ROW_TOP;
    return 2.0 * r->dy + 4.0 * (Y - c->cam_y) / r->z + c->bounce + 60.0 + ROW_TOP;
}

/* The outer edges' world x (the town's facades, the crossings' kerbs, the shoulders): 5DCh - w on the left,
 * w + CE4h on the right; row 0 has the original's quirk (road_project_front), -(w - cam_x) and
 * (w - cam_x) + 19C8h, in s16 in exact mode. */
static inline double x_out(const Cam *c, const Row *r, int side)
{
    if (r->k == 0) {
        if (c->exact) {
            s16 d = sub16((s16)c->w, (s16)(int)c->lat);
            return side ? (s16)(d + 0x19C8) : (s16)-d;
        }
        double d = c->w - c->lat;
        return side ? d + 0x19C8 : -d;
    }
    return side ? c->w + 0xCE4 : 0x5DC - c->w;
}

/* The mirror's projection (road_proj_x_mirror, road_proj_y_mirror: quarter scale, no flip, no bounce) and its outer
 * edges (no row-0 quirk). */
static inline int mpx_i(const Cam *c, const Row *r, double X) { return enh_proj_x_mirror_i((int)r->z, (int)X, i16(r->dx), (int)c->lat); }
static inline int mpy_i(const Cam *c, const Row *r, double Y) { return enh_proj_y_mirror_i((int)r->z, (int)Y, i16(r->dy), (int)c->cam_y); }
static inline double mpx(const Cam *c, const Row *r, double X)
{
    if (c->exact) return mpx_i(c, r, X);
    return 0x118 - (c->lat - (X + r->z * r->dx / 4.0)) / r->z;
}
static inline double mpy(const Cam *c, const Row *r, double Y)
{
    if (c->exact) return mpy_i(c, r, Y) + ROW_TOP;
    return r->dy + (Y - c->cam_y) / r->z + 0x24 + ROW_TOP;
}
static inline double mx_out(const Cam *c, int side) { return side ? c->w + 0xCE4 : 0x5DC - c->w; }

/* Whether ega_hline draws drv_hspan's span x1..x2 (the game's ends, in its order) on the clip xmin..xmax: it
 * rejects a span by its unsorted ends (x2 left of xmin or x1 right of xmax) before it sorts them, so a reversed
 * span whose ends lie on both sides of the clip is drawn and one off it is not. */
static inline bool hspan_shown(double x1, double x2, double xmin, double xmax) { return !(x2 < xmin || x1 > xmax); }

/* A wall's or storey's face as the game's quad_fill draws it: x = xa (from ya_top to ya_bot) to xb (yb_top to
 * yb_bot), its last row and column included (poly_fill_pts), unlike the road bands' lines. */
static inline void face(double xa, double xb, double ya_bot, double yb_bot, double yb_top, double ya_top, u8 col)
{
    double l = xa < xb ? xa : xb, r = (xa < xb ? xb : xa) + 1.0;
    double xs[4] = { xa == l ? l : r, xb == l ? l : r, xb == l ? l : r, xa == l ? l : r };
    double ys[4] = { ya_bot + 1.0, yb_bot + 1.0, yb_top, ya_top };
    enh_fill_poly(xs, ys, 4, col);
}

/* The near row's x that a band's edge reaches from the far row's (xf at line y yf) towards the near row's (xn at yn),
 * as road_draw_band_front and road_draw_band_mirror step it: in exact mode from xf by slope16 (16ths of a pixel per
 * line, truncated towards 0) each line, the sum floored, so the edge is the straight line to xf + n slope16 / 16 at
 * the near row (n lines on), not to xn; over a long band the truncation adds up to more than half a pixel. A smooth
 * view keeps xn. yf, yn as py gives them (the line y + ROW_TOP). */
static inline double band_near_x(const Cam *c, double xf, double yf, double xn, double yn)
{
    if (!c->exact) return xn;
    s16 f = (s16)(int)floor(yf), n = (s16)(int)floor(yn);
    if (f == n) return xn;
    s16 s = cdiv16(shl16(sub16((s16)(int)xf, (s16)(int)xn), 4), sub16(f, n));
    return xf + (double)(n - f) * s / 16.0;
}

static inline void quad(double xa0, double xa1, double ya, double xb0, double xb1, double yb, u8 col)
{
    double xs[4] = { xb0, xb1, xa1, xa0 }, ys[4] = { yb, yb, ya, ya };
    enh_fill_poly(xs, ys, 4, col);
}

/* The camera of a view, for the road and the objects alike. dy = height - HGT(s): the game's height is HGT[seg]
 * (road_advance_segment) plus hoff (the cliff fall); between frames the track's height at pos keeps the view
 * smooth. dx = heading - YAW(s), the heading unwrapped. */
static inline void cam_of(const EnhView *v, Cam *c)
{
    c->mode = v->mode;
    c->base = segc((int)floor(v->pos));
    c->pos = v->pos;
    c->lat = v->lat;
    c->hpos = on_strip(c) ? 0 : v->exact ? v->hoff : interp(c, v->pos, hgt_rel) + v->hoff;
    double hf = floor(v->heading);
    c->hv = (s16)(u16)(enh_s16_of(hf) - (s16)trk(c, TRK_YAW, c->base)) + (v->heading - hf);
    c->cam_y = v->cam_y;
    c->bounce = v->bounce;
    c->heading = v->heading;
    c->w = v->mode == 0x10 ? 200 : 400;
    c->start_seg = v->start_seg;
    c->hoff = v->hoff;
    c->fall_dir = v->fall_dir;
    for (int i = 0; i < 5; i++) c->before[i] = v->before[i];
    c->exact = v->exact;
}

/* A polygon as road_poly_fill draws it (the strip's barrier walls): in exact mode its integer vertices clipped by
 * clip_polygon on x0..x1, y0..y1 (the game's clip, inclusive), then filled with the edge pixels; in a smooth view
 * the vertices filled so under the current clip. x, y as px and py give them (y the line y + ROW_TOP). */
static inline void poly_incl(const Cam *c, const double *x, const double *y, int n, u8 col, int x0, int x1, int y0, int y1)
{
    if (!c->exact) { enh_fill_poly_incl(x, y, n, col); return; }
    s16 in[16], out[128];
    double ox[64], oy[64];
    for (int i = 0; i < n && i < 8; i++) { in[2 * i] = (s16)(int)x[i]; in[2 * i + 1] = (s16)(int)floor(y[i]); }
    int m = enh_clip_poly_i(in, n < 8 ? n : 8, out, x0, x1, y0, y1);
    for (int i = 0; i < m; i++) { ox[i] = out[2 * i]; oy[i] = out[2 * i + 1] + ROW_TOP; }
    enh_fill_poly_incl(ox, oy, m, col);
}

/* The sky sheets (enh_road2.c): output sample row j shows page 1's row floor(syf) (66h..C7h), its column 0 at x shift. */
void enh_sheet_row(int j, double syf, double shift);

/* Mulholland (enh_hills2.c). The view's backdrop dy (enh_mulholland_sky_dy of the climb from 4 segments back: the
 * game's integers in exact mode, the track's height at pos - 4 in a smooth view) and its horizon,
 * road_proj_y(F0h, 2000, dy) unclamped. */
double enh_mulholland_dy(const Cam *c);
double enh_mulholland_horizon(const Cam *c);
/* road_draw_sky_mulholland: screen rows 12h .. 12h + h - 1 show sheet row y + 5Ch - dy (h = min(53h, 199 - (6Eh - dy))),
 * scrolled as sky_scroll(1) does (exact) or at 2 px per heading unit (smooth); colour 0Eh below it. */
void enh_draw_mulholland_sky(const Cam *c, const EnhView *v);
/* front_hillsides of row `row` (far row p = row, near q = row - 1, q1 = row - 2, row 1's from c->before), rows[]
 * the front's 15 rows; l16 the frame's flag (a bridge ends in rows 14..10), o the fall's side. */
void enh_front_hillsides(const Cam *c, const Row *rows, int row, bool l16, int fall_dir);
/* mirror_hillsides of mirror row `row` (mrows[] rows 0..7, the banks read forward), on the mirror's clip
 * F0h..13Fh x 16h..2Ah. */
void enh_mirror_hillsides(const Cam *c, const Row *mrows, int row, int fall_dir);
/* road_draw_front's DS_rail_phase_seg at the view's base: base + k for the farthest row k (14..1) where a rail
 * starts (R[base + k] & 8000h and not R[base + k - 1]), else the fallback (the value kept from a frame before). */
int enh_rail_phase(const Cam *c, int fallback);
/* Mulholland's guard rails and bridges (mulholland_extras_front, mulholland_extras_mirror): a row's table values
 * as guard_rail and scenery_draw_truss read them (x, and the line y); the mirror's YK as its quirk holds it (row
 * ceil(k / 2)'s). */
typedef struct { double xl, xr, xor_, y, ye, yk; } RailRow;
RailRow enh_rail_row_front(const Cam *c, const Row *r);
RailRow enh_rail_row_mirror(const Cam *c, const Row *mrows, int k);
/* guard_rail of row `row` (far p, near q): the post when enh_rail_post(phase, row, base, n), x from XORq + post /
 * (row + 1) to XORq, YKq - 1 down to Yq, fill 5 and edges 2; the rail XORp..XORq from YE - m down to YK, fill 5, its
 * top and bottom edges 2. Front: post -1Eh, n 0Eh, m 0; mirror: -10, 7, 1 (the front's phase, sic). */
void enh_guard_rail(const Cam *c, const RailRow *p, const RailRow *q, int row, int post, int n, int m, int phase, bool mirror);
/* scenery_draw_truss between p (far) and q (near), lines of colour 10 drawn w + 1 times with the original's offsets
 * (w: the row's DS_dash_width entry), the diagonal by the parity (even, or not near: from the near row's bottom);
 * scenery_draw_truss_end at row p. */
void enh_truss(const Cam *c, const RailRow *p, const RailRow *q, int row, int parity, bool near_flag, int w, bool mirror);
void enh_truss_end(const Cam *c, const RailRow *p, int w, bool mirror);
/* mulholland_extras_front of row `row` (rows[] the front's 15 rows), mulholland_extras_mirror of mirror row `row`
 * (mrows[] its 8 rows, the flags one segment off): the truss (front: C[s] and C[s - 1] bridges, parity base - row
 * + 14, near below row 7; mirror: C[s], C[s + 1], C[s + 2], parity base - row, near below row 4), its end (front:
 * not C[s + 1]; mirror: not C[s - 1]) and the guard rail (R & 8000h off a bridge). */
void enh_extras_front(const Cam *c, const Row *rows, int row, int phase);
void enh_extras_mirror(const Cam *c, const Row *mrows, int row, int phase);
