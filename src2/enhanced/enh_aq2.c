/* The Aqueduct (road mode 8) of Street Rod 2's enhanced view (enh_aq2.h): the geometry of race_aq.c's
 * aq_project_front, aq_project_mirror, rotate_about_centre (race_gfx.c), opp_bank_variant and opponent_draw
 * (race_opp.c), in the game's arithmetic (exact) and in floating point (smooth). */
#include "enhanced/enh_aq2.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "enhanced/enh_geo2.h"

#define CAM_X_MIN 0x564
#define CAM_X_MAX 0xD5C

/* a sum as the game's s16 (exact), or as it is */
static inline double S(const AqCam *c, double v) { return c->exact ? i16(v) : v; }

/* road_proj_x / road_proj_y with the clamped cam_x, and their mirror forms */
static double prx(const AqCam *c, double z, double X, double dx)
{
    if (c->exact) return enh_proj_x_i((int)z, i16(X), i16(dx), (int)c->cx);
    return 160.0 - 4.0 * (c->cx - (X + z * dx / 4.0)) / z;
}
static double pry(const AqCam *c, double z, double Y, double dy)
{
    if (c->exact) return enh_proj_y_i((int)z, i16(Y), i16(dy), (int)c->cam_y, (int)c->bounce);
    return 2.0 * dy + 4.0 * (Y - c->cam_y) / z + c->bounce + 60.0;
}
static double mrx(const AqCam *c, double z, double X, double dx)
{
    if (c->exact) return enh_proj_x_mirror_i((int)z, i16(X), i16(dx), (int)c->cx);
    return 0x118 - (c->cx - (X + z * dx / 4.0)) / z;
}
static double mry(const AqCam *c, double z, double Y, double dy)
{
    if (c->exact) return enh_proj_y_mirror_i((int)z, i16(Y), i16(dy), (int)c->cam_y);
    return dy + (Y - c->cam_y) / z + 0x24;
}
static double proj_x(const AqCam *c, double z, double X, double dx) { return c->mirror ? mrx(c, z, X, dx) : prx(c, z, X, dx); }
static double proj_y(const AqCam *c, double z, double Y, double dy) { return c->mirror ? mry(c, z, Y, dy) : pry(c, z, Y, dy); }

/* the channel's centre: (s16)(xr + xl + 12C0h) >> 1 */
static double mid_of(const AqCam *c)
{
    if (c->exact) return (s16)(i16(c->def[2] + c->def[1] + 0x12C0) >> 1);
    return (c->def[2] + c->def[1] + 0x12C0) / 2.0;
}

double enh_aq_z(int i, double q)
{
    if (i <= 0) return 4.0;
    if (i <= 11) return 8.0 * i + 4.0 - q;
    return 100.0 + 8.0 * (i - 12);
}

double enh_aq_mz(int i, double q)
{
    if (i <= 0) return 2.0;
    if (i <= 4) return 8.0 * i + q;
    return 8.0 * i;
}

void enh_aq_rows(const AqCam *c, AqRows *r)
{
    double mid = mid_of(c), dx = c->heading, dy = c->aq_dy;
    const double *d = c->def;
    for (int i = 0; i <= c->n; i++) {
        double z = c->mirror ? enh_aq_mz(i, c->q) : enh_aq_z(i, c->q);
        AqRow *w = &r->r[i];
        w->y = proj_y(c, z, S(c, d[0] + 2000), dy);
        w->xl = proj_x(c, z, S(c, d[1] + 0x5C8), dx);
        w->xr = proj_x(c, z, S(c, d[2] + 0xCF8), dx);
        w->xc = proj_x(c, z, S(c, mid + d[2]), c->mirror ? -dx : dx);   /* the mirror's quirk: + heading */
        w->yt = proj_y(c, z, S(c, d[3] + 0x67C), dy);
        if (c->mirror) {
            w->xtl = proj_x(c, z, S(c, c->wall_l + 0x438), dx);
            w->xtr = proj_x(c, z, S(c, c->wall_r + 0xE88), dx);
        } else {
            w->xtl = proj_x(c, z, S(c, d[4] + 0x438), dx);
            w->xtr = proj_x(c, z, S(c, d[5] + 0xE88), dx);
        }
    }
    double zn = c->mirror ? 2.0 : 4.0, zf = c->mirror ? enh_aq_mz(c->n, c->q) : enh_aq_z(c->n, c->q);
    r->tnl = proj_x(c, zn, S(c, mid + d[1] - 0x28), dx);
    r->tnr = proj_x(c, zn, S(c, d[2] + mid + 0x28), dx);
    r->tfl = proj_x(c, zf, S(c, mid + d[1] - 0x28), dx);
    r->tfr = proj_x(c, zf, S(c, mid + d[2] + 0x28), dx);
    r->tny = r->r[0].y;
    r->tfy = r->r[c->n].y;
}

void enh_aq_cam(const EnhView *v, bool mirror, AqCam *c)
{
    c->exact = v->exact;
    c->mirror = mirror;
    c->n = mirror ? 7 : 14;
    double fl = floor(v->pos);
    c->base = (int)fl;
    c->q = v->exact ? floor(8.0 * (v->pos - fl)) : 8.0 * (v->pos - fl);
    c->lat = v->lat;
    double cx = v->exact ? i16(v->lat) : v->lat;
    c->cx = cx < CAM_X_MIN ? CAM_X_MIN : cx > CAM_X_MAX ? CAM_X_MAX : cx;
    /* the game's dx is the s16 heading; a smooth view keeps the fraction */
    double hf = floor(v->heading), h = enh_s16_of(hf) + (v->exact ? 0.0 : v->heading - hf);
    double dy = v->exact ? i16(v->aq_dy) : v->aq_dy;
    c->heading = mirror ? -h : h;
    c->aq_dy = mirror ? -dy : dy;
    c->cam_y = v->cam_y;
    c->bounce = mirror ? 0.0 : v->bounce;
    for (int i = 0; i < 6; i++) c->def[i] = v->aq_def[i];
    c->wall_l = DSS(DS_aq_wall_l_shift);
    c->wall_r = DSS(DS_aq_wall_r_shift);
    c->roll = v->roll;
    c->roll_raw = v->roll_raw;
    c->roll0 = v->roll_raw == 0;
    if (v->exact) {
        c->cs = v->rot_cos;
        c->sn = v->rot_sin;
    } else {
        double a = v->roll * SDL_PI_D / 180.0;
        c->cs = 255.0 * cos(a);
        c->sn = 255.0 * sin(a);
    }
    if (!mirror) {
        c->pcx = v->piv_x;
        c->pcy = v->piv_y;
        return;
    }
    /* aq_draw_mirror: the pivot ((XL7 + XR7) >> 1, Y7) of the mirror's own rows */
    AqRows r;
    enh_aq_rows(c, &r);
    if (c->exact) c->pcx = (s16)(i16(r.r[7].xl + r.r[7].xr) >> 1);
    else c->pcx = (r.r[7].xl + r.r[7].xr) / 2.0;
    c->pcy = r.r[7].y;
}

static inline s16 mul8(s16 a, s16 b) { return (s16)(((s32)a * b) >> 8); }

void enh_aq_rot(const AqCam *c, double x, double y, double *ox, double *oy)
{
    if (c->exact) {
        s16 cx = i16(c->pcx), cy = i16(c->pcy), cs = i16(c->cs), sn = i16(c->sn);
        s16 dx = (s16)(i16(x) - cx), dy = (s16)(i16(y) - cy);
        *ox = (s16)(mul8(dx, cs) + mul8(dy, sn) + cx);
        *oy = (s16)(mul8(dy, cs) + mul8((s16)-dx, sn) + cy);
        return;
    }
    double dx = x - c->pcx, dy = y - c->pcy;
    *ox = c->pcx + (dx * c->cs + dy * c->sn) / 256.0;
    *oy = c->pcy + (dy * c->cs - dx * c->sn) / 256.0;
}

u16 enh_aq_flag(const AqCam *c, int i)
{
    if (c->mirror) return TRKW(TRK_AQM, 0x179 - c->base - (7 - i));
    return TRKW(TRK_AQ, c->base + i);
}

double enh_aq_roll_unwrap(double prev, s16 prev_raw, s16 raw, bool *snap)
{
    int d = (((raw - prev_raw + 180) % 360) + 360) % 360 - 180;
    *snap = abs(d) > ENH_AQ_ROLL_SNAP;
    return prev + d;
}

double enh_aq_sky_x(double heading, int which, bool exact)
{
    double hf = floor(heading);
    s16 sc = cmod16(shl16(enh_s16_of(hf), 1), 0x140);
    if (exact) {
        if (which == ENH_AQ_STARS) return (s16)(sc >> 1);
        if (which == ENH_AQ_CLOUDS) return sc;
        return (s16)-(s16)(sc >> 2);
    }
    double s = sc + 2.0 * (heading - hf);
    return which == ENH_AQ_STARS ? s / 2.0 : which == ENH_AQ_CLOUDS ? s : -s / 4.0;
}

double enh_aq_opp_bank(const AqCam *front, double opp_lat)
{
    if (front->exact) {
        s16 lat = i16(opp_lat), sxl = i16(front->def[1]), sxr = i16(front->def[2]), e;
        if (lat < (s16)(sxl + 0x5FA)) e = (s16)(sxl + 0x5FA - lat);
        else if (lat > (s16)(sxr + 0xCC6)) e = (s16)(lat - sxr - 0xCC6);
        else e = 0;
        s16 bank = e >= 100 ? 0x2D : e < 1 ? 0 : cdiv16((s16)(e * 0x2D), 100);
        if ((s16)(lat - ((s16)(sxr + sxl + 0x12C0) >> 1)) < 0) bank = (s16)-bank;
        s16 roll = i16(front->roll_raw);
        bank = (s16)(bank + roll);
        if (roll < 0 && roll > -0x5A) bank = (s16)(bank + (roll >> 2));
        if (bank > 0x167) bank = (s16)(bank - 0x168);
        if (bank < -0x167) bank = (s16)(bank + 0x168);
        return bank;
    }
    double sxl = front->def[1], sxr = front->def[2], e;
    if (opp_lat < sxl + 0x5FA) e = sxl + 0x5FA - opp_lat;
    else if (opp_lat > sxr + 0xCC6) e = opp_lat - sxr - 0xCC6;
    else e = 0;
    double bank = (e > 100 ? 100 : e < 0 ? 0 : e) * 0.45;
    if (opp_lat - (sxr + sxl + 0x12C0) / 2.0 < 0) bank = -bank;
    double roll = front->roll_raw;
    bank += roll;
    if (roll < 0 && roll > -0x5A) bank += roll / 4.0;
    if (bank > 0x167) bank -= 0x168;
    if (bank < -0x167) bank += 0x168;
    return bank;
}

double enh_aq_opp_wy(const AqCam *front, double opp_lat)
{
    const double *d = front->def;
    if (front->exact) {
        s16 lat = i16(opp_lat), sxl = i16(d[1]), sxr = i16(d[2]);
        s16 wy = i16(d[0] + 2000), th = i16(d[3] - d[0] + 0x154);
        if (lat < sxl + 0x5C8) {
            s32 num = (s32)(s16)(sxl - lat + 0x5A0) * th;
            s16 den = (s16)(400 - front->wall_l + sxl);
            wy = (s16)(wy - (den ? (s16)(num / den) : 0));
        } else if (lat > sxr + 0xCF8) {
            s32 num = (s32)th * (s16)(lat - sxr - 0xCF8);
            s16 den = (s16)(400 - sxr + front->wall_r);
            wy = (s16)(wy - (den ? (s16)(num / den) : 0));
        }
        return wy;
    }
    double wy = d[0] + 2000, th = d[3] - d[0] + 0x154;
    if (opp_lat < d[1] + 0x5C8) {
        double den = 400 - front->wall_l + d[1];
        if (den != 0) wy -= (d[1] - opp_lat + 0x5A0) * th / den;
    } else if (opp_lat > d[2] + 0xCF8) {
        double den = 400 - d[2] + front->wall_r;
        if (den != 0) wy -= th * (opp_lat - d[2] - 0xCF8) / den;
    }
    return wy;
}

bool enh_aq_opp_anchor(const AqCam *front, const AqCam *mirror, double d, int sprite, double opp_lat, int prev_sx,
                       bool mirror_pass, double *ax, double *ay, double *z)
{
    if (sprite < 0 || sprite > 12) return false;
    bool ex = front->exact;
    double zz = ex ? cdiv16(i16(d), 8) : d / 8.0;
    if (sprite == 0) zz = ex ? add16(i16(zz), 0x10) : zz + 16;
    if (zz < -2 && sprite == 2) zz = ex ? add16(i16(zz), 8) : zz + 8;
    double wy = enh_aq_opp_wy(front, opp_lat), lat = ex ? i16(opp_lat) : opp_lat, sx, sy;
    if (sprite < 9) {
        zz = ex ? (s16)(abs16(i16(zz)) + 4) : fabs(zz) + 4;
        sx = prx(front, zz, lat, front->heading);
        sy = pry(front, zz, wy, front->aq_dy);
        if (ex && sprite == 2 && sx > 0x13F && prev_sx < 200) sx = (s16)(((int)sx + prev_sx) / 2);
        if ((sprite == 2 || sprite == 3) && sy > 100) sy = 100;
    } else {
        zz = ex ? (s16)-i16(zz) : -zz;
        if (!ex && zz < 1) return false;
        sx = mrx(front, zz, lat, ex ? (s16)(10 - i16(front->heading)) : 10 - front->heading);
        sy = mry(front, zz, wy, ex ? (s16)-i16(front->aq_dy) : -front->aq_dy);
    }
    enh_aq_rot(mirror_pass ? mirror : front, sx, sy, ax, ay);
    *z = zz;
    return true;
}

void enh_rows_exact_aq(const EnhView *v, bool mirror, EnhRowTab *t)
{
    EnhView e = *v;
    e.exact = true;
    AqCam c;
    enh_aq_cam(&e, mirror, &c);
    AqRows r;
    enh_aq_rows(&c, &r);
    for (int i = 0; i <= c.n; i++) {
        const AqRow *w = &r.r[i];
        t->y[i] = i16(w->y);
        t->xl[i] = i16(w->xl);
        t->xr[i] = i16(w->xr);
        t->ybl[i] = i16(w->xc);
        t->ye[i] = i16(w->yt);
        t->xol[i] = i16(w->xtl);
        t->xor_[i] = i16(w->xtr);
    }
}

void enh_aqtab_read(EnhAqTab *t)
{
    for (int k = 0; k < 15; k++) {
        t->lx[k] = DSS((u16)(AQ_R_LX + 2 * k));
        t->ly[k] = DSS((u16)(AQ_R_LY + 2 * k));
        t->rx[k] = DSS((u16)(AQ_R_RX + 2 * k));
        t->ry[k] = DSS((u16)(AQ_R_RY + 2 * k));
        t->tlx[k] = DSS((u16)(AQ_R_TLX + 2 * k));
        t->tly[k] = DSS((u16)(AQ_R_TLY + 2 * k));
        t->trx[k] = DSS((u16)(AQ_R_TRX + 2 * k));
        t->try_[k] = DSS((u16)(AQ_R_TRY + 2 * k));
    }
    t->tnl = DSS(AQ_TROUGH_NEAR_XL);
    t->tnr = DSS(AQ_TROUGH_NEAR_XR);
    t->tfl = DSS(AQ_TROUGH_FAR_XL);
    t->tfr = DSS(AQ_TROUGH_FAR_XR);
    t->tny = DSS(AQ_TROUGH_NEAR_Y);
    t->tfy = DSS(AQ_TROUGH_FAR_Y);
    t->roll = DSS(DS_rot_enable);
    t->sn = DSS(DS_rot_sin);
    t->cs = DSS(DS_rot_cos);
    t->cx = DSS(DS_rot_cx);
    t->cy = DSS(DS_rot_cy);
}

void enh_aqtab_exact(const EnhView *v, bool mirror, EnhAqTab *t)
{
    EnhView e = *v;
    e.exact = true;
    AqCam c;
    enh_aq_cam(&e, mirror, &c);
    AqRows r;
    enh_aq_rows(&c, &r);
    for (int i = 0; i <= c.n; i++) {
        int k = c.n - i;
        const AqRow *w = &r.r[i];
        double x, y;
        enh_aq_rot(&c, w->xl, w->y, &x, &y);   t->lx[k] = i16(x);  t->ly[k] = i16(y);
        enh_aq_rot(&c, w->xr, w->y, &x, &y);   t->rx[k] = i16(x);  t->ry[k] = i16(y);
        enh_aq_rot(&c, w->xtl, w->yt, &x, &y); t->tlx[k] = i16(x); t->tly[k] = i16(y);
        enh_aq_rot(&c, w->xtr, w->yt, &x, &y); t->trx[k] = i16(x); t->try_[k] = i16(y);
    }
    t->tnl = i16(r.tnl);
    t->tnr = i16(r.tnr);
    t->tfl = i16(r.tfl);
    t->tfr = i16(r.tfr);
    t->tny = i16(r.tny);
    t->tfy = i16(r.tfy);
    t->roll = i16(c.roll_raw);
    t->sn = i16(c.sn);
    t->cs = i16(c.cs);
    t->cx = i16(c.pcx);
    t->cy = i16(c.pcy);
}

int enh_aqtab_err(const EnhAqTab *a, const EnhAqTab *b, int n)
{
    const s16 *pa[8] = { a->lx, a->ly, a->rx, a->ry, a->tlx, a->tly, a->trx, a->try_ };
    const s16 *pb[8] = { b->lx, b->ly, b->rx, b->ry, b->tlx, b->tly, b->trx, b->try_ };
    int e = 0;
    for (int j = 0; j < 8; j++)
        for (int k = 0; k <= n && k < 15; k++) {
            int d = abs(pa[j][k] - pb[j][k]);
            if (d > e) e = d;
        }
    const s16 ta[6] = { a->tnl, a->tnr, a->tfl, a->tfr, a->tny, a->tfy };
    const s16 tb[6] = { b->tnl, b->tnr, b->tfl, b->tfr, b->tny, b->tfy };
    for (int j = 0; j < 6; j++) {
        int d = abs(ta[j] - tb[j]);
        if (d > e) e = d;
    }
    return e;
}

/* ---------------------------------------------------------------- the front view (aq_draw_sky, aq_draw_scene) */

/* clip_set_view: the game's clip, inclusive */
#define CLIP_X0 0
#define CLIP_X1 0x13F
#define CLIP_Y0 0x12
#define CLIP_Y1 0x65
#define COL_SKY      1
#define COL_CONCRETE 5
#define COL_TROUGH   2
#define COL_LINE     0
#define COL_TOP      15

typedef struct { double x, y; } P2;

/* clip_polygon and poly_fill_pts: exact on the game's integer clip with poly_fill's own walkers, a smooth view on
 * the same clip in doubles */
static void aq_poly(const AqCam *c, const P2 *p, int n, u8 fill, u8 edge)
{
    if (c->exact) {
        s16 in[16], out[128];
        for (int i = 0; i < n; i++) { in[2 * i] = (s16)i16(p[i].x); in[2 * i + 1] = (s16)i16(p[i].y); }
        int m = enh_clip_poly_i(in, n, out, CLIP_X0, CLIP_X1, CLIP_Y0, CLIP_Y1);
        if (m >= 1) enh_poly_fill_i(out, m, fill, edge);
        return;
    }
    double x[8], y[8], ox[64], oy[64];
    for (int i = 0; i < n; i++) { x[i] = p[i].x; y[i] = p[i].y; }
    int m = enh_clip_poly_f(x, y, n, ox, oy, CLIP_X0, CLIP_X1, CLIP_Y0, CLIP_Y1);
    if (m >= 1) enh_fill_poly_edge(ox, oy, m, fill, edge);
}

static void aq_quad(const AqCam *c, P2 a, P2 b, P2 d, P2 e, u8 fill, u8 edge)
{
    P2 q[4] = { a, b, d, e };
    aq_poly(c, q, 4, fill, edge);
}

/* clip_line */
static void aq_line(const AqCam *c, P2 a, P2 b, u8 col)
{
    if (c->exact) {
        s16 in[4] = { (s16)i16(a.x), (s16)i16(a.y), (s16)i16(b.x), (s16)i16(b.y) }, out[4];
        if (enh_clip_line_i(in, out, CLIP_X0, CLIP_X1, CLIP_Y0, CLIP_Y1)) enh_line1(out[0], out[1], out[2], out[3], col);
        return;
    }
    enh_line1(a.x, a.y, b.x, b.y, col);
}

static P2 aq_rot(const AqCam *c, double x, double y)
{
    P2 p;
    enh_aq_rot(c, x, y, &p.x, &p.y);
    return p;
}

/* the game's line_x_at_y / line_y_at_x (truncating), or the line itself */
static double lx_at_y(const AqCam *c, double x1, double y1, double x2, double y2, double y)
{
    if (c->exact) return line_x_at_y((s16)i16(x1), (s16)i16(y1), (s16)i16(x2), (s16)i16(y2), (s16)i16(y));
    return y2 == y1 ? x2 : x1 + (y - y1) * (x2 - x1) / (y2 - y1);
}
static double ly_at_x(const AqCam *c, double x1, double y1, double x2, double y2, double x)
{
    if (c->exact) return line_y_at_x((s16)i16(x1), (s16)i16(y1), (s16)i16(x2), (s16)i16(y2), (s16)i16(x));
    return x2 == x1 ? y2 : y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

static double dv(const AqCam *c, double a, int b) { return c->exact ? cdiv16((s16)i16(a), (s16)b) : a / b; }   /* cdiv16 */
static double qs(const AqCam *c, double a, int n) { return c->exact ? q_shr((s16)i16(a), n) : a / (1 << n); }  /* q_shr */
static double sr(const AqCam *c, double a, int n) { return c->exact ? (s16)i16(a) >> n : a / (1 << n); }        /* s16 >> */

/* the near point extended: 4 near - 3 far (shl16, s16) */
static P2 extend(const AqCam *c, P2 n, P2 f)
{
    P2 p = { S(c, 4 * n.x - S(c, 3 * f.x)), S(c, 4 * n.y - S(c, 3 * f.y)) };
    return p;
}

/* aq_draw_box: p[0..7] = P1..P8 unrotated (far bottom L, R, far top L, R, near bottom L, R, near top L, R) */
static void aq_box(const AqCam *c, const P2 *raw, bool solid)
{
    P2 p[9];
    for (int k = 0; k < 8; k++) p[k + 1] = aq_rot(c, raw[k].x, raw[k].y);
    (void)solid;    /* the pillars' footprint is only the game's hit test */
#define THIN(a, b) ((fabs(S(c, (a).x - (b).x)) <= 2 || fabs(S(c, (a).y - (b).y)) <= 2) ? COL_TROUGH : COL_LINE)
    if (p[5].x > p[1].x) aq_quad(c, p[1], p[3], p[7], p[5], COL_TROUGH, THIN(p[1], p[7]));
    if (p[6].x < p[2].x) aq_quad(c, p[2], p[4], p[8], p[6], COL_TROUGH, THIN(p[2], p[8]));
    if (p[3].y < p[7].y) aq_quad(c, p[3], p[4], p[8], p[7], COL_CONCRETE, THIN(p[3], p[8]));
    aq_quad(c, p[6], p[5], p[7], p[8], COL_TROUGH, THIN(p[6], p[7]));
#undef THIN
}

void enh_draw_aq(const EnhView *v)
{
    enh_buf_setup();
    enh_clip_view();
    AqCam c;
    enh_aq_cam(v, false, &c);
    AqRows rr;
    enh_aq_rows(&c, &rr);
    const AqRow *R = rr.r;
    enum { N = 14 };
    P2 L[15], Rr[15], TL[15], TR[15];   /* rotated, k = 0 the farthest */
    for (int i = 0; i <= N; i++) {
        int k = N - i;
        L[k] = aq_rot(&c, R[i].xl, R[i].y);
        Rr[k] = aq_rot(&c, R[i].xr, R[i].y);
        TL[k] = aq_rot(&c, R[i].xtl, R[i].yt);
        TR[k] = aq_rot(&c, R[i].xtr, R[i].yt);
    }
    int base = c.base;

    /* aq_draw_sky: without roll only down to the pivot's y (the far floor) */
    enh_fill_rows(CLIP_Y0, c.roll0 ? (c.exact ? c.pcy : R[N].y) : CLIP_Y1, COL_SKY);
    enh_cut_id(v, ENH_OBJ_STARS);
    enh_cut_id(v, ENH_OBJ_CLOUDS);

    /* 1. the walls and the floor */
    P2 Lf = L[0], Rf = Rr[0], TLf = TL[0], TRf = TR[0], Ln = L[N], Rn = Rr[N], TLn = TL[N], TRn = TR[N];
    if (!c.roll0) {
        Ln = extend(&c, Ln, Lf);
        Rn = extend(&c, Rn, Rf);
        TLn = extend(&c, TLn, TLf);
        TRn = extend(&c, TRn, TRf);
        aq_quad(&c, TLn, TLf, Lf, Ln, COL_CONCRETE, COL_CONCRETE);
        aq_quad(&c, TRn, TRf, Rf, Rn, COL_CONCRETE, COL_CONCRETE);
        aq_quad(&c, Lf, Rf, Rn, Ln, COL_CONCRETE, COL_CONCRETE);
    } else {
        enh_fill_rows(R[N].y, CLIP_Y1, COL_CONCRETE);
        P2 l0 = { 0, Lf.y }, r0 = { 0x13F, Rf.y };
        aq_quad(&c, TLn, TLf, Lf, l0, COL_CONCRETE, COL_CONCRETE);
        aq_quad(&c, TRn, TRf, Rf, r0, COL_CONCRETE, COL_CONCRETE);
    }
    aq_line(&c, Lf, Ln, COL_LINE);
    aq_line(&c, Rf, Rn, COL_LINE);
    aq_line(&c, TLf, TLn, COL_TOP);
    aq_line(&c, TRf, TRn, COL_TOP);
    P2 tfl = aq_rot(&c, rr.tfl, rr.tfy), tfr = aq_rot(&c, rr.tfr, rr.tfy);
    P2 tnl = aq_rot(&c, rr.tnl, rr.tny), tnr = aq_rot(&c, rr.tnr, rr.tny);
    enh_cuts_at(v, ENH_PASS_FRONT, base + N + 1, INT_MAX, false);
    enh_cuts_at(v, ENH_PASS_FRONT, base + N + 1, INT_MAX, true);

    /* 2. per band, far to near: the white band, the joints, the wall openings, the rows' shapes */
    for (int k = 0; k < N; k++) {
        int i = N - k;
        u16 w = enh_aq_flag(&c, i);
        if (w & 0x400) aq_quad(&c, L[k], Rr[k], Rr[k + 1], L[k + 1], COL_TOP, COL_TOP);
        if ((w & 1) && R[i].y > R[N].y) {
            aq_line(&c, L[k], Rr[k], COL_LINE);
            aq_line(&c, L[k], TL[k], COL_LINE);
            aq_line(&c, Rr[k], TR[k], COL_LINE);
        }
        if (k > 1 && (w & 0x30C)) {                 /* aq_wall_opening_quad */
            const P2 *b = (w & 0x208) ? Rr : L, *t = (w & 0x208) ? TR : TL;
            double d0 = qs(&c, S(&c, b[k].x - t[k].x), 2), d1 = qs(&c, S(&c, b[k + 1].x - t[k + 1].x), 2);
            P2 q[4];
            if (w & 0x0C) {
                q[0].x = S(&c, b[k].x - d0);
                q[3].x = S(&c, b[k + 1].x - d1);
                q[0].y = ly_at_x(&c, b[k].x, b[k].y, t[k].x, t[k].y, q[0].x);
                q[3].y = ly_at_x(&c, b[k + 1].x, b[k + 1].y, t[k + 1].x, t[k + 1].y, q[3].x);
            } else {
                q[0] = b[k];
                q[3] = b[k + 1];
            }
            q[1].x = S(&c, t[k].x + d0);
            q[1].y = ly_at_x(&c, b[k].x, b[k].y, t[k].x, t[k].y, q[1].x);
            q[2].x = S(&c, t[k + 1].x + d1);
            q[2].y = ly_at_x(&c, b[k + 1].x, b[k + 1].y, t[k + 1].x, t[k + 1].y, q[2].x);
            aq_poly(&c, q, 4, COL_TROUGH, COL_LINE);
        }
        enh_cuts_at(v, ENH_PASS_FRONT, base + i, base + i, false);
    }

    /* 3. the centre trough, interrupted where the flags have bit 2 */
    int a = -1, b;
    for (int k = 0; k < N; k++) if (enh_aq_flag(&c, N - k) & 2) { a = k; break; }
    for (b = a + 1; b < N && (enh_aq_flag(&c, N - b) & 2); b++) {}
    if (b < 2) aq_quad(&c, tfl, tnl, tnr, tfr, COL_TROUGH, COL_LINE);
    else {
        if (a > 0) {
            double y = R[N - a].y;
            P2 pl = aq_rot(&c, lx_at_y(&c, rr.tfl, rr.tfy, rr.tnl, rr.tny, y), y);
            P2 pr = aq_rot(&c, lx_at_y(&c, rr.tfr, rr.tfy, rr.tnr, rr.tny, y), y);
            aq_quad(&c, tfl, pl, pr, tfr, COL_TROUGH, COL_LINE);
        }
        if (b < N - 1) {
            double y = R[N - b].y;
            P2 pl = aq_rot(&c, lx_at_y(&c, rr.tfl, rr.tfy, rr.tnl, rr.tny, y), y);
            P2 pr = aq_rot(&c, lx_at_y(&c, rr.tfr, rr.tfy, rr.tnr, rr.tny, y), y);
            aq_quad(&c, pl, tnl, tnr, pr, COL_TROUGH, COL_LINE);
        }
    }

    /* 4. the boxes and pillars, far to near on the raw rows, each row's opponent after them */
    for (int i = N; i > 0; i--) {
        u16 w = enh_aq_flag(&c, i);
        const AqRow *f = &R[i], *n = &R[i - 1];
        if (w & 0x10) {                             /* a low box over the middle third */
            double x0 = S(&c, f->xl + dv(&c, S(&c, f->xc - f->xl), 3)), x1 = S(&c, f->xr + dv(&c, S(&c, f->xc - f->xr), 3));
            double y0 = f->y, yt = S(&c, y0 + dv(&c, S(&c, f->yt - y0), 7));
            double n0 = S(&c, n->xl + dv(&c, S(&c, n->xc - n->xl), 3)), n1 = S(&c, n->xr + dv(&c, S(&c, n->xc - n->xr), 3));
            double ny = n->y, nt = S(&c, ny - dv(&c, S(&c, ny - n->yt), 7));
            P2 p[8] = { { x0, y0 }, { x1, y0 }, { x0, yt }, { x1, yt }, { n0, ny }, { n1, ny }, { n0, nt }, { n1, nt } };
            aq_box(&c, p, false);
        }
        if (w & 0x80) {                             /* two thin full-height pillars */
            double d = dv(&c, S(&c, f->xr - f->xl), 7), e = dv(&c, S(&c, n->xr - n->xl), 7);
            double y0 = f->y, yt = f->yt, ny = n->y, nt = n->yt;
            double l0 = S(&c, f->xl + d), l1 = S(&c, l0 + sr(&c, d, 3));
            double m0 = S(&c, n->xl + e), m1 = S(&c, m0 + sr(&c, e, 3));
            P2 p[8] = { { l0, y0 }, { l1, y0 }, { l0, yt }, { l1, yt }, { m0, ny }, { m1, ny }, { m0, nt }, { m1, nt } };
            aq_box(&c, p, true);
            double r0 = S(&c, f->xr - sr(&c, d, 3) - d), r1 = S(&c, r0 + sr(&c, d, 3));
            double s0 = S(&c, n->xr - sr(&c, e, 3) - e), s1 = S(&c, s0 + sr(&c, e, 3));
            P2 p2[8] = { { r0, y0 }, { r1, y0 }, { r0, yt }, { r1, yt }, { s0, ny }, { s1, ny }, { s0, nt }, { s1, nt } };
            aq_box(&c, p2, true);
        }
        enh_cuts_at(v, ENH_PASS_FRONT, base + i, base + i, true);
    }
    enh_cuts_at(v, ENH_PASS_FRONT, INT_MIN, base, false);
    enh_cuts_at(v, ENH_PASS_FRONT, INT_MIN, base, true);
}

/* ---------------------------------------------------------------- the cuts' places */

EnhObjGeo enh_obj_geo_aq(const EnhView *v, int kind, int seg, int sprite)
{
    EnhObjGeo g = { 0, 0, 0, 0, false, true, 0 };
    AqCam c;
    enh_aq_cam(v, false, &c);
    double ax, ay;
    if (kind == ENH_CUT_ROW) {
        int k = seg - c.base;
        double z = k >= 0 && k <= 14 ? enh_aq_z(k, c.q) : 8.0 * (seg - v->pos) + 4.0;
        if (z < 1.0) return g;
        /* aq_draw_row_shape's anchor (20h: a quarter in from the floor's left edge; 40h: an eighth in from the right),
         * rotated as shp_b_draw rotates it; world x 0 for a row without a shape */
        u16 fl = TRKW(TRK_AQ, seg);
        double x = prx(&c, z, 0, c.heading);
        double xl = prx(&c, z, S(&c, c.def[1] + 0x5C8), c.heading), xr = prx(&c, z, S(&c, c.def[2] + 0xCF8), c.heading);
        if (fl & 0x20) x = S(&c, xl + sr(&c, S(&c, xr - xl), 2));
        else if (fl & 0x40) x = S(&c, xr - sr(&c, S(&c, xr - xl), 3));
        enh_aq_rot(&c, x, pry(&c, z, S(&c, c.def[0] + 2000), c.aq_dy), &ax, &ay);
        g.z = z;
        g.b = 4.0 / z;
        g.rot = c.roll;
    } else if (kind == ENH_CUT_OPP) {
        if (sprite >= 9) return g;                  /* the mirror's pictures */
        AqCam m;
        enh_aq_cam(v, true, &m);
        double z;
        if (!enh_aq_opp_anchor(&c, &m, v->opp_d, sprite, v->opp_lat, v->opp_sx_prev, v->opp_mpass, &ax, &ay, &z))
            return g;
        if (c.exact) ax += ((v->opp_sx + DSS(DS_g_px_shift)) & ~7) - v->opp_sx;   /* drv_blit_masked: whole bytes */
        g.z = z;
        g.b = 4.0 / z;
        g.rot = enh_aq_opp_bank(&c, v->opp_lat);
    } else if (kind == ENH_CUT_STARS || kind == ENH_CUT_CLOUDS) {
        bool stars = kind == ENH_CUT_STARS;
        double x = enh_aq_sky_x(v->heading, stars ? ENH_AQ_STARS : ENH_AQ_CLOUDS, c.exact);
        if (stars && c.roll0) { ax = x; ay = 0; }   /* the stars are rotated only with a roll */
        else enh_aq_rot(&c, x, 0, &ax, &ay);
        g.z = 1;
        g.b = 1;
        g.rot = c.roll;
    } else
        return g;
    g.a = ax;
    g.yg = ay + ROW_TOP;
    g.ok = true;
    return g;
}
