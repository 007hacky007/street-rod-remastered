/* Street Rod 2's road view drawn again from the segment table (Street Rod 1's src/enhanced/enh_road.c, with
 * Street Rod 2's projection): the original's 15 rows evaluated in floating point at the smooth position, the
 * bands between them drawn as the original draws them (straight edges from row to row), the sky and the
 * horizon scrolled continuously. At a game frame's own position the result is the original's geometry. Road modes 0 (the
 * town) and 1 (the street drag); the objects and the opponent the game drew are drawn again in the band loop at
 * their own segment (enh_cuts_at); the mirror is the game's own until it is drawn here too.
 *
 * Projection (road_proj_x / road_proj_y, src2/game/race_road.c), a point at world x X, height Y, on the road
 * at continuous segment position s, seen from position pos:
 *     row k at segment base + k, depth z = 8k + 4 - q (q = 8 frac(pos)); row 0 at 4, row 14 at 116 (fixed)
 *     dx = heading - YAW(s),  dy = height(pos) - HGT(s)
 *     sx = 160 - 4 (cam_x - (X + z dx / 4)) / z
 *     sy = 2 dy + 4 (Y - cam_y) / z + bounce + 60      (Street Rod 1: 4 dy, and a fixed eye at 76Ch)
 * The road edges come from the centre flags as edges_of has them, with its zero-width quirk. */
#include "enhanced/enh_internal.h"
#include "enhanced/enhanced.h"

#include <limits.h>
#include <math.h>

#include "game/race_int.h"
#include "mem.h"
#include "platform/ega.h"

#define SEG_LAST     (TRK_N - 1)
#define FAR_ROWS     14
#define COL_ROAD     1
#define COL_FINISH   15
#define COL_KERB     4
#define COL_GRASS    9
#define COL_WALL_A   2
#define COL_WALL_B   3
#define COL_STOREY   4
#define Y_ROAD       2000
#define Y_EYE        0x76C
#define Y_KERB       0x79E
#define SKY_SCROLL   0.125     /* as Street Rod 1's enhanced view: the sky turns at this fraction of the original's */

const u8 enh_road_colours[2] = { COL_ROAD, COL_FINISH };
double enh_ground_y;           /* the first row below the horizon strip (the compare tool starts there) */

typedef struct {
    int base;
    double pos, lat, hv, hpos;
    double cam_y, bounce;
    double heading;            /* the view's DS_heading, unwrapped */
    int mode, w;               /* DS_road_mode; the outer edges' w (road_project_front): 200 in Mulholland, else 400 */
    bool exact;                /* the game's integer arithmetic (the compare tool, at a game frame's own position) */
} Cam;

static int segc(int i) { return i < 0 ? 0 : i > SEG_LAST ? SEG_LAST : i; }
static u16 trk(u16 arr, int i) { return TRKW(arr, segc(i)); }

static double yaw_rel(const Cam *c, int i) { return (s16)(trk(TRK_YAW, i) - trk(TRK_YAW, c->base)); }
static double hgt_rel(const Cam *c, int i) { return (s16)(trk(TRK_HGT, i) - trk(TRK_HGT, c->base)); }

static double interp(const Cam *c, double s, double (*f)(const Cam *, int))
{
    double fl = floor(s);
    int i = (int)fl;
    return f(c, i) + (f(c, i + 1) - f(c, i)) * (s - fl);
}

/* edges_of (race_road.c): flags 20h and 40h give zero-width roads in Street Rod 2 (spec §5 quirk) */
static void edges(u16 cm, double *xl, double *xr)
{
    cm &= 0xE0;
    if (cm == 0)        { *xl = 0x5DC; *xr = 0xCE4; }
    else if (cm & 0x80) { *xl = 0x7D0; *xr = 0xAF0; }
    else if (cm & 0x20) { *xl = 0xCE4; *xr = 0xCE4; }
    else                { *xl = 0x5DC; *xr = 0x5DC; }
}

typedef struct { double z, dx, dy; int n, k; } Row;    /* k: the row index, -1 for a depth off the rows */

/* road_project_front's row k: depth 4 for row 0, 12 - q for row 1 and 8 more for each row after, row 14 at a
 * fixed 116; dx and dy from the row's own segment. */
static Row row_k(const Cam *c, int k)
{
    Row r;
    double q = 8.0 * (c->pos - c->base);
    r.n = c->base + k;
    r.k = k;
    r.z = k == 0 ? 4.0 : k == FAR_ROWS ? 8.0 * FAR_ROWS + 4.0 : 8.0 * k + 4.0 - q;
    r.dx = c->hv - yaw_rel(c, r.n);
    r.dy = c->hpos - hgt_rel(c, r.n);
    return r;
}

/* The original's line y is pixel row y from its top: the drawn shapes sit half a row lower than the projected
 * value, so that a sample at a row's centre meets the edge where the original's line starts. */
#define ROW_TOP 0.5

static int i16(double v) { return (s16)(u16)(int)v; }   /* an integer value as the game's s16 */

int enh_proj_x_i(int z, int X, int dx, int cx)              /* road_proj_x */
{
    s16 x = add16((s16)X, (s16)((s16)(z * dx) >> 2));
    s16 t = shl16(sub16((s16)cx, x), 2);
    if (z == 0) z = 1;
    return sub16(0xA0, cdiv16(t, (s16)z));
}
int enh_proj_y_i(int z, int Y, int dy, int cam_y, int bounce)   /* road_proj_y */
{
    s16 t = add16(shl16((s16)((s16)(z * dy) & 0xFFFE), 1), shl16(sub16((s16)Y, (s16)cam_y), 2));
    if (z == 0) z = 1;
    return add16(add16(cdiv16(t, (s16)z), (s16)bounce), 0x3C);
}
static int px_i(const Cam *c, const Row *r, double X) { return enh_proj_x_i((int)r->z, (int)X, i16(r->dx), (int)c->lat); }
static int py_i(const Cam *c, const Row *r, double Y)
{
    return enh_proj_y_i((int)r->z, (int)Y, i16(r->dy), (int)c->cam_y, (int)c->bounce);
}

static double px(const Cam *c, const Row *r, double X)
{
    if (c->exact) return px_i(c, r, X);
    return 160.0 - 4.0 * (c->lat - (X + r->z * r->dx / 4.0)) / r->z;
}
static double py(const Cam *c, const Row *r, double Y)
{
    if (c->exact) return py_i(c, r, Y) + ROW_TOP;
    return 2.0 * r->dy + 4.0 * (Y - c->cam_y) / r->z + c->bounce + 60.0 + ROW_TOP;
}

/* The outer edges' world x (the town's facades, the crossings' kerbs, the shoulders): 5DCh - w on the left,
 * w + CE4h on the right; row 0 has the original's quirk (road_project_front), -(w - cam_x) and
 * (w - cam_x) + 19C8h, in s16 in exact mode. */
static double x_out(const Cam *c, const Row *r, int side)
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

double enh_dash_width(int k, double q, bool exact)
{
    double t = DSS((u16)(DS_dash_width + 2 * k));
    if (exact) return t;
    double tn = DSS((u16)(DS_dash_width + 2 * (k - 1)));
    return t + (tn - t) * q / 8.0;
}

/* ---------------------------------------------------------------- sky and horizon */

static u8 src_rows[0xC8][320];     /* page 1 rows 66h..C7h: the sky and horizon sheets */

void enh_sheets_read(void)
{
    for (int y = 0x66; y < 0xC8; y++) {
        u16 o = (u16)(0x2000 + y * EGA_BYTES_PER_LINE);
        for (int x = 0; x < 320; x++) {
            u16 b = (u16)(o + (x >> 3));
            int bit = 7 - (x & 7), c = 0;
            for (int k = 0; k < 4; k++) c |= (ega_plane(k)[b] >> bit & 1) << k;
            src_rows[y][x] = (u8)c;
        }
    }
}

static void sheet_row(int j, double syf, double shift)
{
    EnhBuf *b = &enh_buf;
    int sy = (int)floor(syf);
    if (sy < 0x66) sy = 0x66;
    if (sy > 0xC7) sy = 0xC7;
    u8 *out = b->px + (size_t)j * b->w;
    for (int i = 0; i < b->w; i++) {
        double x = (i + 0.5) / b->sa - shift;
        int col = (int)floor(x) % 320;
        if (col < 0) col += 320;
        out[i] = src_rows[sy][col];
    }
}

/* road_draw_sky: the horizon at road_proj_y(F0h, 2000, dy) with dy the climb from the segment behind (0 on the
 * drag strip), and its clamp (a horizon above row 13h goes to 12h, one below 51h to 52h); in exact mode the
 * game's integer row. */
static double horizon_y(const Cam *c)
{
    double hy;
    if (c->exact) {
        int dy = (c->mode & 6) ? 0 : i16(c->hpos - hgt_rel(c, c->base - 1));
        hy = enh_proj_y_i(0xF0, Y_ROAD, dy, (int)c->cam_y, (int)c->bounce);
    } else {
        double dy = c->hpos - hgt_rel(c, c->base - 1);
        hy = 2.0 * dy + 4.0 * (Y_ROAD - c->cam_y) / 240.0 + c->bounce + 60.0;
    }
    if (hy > 0x51 || hy < 0x13) hy = hy < 0x12 ? 0x12 : 0x52;
    return hy;
}

int enh_sky_shift(s16 h, int sh)                            /* sky_scroll's a */
{
    if (h > 0) return (s16)(cmod16(shl16(h, sh), 0x140) & 0xFFF8);
    return sub16(0x140, (s16)(cmod16(shl16(abs16(h), sh), 0x140) & 0xFFF8));
}

int enh_top_band_row(const double y[15], double ground0, double *ground)
{
    int top = FAR_ROWS;
    double g = ground0;
    for (int k = FAR_ROWS; k >= 1; k--)
        if (y[k] < g) { g = y[k]; top = k; }
    *ground = g;
    return top;
}

/* The sky and the horizon strip above hy + 8 (scrolled as sky_scroll does in exact mode, at SKY_SCROLL of it in
 * a smooth view), then the grass from the ground's top row (road_draw_sky's fill from DS_ground_top_y). */
static void draw_sky(const Cam *c, bool town, double hy, double ground)
{
    EnhBuf *b = &enh_buf;
    if (town) {                    /* fixed: sky 1Dh rows from 66h, skyline 10h rows from A3h at 2Fh */
        for (int j = 0; j < b->h; j++) {
            double y = ENH_VIEW_Y0 + (j + 0.5) / b->sa;
            if (y < 0x2F) sheet_row(j, 0x66 + (y - 0x12), 0);
            else if (y < 0x3F) sheet_row(j, 0xA3 + (y - 0x2F), 0);
            else { enh_fill_rows(y - 0.5 / b->sa, ENH_VIEW_Y1, COL_ROAD); break; }
        }
        enh_ground_y = 0x3F;
        return;
    }
    double heading = (s16)trk(TRK_YAW, c->base) + c->hv, sky, strip;
    if (c->exact) {
        s16 h = enh_s16_of(c->heading);
        sky = enh_sky_shift(h, 1);
        strip = enh_sky_shift(h, 2);
    } else {
        sky = 2.0 * SKY_SCROLL * heading;
        strip = 4.0 * SKY_SCROLL * heading;
    }
    for (int j = 0; j < b->h; j++) {
        double y = ENH_VIEW_Y0 + (j + 0.5) / b->sa;
        if (y < hy - 12) sheet_row(j, y + 0xA6 - hy, sky);
        else if (y < hy + 8) sheet_row(j, 0xA3 + (y - (hy - 12)), strip);
        else break;
    }
    if (c->mode != 0x10) enh_fill_rows(ground < ENH_VIEW_Y0 ? ENH_VIEW_Y0 : ground, ENH_VIEW_Y1, COL_GRASS);
    enh_ground_y = ground;
}

/* ---------------------------------------------------------------- road */

/* A wall's or storey's face as the game's quad_fill draws it: x = xa (from ya_top to ya_bot) to xb (yb_top to
 * yb_bot), its last row and column included (poly_fill_pts), unlike the road bands' lines. */
static void face(double xa, double xb, double ya_bot, double yb_bot, double yb_top, double ya_top, u8 col)
{
    double l = xa < xb ? xa : xb, r = (xa < xb ? xb : xa) + 1.0;
    double xs[4] = { xa == l ? l : r, xb == l ? l : r, xb == l ? l : r, xa == l ? l : r };
    double ys[4] = { ya_bot + 1.0, yb_bot + 1.0, yb_top, ya_top };
    enh_fill_poly(xs, ys, 4, col);
}

static void quad(double xa0, double xa1, double ya, double xb0, double xb1, double yb, u8 col)
{
    double xs[4] = { xb0, xb1, xa1, xa0 }, ys[4] = { yb, yb, ya, ya };
    enh_fill_poly(xs, ys, 4, col);
}

/* town_walls_front's extras, drawn once per segment band (near row m = n - 1, far row n) as the game does:
 * where a storey starts, its end face (colour 2) beside the near edge of the façade; where the next block is a
 * crossing (the near row an intersection), the side street's far wall to the screen's edge (colour 2) with a
 * kerb line under it (colour 4). */
static void town_extras(const Cam *c, const Row *near_, const Row *far_)
{
    const Row rm = *near_, rn = *far_;
    int n = rn.n;
    u16 C = trk(TRK_C, n), Cm = trk(TRK_C, n - 1);
    if (C & 4) return;
    double ym = py(c, &rm, Y_ROAD), yem = py(c, &rm, Y_EYE);
    for (int side = 0; side < 2; side++) {
        u16 F = trk(side ? TRK_R : TRK_L, n), Fm = trk(side ? TRK_R : TRK_L, n - 1);
        double xm = px(c, &rm, x_out(c, &rm, side)), xn = px(c, &rn, x_out(c, &rn, side));
        if (Cm & 4) {                                       /* the side street beyond the corner */
            if (!side) {
                enh_fill_rect(0, ym - 1, xm, ym, COL_KERB);
                enh_fill_rect(0, yem, xm, ym - 1, COL_WALL_A);
            } else {
                enh_fill_rect(xm, ym - 1, 320, ym, COL_KERB);
                enh_fill_rect(xm, yem, 320, ym - 1, COL_WALL_A);
            }
        }
        if (!(F & 4) || (Fm & 4)) continue;                 /* not where a storey starts */
        double top = (F & 8) ? 2 * yem - ym : yem - (ym - yem) / 2;
        double depth = xm - xn;                             /* the façade's projected depth, outward */
        if (!side) enh_fill_rect(xm + depth, top, xm, yem, COL_WALL_A);
        else enh_fill_rect(xm, top, xm + depth, yem, COL_WALL_A);
    }
}

/* The centre dash between the near row (a) and the far row (b), x from a0 to a1 and b0 to b1: drawn on the lines
 * where the road's left edge (xla to xlb) is left of 13Fh (road_draw_band_front). */
static void dash_quad(double a0, double a1, double ya, double b0, double b1, double yb, double xla, double xlb)
{
    bool ina = xla < 0x13F, inb = xlb < 0x13F;
    if (!ina && !inb) return;
    if (ina != inb) {                                       /* cut where the left edge crosses 13Fh */
        double t = (0x13F - xla) / (xlb - xla);
        double ym = ya + (yb - ya) * t, m0 = a0 + (b0 - a0) * t, m1 = a1 + (b1 - a1) * t;
        if (ina) { b0 = m0; b1 = m1; yb = ym; } else { a0 = m0; a1 = m1; ya = ym; }
    }
    quad(a0, a1, ya, b0, b1, yb, COL_KERB);
}

/* the band between row ra (near) and rb (far), with the far row's flags: road_draw_band_front, and in town
 * town_walls_front; dash: the centre dash's width (enh_dash_width) */
static void draw_band(const Cam *c, const Row *pa, const Row *pb, bool town, double dash)
{
    const Row ra = *pa, rb = *pb;
    double ya = py(c, &ra, Y_ROAD), yb = py(c, &rb, Y_ROAD);
    int n = rb.n;
    u16 C = trk(TRK_C, n), L = trk(TRK_L, n), R = trk(TRK_R, n);
    bool cross = C & 1;
    /* A crossing's band leaves the ground as it is (the town's road colour, a race's grass); a plain band paints
     * grass across the row after the walls (road_draw_band_front), then the road. */
    if (cross && yb < ya) enh_fill_rows(yb, ya, town ? COL_ROAD : COL_GRASS);

    double la, ra_, lb, rb_;
    edges(trk(TRK_C, ra.n), &la, &ra_);
    edges(trk(TRK_C, rb.n), &lb, &rb_);
    double xla = px(c, &ra, la), xra = px(c, &ra, ra_), xlb = px(c, &rb, lb), xrb = px(c, &rb, rb_);
    if (town) town_extras(c, &ra, &rb);
    if (town && !(C & 4)) {    /* drawn even when the band itself has no rows (the far end of the street) */                                 /* façades and upper storeys (town_walls_front) */
        double yea = py(c, &ra, Y_EYE), yeb = py(c, &rb, Y_EYE);
        for (int side = 0; side < 2; side++) {
            u16 F = side ? R : L;
            double xa = px(c, &ra, x_out(c, &ra, side)), xb = px(c, &rb, x_out(c, &rb, side));
            face(xa, xb, ya, yb, yeb, yea, (F & 0x10) ? COL_WALL_A : COL_WALL_B);
            if (F & 4) {
                double Yt = (F & 8) ? 2 * Y_EYE - Y_ROAD : Y_EYE - (Y_ROAD - Y_EYE) / 2;
                face(xa, xb, yea - 1.0, yeb - 1.0, py(c, &rb, Yt), py(c, &ra, Yt), COL_STOREY);
            }
        }
    }
    if (yb >= ya) return;                                   /* no rows, or back-facing (beyond a crest) */
    if (!cross) {
        enh_fill_rows(yb, ya, COL_GRASS);
        quad(xla, xra, ya, xlb, xrb, yb, (C & 0x10) ? COL_FINISH : COL_ROAD);
    } else {
        if (C & 4) return;                                  /* intersection */
        double xola = px(c, &ra, x_out(c, &ra, 0)), xolb = px(c, &rb, x_out(c, &rb, 0));
        double xora = px(c, &ra, x_out(c, &ra, 1)), xorb = px(c, &rb, x_out(c, &rb, 1));
        if (L & 0x80) {
            double ma = (xola + xla) / 2, mb = (xolb + xlb) / 2;
            quad(xola, ma, ya, xolb, mb, yb, COL_KERB);
            quad(ma, xla, ya, mb, xlb, yb, COL_GRASS);
        } else
            quad(xola, xla, ya, xolb, xlb, yb, COL_KERB);
        if (R & 0x80) {
            double ma = (xora + xra) / 2, mb = (xorb + xrb) / 2;
            quad(xra, ma, ya, xrb, mb, yb, COL_GRASS);
            quad(ma, xora, ya, mb, xorb, yb, COL_KERB);
        } else
            quad(xra, xora, ya, xrb, xorb, yb, COL_KERB);
    }
    if (C & 8) {                    /* centre dash: x from mid - (w >> 1) to mid - (w >> 1) + w, mid = (xl + xr) >> 1 */
        double h = c->exact ? floor(dash / 2) : dash / 2;
        double ca = (xla + xra) / 2, cb = (xlb + xrb) / 2;
        if (c->exact) { ca = floor(ca); cb = floor(cb); }
        dash_quad(ca - h, ca - h + dash + 1, ya, cb - h, cb - h + dash + 1, yb, xla, xlb);
    }
}

/* The camera of a view, for the road and the objects alike. dy = height - HGT(s): the game's height is HGT[seg]
 * (road_advance_segment) plus hoff (the cliff fall); between frames the track's height at pos keeps the view
 * smooth. dx = heading - YAW(s), the heading unwrapped. */
static void cam_of(const EnhView *v, Cam *c)
{
    c->base = segc((int)floor(v->pos));
    c->pos = v->pos;
    c->lat = v->lat;
    c->hpos = v->exact ? v->hoff : interp(c, v->pos, hgt_rel) + v->hoff;
    double hf = floor(v->heading);
    c->hv = (s16)(u16)(enh_s16_of(hf) - (s16)trk(TRK_YAW, c->base)) + (v->heading - hf);
    c->cam_y = v->cam_y;
    c->bounce = v->bounce;
    c->heading = v->heading;
    c->mode = v->mode;
    c->w = v->mode == 0x10 ? 200 : 400;
    c->exact = v->exact;
}

void enh_rows_exact_front(const EnhView *v, EnhRowTab *t)
{
    Cam c;
    cam_of(v, &c);
    c.exact = true;
    for (int k = 0; k <= FAR_ROWS; k++) {
        Row r = row_k(&c, k);
        double xl, xr;
        edges(trk(TRK_C, r.n), &xl, &xr);
        t->xl[k] = (s16)px_i(&c, &r, xl);
        t->xr[k] = (s16)px_i(&c, &r, xr);
        t->xol[k] = (s16)px_i(&c, &r, x_out(&c, &r, 0));
        t->xor_[k] = (s16)px_i(&c, &r, x_out(&c, &r, 1));
        t->y[k] = (s16)py_i(&c, &r, Y_ROAD);
        t->ye[k] = (s16)py_i(&c, &r, Y_EYE);
        t->yk[k] = (s16)py_i(&c, &r, Y_KERB);
        t->ybl[k] = (s16)py_i(&c, &r, sub16(Y_ROAD, (s16)trk(TRK_BL, r.n)));
        t->ybr[k] = (s16)py_i(&c, &r, sub16(Y_ROAD, (s16)trk(TRK_BR, r.n)));
    }
}

void enh_draw_road(const EnhView *v)
{
    enh_buf_setup();
    enh_clip_view();
    Cam c;
    cam_of(v, &c);
    Row rows[FAR_ROWS + 1];
    double ys[FAR_ROWS + 1], hy = horizon_y(&c), ground = 0x3F;
    for (int k = 0; k <= FAR_ROWS; k++) {
        rows[k] = row_k(&c, k);
        ys[k] = py(&c, &rows[k], Y_ROAD) - ROW_TOP;         /* the line y: the integer row in exact mode */
    }
    /* a race hides the bands beyond a crest (road_draw_sky's top_band_row); the town draws them all */
    int top = v->town ? FAR_ROWS : enh_top_band_row(ys, hy + 8, &ground);
    draw_sky(&c, v->town, hy, ground);
    double q = 8.0 * (c.pos - c.base);
    /* far to near, each segment's cuts after its band and the opponent after its row's objects (road_draw_front);
     * cuts beyond the far row first, those at or behind the near row last */
    int base = c.base;
    enh_cuts_at(v, ENH_PASS_FRONT, base + FAR_ROWS + 1, INT_MAX, false);
    enh_cuts_at(v, ENH_PASS_FRONT, base + FAR_ROWS + 1, INT_MAX, true);
    for (int k = FAR_ROWS; k >= 1; k--) {
        if (k <= top) draw_band(&c, &rows[k - 1], &rows[k], v->town, enh_dash_width(k, q, c.exact));
        enh_cuts_at(v, ENH_PASS_FRONT, base + k, base + k, false);
        enh_cuts_at(v, ENH_PASS_FRONT, base + k, base + k, true);
    }
    enh_cuts_at(v, ENH_PASS_FRONT, INT_MIN, base, false);
    enh_cuts_at(v, ENH_PASS_FRONT, INT_MIN, base, true);
}

/* ---------------------------------------------------------------- objects */

EnhObjGeo enh_obj_geo(const EnhView *v, int kind, int seg, int sprite)
{
    EnhObjGeo g = { 0, 0, 0, 0, false };
    if (kind != ENH_CUT_ROW && kind != ENH_CUT_OPP) return g;   /* the mirror's cuts: not placed until the mirror is drawn */
    bool opp = kind == ENH_CUT_OPP;
    Cam c;
    cam_of(v, &c);
    Row r;
    r.n = seg;
    r.k = -1;
    if (opp) {                                              /* the opponent (opponent_draw), at the d it was drawn at */
        double z = c.exact ? cdiv16((s16)v->opp_d, 8) : v->opp_d / 8.0;
        if (sprite == 0) z += 16;
        if (z < -2 && sprite == 2) z += 8;
        r.z = fabs(z) + 4.0;
        if (c.exact) {                                      /* the game's integer segment */
            r.dx = c.hv - yaw_rel(&c, v->opp_seg);
            r.dy = c.hpos - hgt_rel(&c, v->opp_seg);
        } else {
            r.dx = c.hv - interp(&c, v->opp_pos, yaw_rel);
            r.dy = c.hpos - interp(&c, v->opp_pos, hgt_rel);
        }
        if (v->mode == 4) r.dy = 0;
    } else {                                                /* on the road at segment seg */
        int k = seg - c.base;
        if (k >= 0 && k <= FAR_ROWS) r = row_k(&c, k);      /* the row's own depth: row 0 at 4, row 14 at 116 */
        else {
            r.z = 8.0 * (seg - v->pos) + 4.0;
            r.dx = c.hv - yaw_rel(&c, seg);
            r.dy = c.hpos - hgt_rel(&c, seg);
        }
    }
    if (r.z < 1.0) return g;
    g.z = r.z;
    g.yg = py(&c, &r, Y_ROAD);
    g.b = 4.0 / r.z;
    if (opp) {
        g.a = c.exact ? v->opp_sx : px(&c, &r, v->opp_lat);   /* x relative to the opponent's lane */
        if (sprite > 3 && sprite < 9) {                     /* rear views: not above the road row they stand on */
            int row = ((int)r.z + 1) >> 3;
            Row rr = row_k(&c, SDL_min(row, FAR_ROWS));
            double yr = row > FAR_ROWS ? v->ground_top_y + ROW_TOP : py(&c, &rr, Y_ROAD);
            if (g.yg < yr) g.yg = yr;
        }
        /* pictures 2 and 3: the base at most row 100 (the bottom clamp at 65h is then always met) */
        if ((sprite == 2 || sprite == 3) && g.yg > 100 + ROW_TOP) g.yg = 100 + ROW_TOP;
    } else
        g.a = px(&c, &r, 0);
    g.ok = true;
    return g;
}
