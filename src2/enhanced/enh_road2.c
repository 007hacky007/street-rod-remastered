/* Street Rod 2's road view drawn again from the segment table (Street Rod 1's src/enhanced/enh_road.c, with
 * Street Rod 2's projection): the original's 15 rows evaluated in floating point at the smooth position, the
 * bands between them drawn as the original draws them (straight edges from row to row), the sky and the
 * horizon scrolled continuously. At a game frame's own position the result is the original's geometry. Road modes 0 (the
 * town) and 1 (the street drag); the objects, the opponent and the mirror are the game's own until they are
 * drawn here too.
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
#define X_OUT_L      0x44C     /* -(400 - 5DCh): the town's façades, the shoulders' outer edge */
#define X_OUT_R      0xE74     /* 400 + CE4h */
#define SKY_SCROLL   0.125     /* as Street Rod 1's enhanced view: the sky turns at this fraction of the original's */

const u8 enh_road_colours[2] = { COL_ROAD, COL_FINISH };
double enh_ground_y;           /* the first row below the horizon strip (the compare tool starts there) */

typedef struct {
    int base;
    double pos, lat, hv, hpos;
    double cam_y, bounce;
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

typedef struct { double z, dx, dy; int n; } Row;

/* road_project_front's row k: depth 4 for row 0, 12 - q for row 1 and 8 more for each row after, row 14 at a
 * fixed 116; dx and dy from the row's own segment. */
static Row row_k(const Cam *c, int k)
{
    Row r;
    double q = 8.0 * (c->pos - c->base);
    r.n = c->base + k;
    r.z = k == 0 ? 4.0 : k == FAR_ROWS ? 8.0 * FAR_ROWS + 4.0 : 8.0 * k + 4.0 - q;
    r.dx = c->hv - yaw_rel(c, r.n);
    r.dy = c->hpos - hgt_rel(c, r.n);
    return r;
}

/* The original's line y is pixel row y from its top: the drawn shapes sit half a row lower than the projected
 * value, so that a sample at a row's centre meets the edge where the original's line starts. */
#define ROW_TOP 0.5

static double px(const Cam *c, const Row *r, double X)
{
    if (c->exact) {                                         /* road_proj_x */
        int z = (int)r->z, x = (int)X + ((int)(s16)(z * (int)r->dx) >> 2);
        return 160 - ((int)c->lat - x) * 4 / z;
    }
    return 160.0 - 4.0 * (c->lat - (X + r->z * r->dx / 4.0)) / r->z;
}
static double py(const Cam *c, const Row *r, double Y)
{
    if (c->exact) {                                         /* road_proj_y */
        int z = (int)r->z, t = 2 * ((int)(s16)(z * (int)r->dy) & ~1) + 4 * ((int)Y - (int)c->cam_y);
        return t / z + c->bounce + 60 + ROW_TOP;
    }
    return 2.0 * r->dy + 4.0 * (Y - c->cam_y) / r->z + c->bounce + 60.0 + ROW_TOP;
}


/* The centre dash's world width (px * z / 4), from the game's per-row table (DS_dash_width[row]: px at the
 * row's depth): the mean over the rows. */
static double dash_half(void)
{
    double sum = 0;
    for (int k = 1; k < FAR_ROWS; k++) sum += DSS((u16)(DS_dash_width + 2 * k)) * (8.0 * k + 4.0) / 4.0;
    return sum / (FAR_ROWS - 1);
}

/* ---------------------------------------------------------------- sky and horizon */

static u8 src_rows[0xB8][320];     /* page 1 rows 66h..B7h: the sky and horizon sheets */

static void read_sheets(void)
{
    for (int y = 0x66; y < 0xB8; y++) {
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
    if (sy > 0xB7) sy = 0xB7;
    u8 *out = b->px + (size_t)j * b->w;
    for (int i = 0; i < b->w; i++) {
        double x = (i + 0.5) / b->sa - shift;
        int col = (int)floor(x) % 320;
        if (col < 0) col += 320;
        out[i] = src_rows[sy][col];
    }
}

/* road_draw_sky: the horizon at road_proj_y(F0h, 2000, dy) with dy the climb from the segment behind, and
 * its clamp (a horizon above row 13h goes to 12h, one below 51h to 52h). */
static double horizon_y(const Cam *c)
{
    double dy = c->hpos - hgt_rel(c, c->base - 1);
    double hy = 2.0 * dy + 4.0 * (Y_ROAD - c->cam_y) / 240.0 + c->bounce + 60.0;
    if (hy > 0x51 || hy < 0x13) hy = hy < 0x12 ? 0x12 : 0x52;
    return hy;
}

static void draw_sky(const Cam *c, bool town)
{
    EnhBuf *b = &enh_buf;
    read_sheets();
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
    double hy = horizon_y(c);
    double heading = (s16)trk(TRK_YAW, c->base) + c->hv;
    for (int j = 0; j < b->h; j++) {
        double y = ENH_VIEW_Y0 + (j + 0.5) / b->sa;
        if (y < hy - 12) sheet_row(j, y + 0xA6 - hy, 2.0 * SKY_SCROLL * heading);
        else if (y < hy + 8) sheet_row(j, 0xA3 + (y - (hy - 12)), 4.0 * SKY_SCROLL * heading);
        else { enh_fill_rows(y - 0.5 / b->sa, ENH_VIEW_Y1, COL_GRASS); break; }
    }
    enh_ground_y = hy + 8;
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
        double X = side ? X_OUT_R : X_OUT_L, xm = px(c, &rm, X), xn = px(c, &rn, X);
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

/* the band between row ra (near) and rb (far), with the far row's flags: road_draw_band_front, and in town
 * town_walls_front */
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
            double X = side ? X_OUT_R : X_OUT_L, xa = px(c, &ra, X), xb = px(c, &rb, X);
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
        double xola = px(c, &ra, X_OUT_L), xolb = px(c, &rb, X_OUT_L);
        double xora = px(c, &ra, X_OUT_R), xorb = px(c, &rb, X_OUT_R);
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
    if (C & 8) {                                            /* centre dash: the row's table width, centred */
        double ca = (xla + xra) / 2, cb = (xlb + xrb) / 2, wa = dash * 4.0 / ra.z, wb = dash * 4.0 / rb.z;
        quad(ca - wa / 2, ca + wa / 2, ya, cb - wb / 2, cb + wb / 2, yb, COL_KERB);
    }
}

void enh_draw_road(const EnhView *v)
{
    enh_buf_setup();
    enh_clip_view();
    Cam c;
    c.base = segc((int)floor(v->pos));
    c.pos = v->pos;
    c.lat = v->lat;
    /* dy = height - HGT(s): the game's height is HGT[seg] (road_advance_segment); between frames the track's
     * height at pos keeps the view smooth. dx = heading - YAW(s). */
    c.hpos = v->exact ? (s16)(v->height - (s16)trk(TRK_HGT, c.base)) : interp(&c, v->pos, hgt_rel);
    c.hv = (s16)((s16)v->heading - (s16)trk(TRK_YAW, c.base)) + (v->heading - (s16)v->heading);
    c.cam_y = v->cam_y;
    c.bounce = v->bounce;
    c.exact = v->exact;
    draw_sky(&c, v->town);
    double dash = dash_half();
    Row rows[FAR_ROWS + 1];
    for (int k = 0; k <= FAR_ROWS; k++) rows[k] = row_k(&c, k);
    for (int k = FAR_ROWS; k >= 1; k--) draw_band(&c, &rows[k - 1], &rows[k], v->town, dash);   /* far to near */
}

/* ---------------------------------------------------------------- objects */

EnhObjGeo enh_obj_geo(const EnhView *v, int seg, int sprite)
{
    EnhObjGeo g = { 0, 0, 0, 0, false };
    Cam c;
    c.base = segc((int)floor(v->pos));
    c.pos = v->pos;
    c.lat = v->lat;
    c.hpos = interp(&c, v->pos, hgt_rel);
    c.hv = (s16)((s16)v->heading - (s16)trk(TRK_YAW, c.base)) + (v->heading - (s16)v->heading);
    c.cam_y = v->cam_y;
    c.bounce = v->bounce;
    c.exact = false;
    Row r;
    double lat_shift = 0;
    if (seg < 0) {                                          /* the opponent (opponent_draw) */
        double d = 64.0 * (v->opp_pos - v->pos), z = d / 8.0;
        if (sprite == 0) z += 16;
        if (z < -2 && sprite == 2) z += 8;
        r.z = fabs(z) + 4.0;
        r.dx = c.hv - interp(&c, v->opp_pos, yaw_rel);
        r.dy = c.hpos - interp(&c, v->opp_pos, hgt_rel);
        lat_shift = v->opp_lat;                             /* x relative to the opponent's lane */
    } else {                                                /* on the road at segment seg */
        r.z = 8.0 * (seg - v->pos) + 4.0;
        r.dx = c.hv - yaw_rel(&c, seg);
        r.dy = c.hpos - hgt_rel(&c, seg);
    }
    if (r.z < 1.0) return g;
    g.z = r.z;
    g.yg = py(&c, &r, Y_ROAD);
    g.b = 4.0 / r.z;
    g.a = px(&c, &r, lat_shift);
    g.ok = true;
    return g;
}
