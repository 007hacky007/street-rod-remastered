/* Street Rod 2's rear-view mirror drawn again (road_project_mirror, road_draw_mirror in src2/game/race_road.c), in
 * its glass F2h..13Dh x 16h..2Ah; the frame and the mount stay the game's pixels (enh_mirror_end). The original's
 * quirks are kept in the exact and the smooth view alike, so that its picture stays the original's:
 *     row k (0..7) at segment base - k, depth 8 for row 0, 8k + q for rows 1..4, 40, 48, 56 for rows 5..7 (the
 *     k == 5 `z -= q` freezes them within a segment)
 *     dx = YAW(s) - heading (in Mulholland row 0's beyond +-3Ch becomes +-46h), dy = HGT(s) - height (0 on the strip)
 *     sx = 118h - (cam_x - (X + z dx / 4)) / z          (quarter scale, no flip)
 *     sy = dy + (Y - cam_y) / z + 24h                   (no bounce)
 *     a band's flags one segment off (base - r - 1), the banks read forward (base + k), YK written twice per row
 * The cuts of the mirror (its rows' objects, the starter, the opponent behind) are drawn in the band loop at their
 * own segment, far to near. */
#include "enhanced/enh_geo2.h"

#include <limits.h>

#include "platform/ega.h"

#define MIR_X0 0xF2            /* the glass, right and bottom exclusive */
#define MIR_Y0 0x16
#define MIR_X1 0x13E
#define MIR_Y1 0x2B
#define MIR_ROWS 7
#define STARTER_PIN_Y 0x27     /* starter_draw_mirror: the figure's bottom, whatever the row's y */

double enh_mirror_z(int k, double q)
{
    if (k <= 0) return 8.0;
    if (k <= 4) return 8.0 * k + q;
    return 8.0 * k;
}

int enh_proj_x_mirror_i(int z, int X, int dx, int cx)
{
    s16 x = add16((s16)X, (s16)((s16)(z * dx) >> 2));
    s16 t = sub16((s16)cx, x);
    if (z == 0) z = 1;
    return sub16(0x118, cdiv16(t, (s16)z));
}

int enh_proj_y_mirror_i(int z, int Y, int dy, int cam_y)
{
    s16 t = sub16(add16((s16)Y, (s16)(z * dy)), (s16)cam_y);
    if (z == 0) z = 1;
    return add16(cdiv16(t, (s16)z), 0x24);
}

/* Mirror row k at segment base - k. */
static Row mrow(const Cam *c, int k)
{
    Row r;
    r.n = c->base - k;
    r.k = k;
    r.z = enh_mirror_z(k, 8.0 * (c->pos - c->base));
    r.dx = yaw_rel(c, r.n) - c->hv;
    if (c->exact) r.dx = i16(r.dx);
    if (k == 0 && (c->mode & 0x10)) { if (r.dx > 0x3C) r.dx = 0x46; else if (r.dx < -0x3C) r.dx = -0x46; }
    r.dy = (c->mode & 6) ? 0 : hgt_rel(c, r.n) - c->hpos;
    return r;
}

static int mpx_i(const Cam *c, const Row *r, double X) { return enh_proj_x_mirror_i((int)r->z, (int)X, i16(r->dx), (int)c->lat); }
static int mpy_i(const Cam *c, const Row *r, double Y) { return enh_proj_y_mirror_i((int)r->z, (int)Y, i16(r->dy), (int)c->cam_y); }

static double mpx(const Cam *c, const Row *r, double X)
{
    if (c->exact) return mpx_i(c, r, X);
    return 0x118 - (c->lat - (X + r->z * r->dx / 4.0)) / r->z;
}
static double mpy(const Cam *c, const Row *r, double Y)
{
    if (c->exact) return mpy_i(c, r, Y) + ROW_TOP;
    return r->dy + (Y - c->cam_y) / r->z + 0x24 + ROW_TOP;
}

/* the outer edges (road_project_mirror: no row-0 quirk) */
static double mx_out(const Cam *c, int side) { return side ? c->w + 0xCE4 : 0x5DC - c->w; }

void enh_rows_exact_mirror(const EnhView *v, EnhRowTab *t)
{
    Cam c;
    cam_of(v, &c);
    c.exact = true;
    for (int k = 0; k <= MIR_ROWS; k++) {
        Row r = mrow(&c, k);
        double xl, xr;
        edges(trk(TRK_C, r.n), &xl, &xr);
        t->xl[k] = (s16)mpx_i(&c, &r, xl);
        t->xr[k] = (s16)mpx_i(&c, &r, xr);
        t->y[k] = (s16)mpy_i(&c, &r, Y_ROAD);
        s16 yk = (s16)mpy_i(&c, &r, Y_KERB);
        if (k == 0) t->yk[0] = yk;
        else t->yk[2 * k - 1] = t->yk[2 * k] = yk;
        t->xol[k] = (s16)mpx_i(&c, &r, mx_out(&c, 0));
        t->xor_[k] = (s16)mpx_i(&c, &r, mx_out(&c, 1));
        t->ye[k] = (s16)mpy_i(&c, &r, Y_EYE);
        t->ybl[k] = (s16)mpy_i(&c, &r, sub16(Y_ROAD, (s16)trk(TRK_BL, c.base + k)));   /* forward (sic) */
        t->ybr[k] = (s16)mpy_i(&c, &r, sub16(Y_ROAD, (s16)trk(TRK_BR, c.base + k)));
    }
}

/* ---------------------------------------------------------------- objects */

EnhObjGeo enh_obj_geo_mirror(const EnhView *v, int kind, int seg, int sprite)
{
    EnhObjGeo g = { 0, 0, 0, 0, false };
    (void)sprite;
    Cam c;
    cam_of(v, &c);
    Row r;
    if (kind == ENH_CUT_OPP) {                              /* opponent_draw's mirror branch, at the d it was drawn at */
        r.n = v->opp_seg;
        r.k = -1;
        if (c.exact) {
            int z = -cdiv16((s16)v->opp_d, 8);
            if (z == 0) z = 1;                              /* road_proj_*_mirror */
            r.z = z;
            r.dx = i16(10.0 + yaw_rel(&c, v->opp_seg) - c.hv);
            r.dy = hgt_rel(&c, v->opp_seg) - c.hpos;
        } else {
            if (v->opp_d >= 8) return g;
            r.z = SDL_max(-v->opp_d / 8.0, 1.0);
            r.dx = 10.0 + interp(&c, v->opp_pos, yaw_rel) - c.hv;
            r.dy = interp(&c, v->opp_pos, hgt_rel) - c.hpos;
        }
        if (v->mode == 4) r.dy = 0;
        if (r.z < 1.0) return g;
        g.a = c.exact ? v->opp_sx : mpx(&c, &r, v->opp_lat);
    } else {                                                /* a row's object, or the starter, at segment seg */
        int k = c.base - seg;
        if (k < 0) return g;
        if (k <= MIR_ROWS) r = mrow(&c, k);
        else {                                              /* beyond the rows: the frozen rows' 8k */
            r.n = seg;
            r.k = -1;
            r.z = 8.0 * k;
            r.dx = yaw_rel(&c, seg) - c.hv;
            r.dy = (c.mode & 6) ? 0 : hgt_rel(&c, seg) - c.hpos;
        }
        g.a = mpx(&c, &r, 0);
    }
    g.z = r.z;
    g.b = 1.0 / r.z;
    g.yg = kind == ENH_CUT_STARTER ? STARTER_PIN_Y : mpy(&c, &r, Y_ROAD);
    g.ok = true;
    return g;
}

/* ---------------------------------------------------------------- walls and bands */

/* town_walls_mirror (keyed on the crossing flag, not on the town): the facade, and its storey (half on the left,
 * double on the right), the side street when the next segment is a crossing, a storey's end face where the next
 * segment has none. near: row r - 1, far: row r, fs: the band's flag segment. */
static void town_walls(const Cam *c, const Row *near_, const Row *far_, int fs)
{
    u16 C = trk(TRK_C, fs), Cn = trk(TRK_C, fs + 1);
    if (!(C & 1) || (C & 4)) return;
    const Row ra = *near_, rb = *far_;
    double ya = mpy(c, &ra, Y_ROAD), yb = mpy(c, &rb, Y_ROAD), yea = mpy(c, &ra, Y_EYE), yeb = mpy(c, &rb, Y_EYE);
    for (int side = 0; side < 2; side++) {
        u16 F = trk(side ? TRK_R : TRK_L, fs), Fn = trk(side ? TRK_R : TRK_L, fs + 1);
        double xa = mpx(c, &ra, mx_out(c, side)), xb = mpx(c, &rb, mx_out(c, side));
        u8 col = (F & 0x10) ? COL_WALL_A : COL_WALL_B;
        if (c->exact) {                                     /* the game's integer quad_fill */
            int yn = mpy_i(c, &ra, Y_ROAD), yf = mpy_i(c, &rb, Y_ROAD), en = mpy_i(c, &ra, Y_EYE), ef = mpy_i(c, &rb, Y_EYE);
            enh_town_face_clip(side, (int)xa, (int)xb, yn, yf, ef, en, col, 0xF0, MIR_Y0, 0x13F, MIR_Y1 - 1);
            if (F & 4) {
                s16 k = side ? 1 : 2;
                enh_town_face_clip(side, (int)xa, (int)xb, en, ef, add16(cdiv16(sub16(ef, yf), k), ef),
                                   add16(cdiv16(sub16(en, yn), k), en), COL_STOREY, 0xF0, MIR_Y0, 0x13F, MIR_Y1 - 1);
            }
        } else {
            face(xa, xb, ya, yb, yeb, yea, col);
            if (F & 4) {
                double Yt = side ? 2 * Y_EYE - Y_ROAD : Y_EYE - (Y_ROAD - Y_EYE) / 2;
                face(xa, xb, yea - 1.0, yeb - 1.0, mpy(c, &rb, Yt), mpy(c, &ra, Yt), COL_STOREY);
            }
        }
        if (Cn & 4) {                                       /* the side street beyond the corner, to the glass's edge */
            if (!side) {
                enh_fill_rect(0, ya - 1, xa + 1, ya, COL_KERB);
                enh_fill_rect(0, yea, xa + 1, ya - 1, COL_WALL_A);
            } else {
                enh_fill_rect(xa, ya - 1, 320, ya, COL_KERB);
                enh_fill_rect(xa, yea, 320, ya - 1, COL_WALL_A);
            }
        }
        if (!(F & 4) || (Fn & 4)) continue;                 /* not where a storey starts */
        double top = side ? 2 * yea - ya : yea - (ya - yea) / 2;
        double depth = xa - xb;                             /* the facade's projected depth, outward */
        if (!side) enh_fill_rect(xa + depth, top, xa, yea, COL_WALL_A);
        else enh_fill_rect(xa + 1, top, xa + depth + 1, yea, COL_WALL_A);
    }
}

/* a + ((b - a) >> 1), in the game's integers in exact mode */
static double mid(const Cam *c, double a, double b) { return c->exact ? a + floor((b - a) / 2) : (a + b) / 2; }

/* road_draw_band_mirror: the band from row r (far) down to row r - 1 (near), the flags of segment fs; the grass
 * and the road on a plain band, only the kerbs on a crossing (its road is the backdrop), the finish white only
 * once the car is past 169h (3ADh in Mulholland), the dash DS_dash_width[13 + r] stepped per row. */
static void band(const Cam *c, const Row *near_, const Row *far_, int r, int fs)
{
    const Row ra = *near_, rb = *far_;
    double ya = mpy(c, &ra, Y_ROAD), yb = mpy(c, &rb, Y_ROAD);
    if (yb >= ya) return;                                   /* no rows, or back-facing */
    u16 C = trk(TRK_C, fs), L = trk(TRK_L, fs), R = trk(TRK_R, fs);
    double la, ra_, lb, rb_;
    edges(trk(TRK_C, ra.n), &la, &ra_);
    edges(trk(TRK_C, rb.n), &lb, &rb_);
    double xla = mpx(c, &ra, la), xra = mpx(c, &ra, ra_), xlb = mpx(c, &rb, lb), xrb = mpx(c, &rb, rb_);
    /* The game's spans include both ends (x1..x2), and a later span covers the one before. */
    if (!(C & 1)) {
        enh_fill_rows(yb, ya, COL_GRASS);
        bool white = (C & 0x10) && c->base >= (c->mode == 0x10 ? 0x3AD : 0x169);
        quad(xla, xra + 1, ya, xlb, xrb + 1, yb, white ? COL_FINISH : COL_ROAD);
    } else {
        if (C & 4) return;                                  /* intersection */
        double xola = mpx(c, &ra, mx_out(c, 0)), xolb = mpx(c, &rb, mx_out(c, 0));
        double xora = mpx(c, &ra, mx_out(c, 1)), xorb = mpx(c, &rb, mx_out(c, 1));
        if (L & 0x80) {             /* the kerb to the middle, m = xol + ((xl - xol) >> 1), then the grass */
            double ma = mid(c, xola, xla), mb = mid(c, xolb, xlb);
            quad(xola, ma + 1, ya, xolb, mb + 1, yb, COL_KERB);
            quad(ma, xla + 1, ya, mb, xlb + 1, yb, COL_GRASS);
        } else
            quad(xola, xla + 1, ya, xolb, xlb + 1, yb, COL_KERB);
        if (R & 0x80) {             /* the kerb from the middle, m = xor - ((xor - xr) >> 1), then the grass */
            double ma = c->exact ? xora - floor((xora - xra) / 2) : (xora + xra) / 2;
            double mb = c->exact ? xorb - floor((xorb - xrb) / 2) : (xorb + xrb) / 2;
            quad(ma, xora + 1, ya, mb, xorb + 1, yb, COL_KERB);
            quad(xra, ma + 1, ya, xrb, mb + 1, yb, COL_GRASS);
        } else
            quad(xra, xora + 1, ya, xrb, xorb + 1, yb, COL_KERB);
    }
    if (C & 8) {                    /* x from mid - (w >> 1) to mid - (w >> 1) + w, mid = (xl + xr) >> 1 */
        double w = DSS((u16)(DS_dash_width + 2 * (13 + r)));
        double h = c->exact ? floor(w / 2) : w / 2;
        double ca = (xla + xra) / 2, cb = (xlb + xrb) / 2;
        if (c->exact) { ca = floor(ca); cb = floor(cb); }
        quad(ca - h, ca - h + w + 1, ya, cb - h, cb - h + w + 1, yb, COL_KERB);
    }
}

/* ---------------------------------------------------------------- the pass */

void enh_draw_mirror(const EnhView *v)
{
    Cam c;
    cam_of(v, &c);
    enh_clip(MIR_X0, MIR_Y0, MIR_X1, MIR_Y1);
    Row rows[MIR_ROWS + 1];
    for (int k = 0; k <= MIR_ROWS; k++) rows[k] = mrow(&c, k);
    /* mirror_draw_backdrop: the glass of the mirror's picture (page 2 at E8h,68h), then the sky strip (page 2 at
     * E8h,81h) above the far row, 2 rows (14h rows from 10 above it in Mulholland) */
    enh_draw_image(enh_image_vram(0x4000, 0xEA, 0x6A, MIR_X1 - MIR_X0, MIR_Y1 - MIR_Y0), MIR_X0, MIR_Y0, MIR_X1, MIR_Y1);
    bool hills = c.mode == 0x10;
    int sh = hills ? 0x14 : 2;
    double sy = mpy(&c, &rows[MIR_ROWS], Y_ROAD) - ROW_TOP - (hills ? 10 : 2);
    enh_draw_image(enh_image_vram(0x4000, 0xE8, 0x81, 0x50, sh), 0xF0, sy, 0xF0 + 0x50, sy + sh);
    /* far to near, each segment's cuts after its band and the opponent after its row's objects */
    int base = c.base;
    enh_cuts_at(v, ENH_PASS_MIRROR, INT_MIN, base - MIR_ROWS - 1, false);
    enh_cuts_at(v, ENH_PASS_MIRROR, INT_MIN, base - MIR_ROWS - 1, true);
    for (int r = MIR_ROWS; r >= 1; r--) {
        int n = base - r, fs = n - 1;
        town_walls(&c, &rows[r - 1], &rows[r], fs);
        band(&c, &rows[r - 1], &rows[r], r, fs);
        enh_cuts_at(v, ENH_PASS_MIRROR, n, n, false);
        enh_cuts_at(v, ENH_PASS_MIRROR, n, n, true);
    }
    enh_cuts_at(v, ENH_PASS_MIRROR, base, INT_MAX, false);
    enh_cuts_at(v, ENH_PASS_MIRROR, base, INT_MAX, true);
}
