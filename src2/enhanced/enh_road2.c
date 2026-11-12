/* Street Rod 2's road view drawn again from the segment table (Street Rod 1's src/enhanced/enh_road.c, with
 * Street Rod 2's projection): the original's 15 rows evaluated in floating point at the smooth position, the
 * bands between them drawn as the original draws them (straight edges from row to row), the sky and the
 * horizon scrolled as sky_scroll does in exact mode, continuously in a smooth view. At a game frame's own
 * position the result is the original's geometry. Road modes 0 (the town), 1 (the street drag), 2 and 4 (the
 * Grudge drag strip: its own flags, flat, the wide dash at the start and the barrier walls) and 10h (Mulholland:
 * its backdrop and hillsides from enh_hills2.c, the shoulders beside the banks, the grass on the flat sides, the
 * row-0 dx clamp); the objects and the opponent the game drew are drawn again in the band loop at their own
 * segment (enh_cuts_at); the mirror is drawn after it (enh_mirror2.c). Road mode 8 (the Aqueduct) and its mirror
 * are enh_aq2.c's.
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
#include "enhanced/enh_geo2.h"
#include "enhanced/enh_aq2.h"
#include "remaster/rm_pack.h"
#include "remaster/rm_tags.h"

#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "game/race_int.h"
#include "mem.h"
#include "platform/ega.h"

const u8 enh_road_colours[2] = { COL_ROAD, COL_FINISH };
double enh_ground_y;           /* the first row below the horizon strip (the compare tool starts there) */

u16 enh_track(int mode, u16 arr, int i)                     /* road_set_seg_ptrs */
{
    if (!(mode & 6)) return TRKW(arr, segc(i));
    if (arr == TRK_C) return TRKW(TRK_C2, i);
    if (arr == TRK_L) return TRKW(TRK_L2, i);
    if (arr == TRK_R) return TRKW(TRK_R2, i);
    return TRKW(arr, i + 0xC0);
}

int enh_wall_colour(int phase, int n, int row, int seg)    /* road_draw_barrier_walls */
{
    return abs(n - row - seg + phase) % 10 < 5 ? 0x0B : 4;
}

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
    if (k == 0 && c->mode == 0x10) r.dx = SDL_clamp(r.dx, -0x30, 0x30);   /* Mulholland's row-0 clamp */
    r.dy = c->hpos - hgt_rel(c, r.n);
    return r;
}

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

double enh_dash_width(int k, double q, bool exact)
{
    double t = DSS((u16)(DS_dash_width + 2 * k));
    if (exact) return t;
    double tn = DSS((u16)(DS_dash_width + 2 * (k - 1)));
    return t + (tn - t) * q / 8.0;
}

/* ---------------------------------------------------------------- sky and horizon */

static u8 src_rows[0xC8][320];     /* page 1 rows 66h..C7h: the sky and horizon sheets */
static RmTag src_tags[0xC8][320];  /* ENH: their remaster tags (verified), 0 = original */

void enh_sheets_read(void)
{
    enh_flush();                   /* recorded sheet rows read the tables rewritten here */
    bool rm = rm_pack_active();
    for (int y = 0x66; y < 0xC8; y++) {
        u16 o = (u16)(0x2000 + y * EGA_BYTES_PER_LINE);
        for (int x = 0; x < 320; x++) {
            u16 b = (u16)(o + (x >> 3));
            int bit = 7 - (x & 7), c = 0;
            for (int k = 0; k < 4; k++) c |= (ega_plane(k)[b] >> bit & 1) << k;
            src_rows[y][x] = (u8)c;
            RmTag t = rm ? rm_vtag(b, bit) : 0;
            RmPic *p = t ? rm_pic(rm_tag_slot(t)) : NULL;
            if (p && (rm_tag_u(t) >= p->w || rm_tag_v(t) >= p->h || p->orig[rm_tag_v(t) * p->w + rm_tag_u(t)] != c)) p = NULL;
            src_tags[y][x] = p ? t : 0;
        }
    }
}

/* one sample row from sheet row syf (fraction: the row within the remastered art), scrolled so that screen
 * x shows sheet column x - shift */
typedef struct { int j; double syf, shift; } SheetRow;
static void sheet_row_run(const EnhCtx *k, const void *arg)
{
    const SheetRow *q = arg;
    int j = q->j;
    double syf = q->syf, shift = q->shift;
    if (j < k->by0 || j >= k->by1) return;          /* the whole row, whatever the clip */
    const u32 *cur_rgb = k->rgb;
    EnhBuf *b = &enh_buf;
    int sy = (int)floor(syf), fv = (int)((syf - floor(syf)) * RM_ART_SCALE);
    if (sy < 0x66) sy = 0x66;
    if (sy > 0xC7) sy = 0xC7;
    u8 *out = b->px + (size_t)j * b->w;
    u32 *orgb = b->rgb && cur_rgb ? b->rgb + (size_t)j * b->w : NULL;   /* ENH: remastered art */
    int st_mode = orgb ? rm_selftest() : 0;
    unsigned long st_n = 0, st_bad = 0;            /* the self-test's counts, added once */
    for (int i = 0; i < b->w; i++) {
        double x = (i + 0.5) / b->sa - shift;
        int col = (int)floor(x) % 320;
        if (col < 0) col += 320;
        out[i] = src_rows[sy][col];
        RmTag t = orgb ? src_tags[sy][col] : 0;
        RmPic *p = t ? rm_pic_ready(rm_tag_slot(t)) : NULL;
        if (!p) continue;
        int a, fu = (int)((x - floor(x)) * RM_ART_SCALE);
        u32 c = rm_texel(p, rm_tag_u(t) * RM_ART_SCALE + fu, rm_tag_v(t) * RM_ART_SCALE + fv, cur_rgb, &a);
        if (st_mode) { st_n++; st_bad += a < 128 || c != cur_rgb[src_rows[sy][col]]; }
        if (a < 128) continue;
        if (st_mode == 2) c = (c >> 1 & 0x7F7F7F) + 0x400060;
        out[i] = ENH_RGB;
        orgb[i] = c;
    }
    if (st_mode) rm_selftest_race_n(st_n, st_bad);
}

void enh_sheet_row(int j, double syf, double shift)
{
    SheetRow a = { j, syf, shift };
    enh_submit(sheet_row_run, &a, sizeof a);
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
            if (y < 0x2F) enh_sheet_row(j, 0x66 + (y - 0x12), 0);
            else if (y < 0x3F) enh_sheet_row(j, 0xA3 + (y - 0x2F), 0);
            else { enh_fill_rows(y - 0.5 / b->sa, ENH_VIEW_Y1, COL_ROAD); break; }
        }
        enh_ground_y = 0x3F;
        return;
    }
    double heading = (s16)trk(c, TRK_YAW, c->base) + c->hv, sky, strip;
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
        if (y < hy - 12) enh_sheet_row(j, y + 0xA6 - hy, sky);
        else if (y < hy + 8) enh_sheet_row(j, 0xA3 + (y - (hy - 12)), strip);
        else break;
    }
    enh_fill_rows(ground < ENH_VIEW_Y0 ? ENH_VIEW_Y0 : ground, ENH_VIEW_Y1, COL_GRASS);
    enh_ground_y = ground;
}

/* ---------------------------------------------------------------- road */

void enh_town_face_clip(int side, int xn, int xf, int yn, int yf, int ytf, int ytn, u8 col, int x0, int y0, int x1, int y1)
{
    /* quad_fill's points in town_walls_front's (and town_walls_mirror's) order: left from the near row, right
     * from the far one */
    int pts[8] = { xn, yn, xf, yf, xf, ytf, xn, ytn };
    if (side) { int r[8] = { xf, yf, xn, yn, xn, ytn, xf, ytf }; memcpy(pts, r, sizeof pts); }
    s16 in[8], out[128];
    double ox[64], oy[64];
    for (int i = 0; i < 8; i++) in[i] = (s16)pts[i];
    int n = enh_clip_poly_i(in, 4, out, x0, x1, y0, y1);
    for (int i = 0; i < n; i++) { ox[i] = out[2 * i]; oy[i] = out[2 * i + 1] + ROW_TOP; }
    enh_fill_poly_incl(ox, oy, n, col);
}

void enh_town_face_exact(int side, int xn, int xf, int yn, int yf, int ytf, int ytn, u8 col)
{
    enh_town_face_clip(side, xn, xf, yn, yf, ytf, ytn, col, 0, ENH_VIEW_Y0, 0x13F, 0x65);
}

/* town_walls_front's extras, drawn once per segment band (near row m = n - 1, far row n) as the game does:
 * where a storey starts, its end face (colour 2) beside the near edge of the façade; where the next block is a
 * crossing (the near row an intersection), the side street's far wall to the screen's edge (colour 2) with a
 * kerb line under it (colour 4). */
static void town_extras(const Cam *c, const Row *near_, const Row *far_)
{
    const Row rm = *near_, rn = *far_;
    int n = rn.n;
    u16 C = trk(c, TRK_C, n), Cm = trk(c, TRK_C, n - 1);
    if (C & 4) return;
    double ym = py(c, &rm, Y_ROAD), yem = py(c, &rm, Y_EYE);
    for (int side = 0; side < 2; side++) {
        u16 F = trk(c, side ? TRK_R : TRK_L, n), Fm = trk(c, side ? TRK_R : TRK_L, n - 1);
        double xm = px(c, &rm, x_out(c, &rm, side)), xn = px(c, &rn, x_out(c, &rn, side));
        if ((Cm & 4) && (side ? hspan_shown(xm, 0x13F, 0, 0x13F) : hspan_shown(0, xm, 0, 0x13F))) {
            if (!side) {                                    /* the side street beyond the corner */
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
        /* the game's span 2a - b .. a - 1 (left) or b + 1 .. 2b - a (right): reversed where row 0's outer edge
         * quirk puts the near edge beyond the far one, and then most often rejected */
        bool shown = side ? hspan_shown(xm + 1, xm + depth, 0, 0x13F) : hspan_shown(xm + depth, xm - 1, 0, 0x13F);
        if (!shown) continue;
        if (!side) enh_fill_rect(xm + depth, top, xm, yem, COL_WALL_A);
        else enh_fill_rect(xm, top, xm + depth, yem, COL_WALL_A);
    }
}

/* The centre dash between the near row (a) and the far row (b), x from a0 to a1 and b0 to b1: drawn on the lines
 * where the road's left edge (xla to xlb) is left of 13Fh (road_draw_band_front). */
static void dash_quad(double a0, double a1, double ya, double b0, double b1, double yb, double xla, double xlb, u8 col)
{
    bool ina = xla < 0x13F, inb = xlb < 0x13F;
    if (!ina && !inb) return;
    if (ina != inb) {                                       /* cut where the left edge crosses 13Fh */
        double t = (0x13F - xla) / (xlb - xla);
        double ym = ya + (yb - ya) * t, m0 = a0 + (b0 - a0) * t, m1 = a1 + (b1 - a1) * t;
        if (ina) { b0 = m0; b1 = m1; yb = ym; } else { a0 = m0; a1 = m1; ya = ym; }
    }
    quad(a0, a1, ya, b0, b1, yb, col);
}

static s16 slope16(s16 xa, s16 ya, s16 xb, s16 yb) { return yb == ya ? 0 : cdiv16(shl16(sub16(xb, xa), 4), sub16(yb, ya)); }

void enh_dash_exact(int xlq, int xrq, int yq, int xlp, int xrp, int yp, int w, u8 col)
{
    s16 xl = (s16)xlp, xr = (s16)xrp, axl = shl16(xl, 4), axr = shl16(xr, 4);
    s16 sl = slope16((s16)xlq, (s16)yq, xl, (s16)yp), sr = slope16((s16)xrq, (s16)yq, xr, (s16)yp);
    for (int y = yp, n = yq - yp; n > 0 && y < 0x66; y++, n--) {
        s16 dx = sub16((s16)(add16(xl, xr) >> 1), (s16)(w >> 1));
        if (xl < 0x13F) enh_fill_rect(dx, y, dx + w + 1, y + 1, col);
        axl = add16(axl, sl); axr = add16(axr, sr);
        xl = (s16)(axl >> 4); xr = (s16)(axr >> 4);
    }
}

/* The centre dash of the band of far segment n: x from mid - (w >> 1) to mid - (w >> 1) + w, mid = (xl + xr) >> 1;
 * on the strip by the start 8 times as wide, colour 2. */
static void centre_dash(const Cam *c, int n, double dash, double xla, double xra, double ya, double xlb, double xrb, double yb)
{
    u8 col = COL_KERB;
    if (on_strip(c) && n < c->start_seg + 5) { dash *= 8; col = COL_START; }   /* the start box: 8 wide */
    if (c->exact) {
        enh_dash_exact((int)xla, (int)xra, (int)floor(ya), (int)xlb, (int)xrb, (int)floor(yb), (int)dash, col);
        return;
    }
    double h = dash / 2, ca = (xla + xra) / 2, cb = (xlb + xrb) / 2;
    dash_quad(ca - h, ca - h + dash + 1, ya, cb - h, cb - h + dash + 1, yb, xla, xlb, col);
}

/* the band between row ra (near) and rb (far), with the far row's flags: road_draw_band_front, and in town
 * town_walls_front; dash: the centre dash's width (enh_dash_width) */
static void draw_band(const Cam *c, const Row *pa, const Row *pb, bool town, double dash)
{
    const Row ra = *pa, rb = *pb;
    double ya = py(c, &ra, Y_ROAD), yb = py(c, &rb, Y_ROAD);
    int n = rb.n;
    u16 C = trk(c, TRK_C, n), L = trk(c, TRK_L, n), R = trk(c, TRK_R, n);
    bool cross = C & 1;
    /* A crossing's band leaves the ground as it is (the town's road colour, a race's grass); a plain band paints
     * grass across the row after the walls (road_draw_band_front), then the road. */
    if (cross && yb < ya) enh_fill_rows(yb, ya, town ? COL_ROAD : COL_GRASS);

    double la, ra_, lb, rb_;
    edges(trk(c, TRK_C, ra.n), &la, &ra_);
    edges(trk(c, TRK_C, rb.n), &lb, &rb_);
    double xla = px(c, &ra, la), xra = px(c, &ra, ra_), xlb = px(c, &rb, lb), xrb = px(c, &rb, rb_);
    if (town) town_extras(c, &ra, &rb);
    if (town && !(C & 4)) {    /* drawn even when the band itself has no rows (the far end of the street) */                                 /* façades and upper storeys (town_walls_front) */
        double yea = py(c, &ra, Y_EYE), yeb = py(c, &rb, Y_EYE);
        for (int side = 0; side < 2; side++) {
            u16 F = side ? R : L;
            double xa = px(c, &ra, x_out(c, &ra, side)), xb = px(c, &rb, x_out(c, &rb, side));
            if (c->exact) {                                 /* the game's integer quad_fill */
                int yn = py_i(c, &ra, Y_ROAD), yf = py_i(c, &rb, Y_ROAD), en = py_i(c, &ra, Y_EYE), ef = py_i(c, &rb, Y_EYE);
                enh_town_face_exact(side, (int)xa, (int)xb, yn, yf, ef, en, (F & 0x10) ? COL_WALL_A : COL_WALL_B);
                if (F & 4) {
                    s16 k = (F & 8) ? 1 : 2;
                    enh_town_face_exact(side, (int)xa, (int)xb, en, ef, add16(cdiv16(sub16(ef, yf), k), ef),
                                        add16(cdiv16(sub16(en, yn), k), en), COL_STOREY);
                }
                continue;
            }
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
    if (C & 8) centre_dash(c, n, dash, xla, xra, ya, xlb, xrb, yb);
}

/* Mulholland's band (road_draw_band_front, mode 10h) between row ra (near) and rb (far), the far row's flags: the
 * shoulders (colour 2, xol..xl and xr..xor) beside the banks while the car has not fallen below the road, the
 * grass (colour 9) over them on a flat side (the near row's bank at the road's height), to the screen's edge or,
 * in rows 10..14 of a frame where a bridge ends far ahead (l16), ext px out; then the road and the dash. Beside a
 * bank or a bridge the hillsides and the backdrop show. */
static void hill_band(const Cam *c, const Row *pa, const Row *pb, bool l16, double dash)
{
    const Row ra = *pa, rb = *pb;
    double ya = py(c, &ra, Y_ROAD), yb = py(c, &rb, Y_ROAD);
    if (yb >= ya) return;
    int n = rb.n, row = rb.k;
    u16 C = trk(c, TRK_C, n);
    double la, ra_, lb, rb_;
    edges(trk(c, TRK_C, ra.n), &la, &ra_);
    edges(trk(c, TRK_C, rb.n), &lb, &rb_);
    double xla = px(c, &ra, la), xra = px(c, &ra, ra_), xlb = px(c, &rb, lb), xrb = px(c, &rb, rb_);
    if (!(C & 0x80) && c->hoff >= -0x0B) {
        quad(px(c, &ra, x_out(c, &ra, 0)), xla, ya, px(c, &rb, x_out(c, &rb, 0)), xlb, yb, COL_SHOULDER);
        quad(xra, px(c, &ra, x_out(c, &ra, 1)), ya, xrb, px(c, &rb, x_out(c, &rb, 1)), yb, COL_SHOULDER);
    }
    bool ext = l16 && row >= 10;
    double e = ext ? DSS((u16)(DS_hill_peak_tab + 4 + 4 * (16 - row))) : 0;
    for (int side = 0; side < 2; side++) {
        u16 bank = trk(c, side ? TRK_BR : TRK_BL, ra.n);
        bool flat = c->exact ? py_i(c, &ra, sub16(Y_ROAD, (s16)bank)) == py_i(c, &ra, Y_ROAD) : bank == 0;
        if (!flat) continue;
        if (!side) quad(ext ? xla - e : 0, xla, ya, ext ? xlb - e : 0, xlb, yb, COL_GRASS);
        else quad(xra, ext ? xra + e : 320, ya, xrb, ext ? xrb + e : 320, yb, COL_GRASS);
    }
    bool white = (C & 0x10) && c->base >= 0x3AD;            /* the finish, once the car is near it */
    quad(xla, xra, ya, xlb, xrb, yb, white ? COL_FINISH : COL_ROAD);
    if (C & 8) centre_dash(c, n, dash, xla, xra, ya, xlb, xrb, yb);
}

/* road_draw_barrier_walls on the strip, beside the band of row p (far) and row q (near): on each side, right then
 * left, the quad from the kerb's height down to the road at the road's edge, (X_p, YK_p), (X_p, Y_p), (X_q, Y_q),
 * (X_q, YK_q); clip_set_view's clip in exact mode. */
static void barrier_walls(const Cam *c, const Row *q, const Row *p, u8 col)
{
    double lp, rp, lq, rq;
    edges(trk(c, TRK_C, p->n), &lp, &rp);
    edges(trk(c, TRK_C, q->n), &lq, &rq);
    for (int side = 1; side >= 0; side--) {
        double xp = px(c, p, side ? rp : lp), xq = px(c, q, side ? rq : lq);
        double x[4] = { xp, xp, xq, xq };
        double y[4] = { py(c, p, Y_KERB), py(c, p, Y_ROAD), py(c, q, Y_ROAD), py(c, q, Y_KERB) };
        poly_incl(c, x, y, 4, col, 0, 0x13F, ENH_VIEW_Y0, 0x65);
    }
}

void enh_rows_exact_front(const EnhView *v, EnhRowTab *t)
{
    Cam c;
    cam_of(v, &c);
    c.exact = true;
    for (int k = 0; k <= FAR_ROWS; k++) {
        Row r = row_k(&c, k);
        double xl, xr;
        edges(trk(&c, TRK_C, r.n), &xl, &xr);
        t->xl[k] = (s16)px_i(&c, &r, xl);
        t->xr[k] = (s16)px_i(&c, &r, xr);
        t->xol[k] = (s16)px_i(&c, &r, x_out(&c, &r, 0));
        t->xor_[k] = (s16)px_i(&c, &r, x_out(&c, &r, 1));
        t->y[k] = (s16)py_i(&c, &r, Y_ROAD);
        t->ye[k] = (s16)py_i(&c, &r, Y_EYE);
        t->yk[k] = (s16)py_i(&c, &r, Y_KERB);
        t->ybl[k] = (s16)py_i(&c, &r, sub16(Y_ROAD, (s16)trk(&c, TRK_BL, r.n)));
        t->ybr[k] = (s16)py_i(&c, &r, sub16(Y_ROAD, (s16)trk(&c, TRK_BR, r.n)));
    }
}

void enh_draw_road(const EnhView *v)
{
    enh_buf_setup();
    enh_clip_view();
    Cam c;
    cam_of(v, &c);
    bool hills = c.mode == 0x10;
    Row rows[FAR_ROWS + 1];
    double ys[FAR_ROWS + 1], hy = hills ? enh_mulholland_horizon(&c) : horizon_y(&c), ground = 0x3F;
    for (int k = 0; k <= FAR_ROWS; k++) {
        rows[k] = row_k(&c, k);
        ys[k] = py(&c, &rows[k], Y_ROAD) - ROW_TOP;         /* the line y: the integer row in exact mode */
    }
    /* a race hides the bands beyond a crest (road_draw_sky's top_band_row); the town draws them all */
    int top = v->town ? FAR_ROWS : enh_top_band_row(ys, hy + 8, &ground);
    int base = c.base;
    if (hills) { enh_draw_mulholland_sky(&c, v); enh_ground_y = ground; }
    else draw_sky(&c, v->town, hy, ground);
    /* Mulholland: a bridge ends in rows 14..10 (road_draw_front's L16, one flag for the frame) */
    bool l16 = false;
    for (int k = FAR_ROWS; k > 9 && hills; k--)
        if (!(trk(&c, TRK_C, base + k) & 0x80) && (trk(&c, TRK_C, base + k - 1) & 0x80)) l16 = true;
    double q = 8.0 * (c.pos - c.base);
    int phase = enh_rail_phase(&c, v->rail_phase);         /* at the view's base, else the frame's */
    /* far to near, each segment's cuts after its band and the opponent after its row's objects, then the strip's walls
     * (road_draw_front); cuts beyond the far row first, those at or behind the near row last */
    enh_cuts_at(v, ENH_PASS_FRONT, base + FAR_ROWS + 1, INT_MAX, false);
    enh_cuts_at(v, ENH_PASS_FRONT, base + FAR_ROWS + 1, INT_MAX, true);
    for (int k = FAR_ROWS; k >= 1; k--) {
        /* Mulholland's hillsides beside a row off the bridges, whatever top_band_row */
        if (hills && !(trk(&c, TRK_C, base + k) & 0x80)) enh_front_hillsides(&c, rows, k, l16, c.fall_dir);
        if (k <= top) {
            if (hills) hill_band(&c, &rows[k - 1], &rows[k], l16, enh_dash_width(k, q, c.exact));
            else draw_band(&c, &rows[k - 1], &rows[k], v->town, enh_dash_width(k, q, c.exact));
        }
        enh_cuts_at(v, ENH_PASS_FRONT, base + k, base + k, false);
        if (hills) enh_extras_front(&c, rows, k, phase);   /* the rails and trusses, whatever top_band_row */
        enh_cuts_at(v, ENH_PASS_FRONT, base + k, base + k, true);
        if (on_strip(&c))          /* whatever top_band_row */
            barrier_walls(&c, &rows[k - 1], &rows[k], (u8)enh_wall_colour(c.start_seg, FAR_ROWS, k, base));
    }
    enh_cuts_at(v, ENH_PASS_FRONT, INT_MIN, base, false);
    enh_cuts_at(v, ENH_PASS_FRONT, INT_MIN, base, true);
}

/* ---------------------------------------------------------------- objects */

EnhObjGeo enh_obj_geo(const EnhView *v, int kind, int seg, int sprite)
{
    EnhObjGeo g = { 0, 0, 0, 0, false, false, 0 };
    if (v->mode == 8) return enh_obj_geo_aq(v, kind, seg, sprite);   /* the Aqueduct's, its mirror's kinds included */
    if (kind == ENH_CUT_MIR || kind == ENH_CUT_STARTER || (kind == ENH_CUT_OPP && sprite >= 9))
        return enh_obj_geo_mirror(v, kind, seg, sprite);
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
