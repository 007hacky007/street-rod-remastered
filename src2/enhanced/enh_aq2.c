/* The Aqueduct (road mode 8) of Street Rod 2's enhanced view (enh_aq2.h): the geometry of race_aq.c's
 * aq_project_front, aq_project_mirror, rotate_about_centre (race_gfx.c), opp_bank_variant and opponent_draw
 * (race_opp.c), in the game's arithmetic (exact) and in floating point (smooth). */
#include "enhanced/enh_aq2.h"

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
