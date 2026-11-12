#pragma once
/* The Aqueduct (road mode 8) of Street Rod 2's enhanced view: its camera, rows (aq_project_front,
 * aq_project_mirror), the roll rotation (rotate_about_centre), the segment flags, the sky's offsets and the
 * opponent's place (opp_bank_variant, opponent_draw). Exact mode repeats the game's s16 arithmetic, a smooth view
 * the same formulas in doubles. Nothing here reads or writes the game's drawing state (the rotation, the rows):
 * only the wall shifts (set at race start) and the track's flags are read at run time. */
#include <stdbool.h>

#include "enhanced/enh_internal.h"

/* race_aq.c's file-local DGROUP addresses: the rotated rows (k = 0 the farthest) and the trough */
#define AQ_R_LX  0x9732
#define AQ_R_LY  0x9760
#define AQ_R_RX  0xB170
#define AQ_R_RY  0xB1A0
#define AQ_R_TLX 0x9BC6
#define AQ_R_TLY 0x9CFC
#define AQ_R_TRX 0x9DCE
#define AQ_R_TRY 0x9F0E
#define AQ_TROUGH_NEAR_XL 0xB16E
#define AQ_TROUGH_NEAR_XR 0xB872
#define AQ_TROUGH_FAR_XL  0xB7EA
#define AQ_TROUGH_FAR_XR  0xB8D8
#define AQ_TROUGH_NEAR_Y  0xADA6
#define AQ_TROUGH_FAR_Y   0xB162
#define ENH_AQ_ROLL_SNAP  60        /* degrees: a larger roll step is the bounce's snap back */

enum { ENH_AQ_STARS, ENH_AQ_CLOUDS, ENH_AQ_MIR_STARS };

typedef struct {
    bool exact, mirror, roll0;      /* roll0: roll_raw == 0 (the unrotated constructions) */
    int base, n;                    /* n: the last row, 14 front, 7 mirror */
    double q;                       /* 8 frac(pos) */
    double lat, cx;                 /* DS_cam_x; cx clamped to 564h..D5Ch (cam_x_clamped) */
    double heading, aq_dy;          /* the projection's dx and dy (the mirror negates both, XC excepted) */
    double cam_y, bounce;           /* the mirror: no bounce */
    double def[6];                  /* EnhView.aq_def: floor_h, xl, xr, top_h, top_xl, top_xr */
    s16 wall_l, wall_r;             /* DS_aq_wall_l_shift, DS_aq_wall_r_shift, read at run time */
    double cs, sn, pcx, pcy;        /* the rotation (x256) and its pivot */
    double roll, roll_raw;          /* EnhView.roll (unwrapped), EnhView.roll_raw */
    int x0, y0, x1, y1;             /* the game's clip, inclusive: clip_set_view (front), clip_set_mirror (mirror) */
} AqCam;

typedef struct { double y, xl, xr, xc, yt, xtl, xtr; } AqRow;
typedef struct { AqRow r[15]; double tnl, tnr, tfl, tfr, tny, tfy; } AqRows;   /* raw rows, row 0 nearest */

void enh_aq_cam(const EnhView *v, bool mirror, AqCam *c);   /* the mirror's pivot from its own rows 7 */
double enh_aq_z(int i, double q);    /* 4; 8i + 4 - q for 1..11; 100, 108, 116 for 12..14 */
double enh_aq_mz(int i, double q);   /* 2; 8i + q for 1..4; 8i for 5 and up (40, 48, 56) */
void enh_aq_rows(const AqCam *c, AqRows *r);   /* aq_project_front (rows 0..14) / aq_project_mirror (0..7) */
/* A point of the game's line coordinates rotated as rotate_about_centre rotates it (exact), or by the view's
 * roll about its pivot (smooth, the 255/256 shrink kept). */
void enh_aq_rot(const AqCam *c, double x, double y, double *ox, double *oy);
u16 enh_aq_flag(const AqCam *c, int i);   /* front TRKW(TRK_AQ, base + i); mirror TRKW(TRK_AQM, 179h - base - (7 - i)) */
/* The roll unwrapped: the step raw - prev_raw wrapped to -180..180 added to prev; *snap when the step is more
 * than ENH_AQ_ROLL_SNAP degrees (the bounce's snap back, not interpolated). */
double enh_aq_roll_unwrap(double prev, s16 prev_raw, s16 raw, bool *snap);
/* The sky's x offset for heading h: the stars sc >> 1, the clouds sc, the mirror's stars -(sc >> 2), sc =
 * cmod16(shl16(h, 1), 140h); a smooth view adds 2 frac(h) to sc. */
double enh_aq_sky_x(double heading, int which, bool exact);
double enh_aq_opp_bank(const AqCam *front, double opp_lat);   /* opp_bank_variant's DS_opp_bank, from roll_raw */
double enh_aq_opp_wy(const AqCam *front, double opp_lat);     /* opponent_draw's wy up the walls */
/* opponent_draw's place in mode 8: z (|z| + 4 front, -z mirror), sx, sy, then rotated about the pivot of the pass
 * that draws it (mirror_pass: opp_seg < seg, the mirror's; else the front's), whatever the picture; false when
 * not drawn */
bool enh_aq_opp_anchor(const AqCam *front, const AqCam *mirror, double d, int sprite, double opp_lat, int prev_sx,
                       bool mirror_pass, double *ax, double *ay, double *z);
/* y, xl, xr into their slots, XC into ybl, YT into ye, XTL into xol, XTR into xor_ (race_aq.c's A_*); rows 0..n;
 * yk and ybr untouched */
void enh_rows_exact_aq(const EnhView *v, bool mirror, EnhRowTab *t);
/* aq_draw_sky and aq_draw_scene(14) (race_aq.c) of the view: the sky, the rolled channel, its walls, bands, openings,
 * trough, boxes and pillars, the captured cuts at their place (the stars and clouds after the sky's fill, the rows'
 * shapes in the band loop, the opponent with the boxes); exact mode with the game's integer clip and lines. */
void enh_draw_aq(const EnhView *v);
/* aq_draw_mirror (race_aq.c) of the view in its glass F2h..13Dh x 16h..2Ah: the backdrop, the mirror's stars, then
 * aq_draw_scene(7) with the mirror's rows, pivot and clip (F0h..13Fh x 16h..2Ah), its rows' shapes, the starter and the
 * opponent behind at their place. */
void enh_draw_aq_mirror(const EnhView *v);
/* enh_obj_geo in mode 8 (xf): ENH_CUT_ROW at segment seg (row k = seg - base at enh_aq_z, beyond the rows 8 (seg -
 * pos) + 4) anchored where aq_draw_row_shape anchors its shape (flag 20h, else 40h; world x 0 on the floor without
 * either), rot the roll; ENH_CUT_OPP at enh_aq_opp_anchor, rot its bank (pictures below 9 b 4 / z, in exact mode moved
 * by the byte their picture was blitted at; 9 and up the mirror's, b 1 / z); ENH_CUT_STARS and ENH_CUT_CLOUDS at
 * (enh_aq_sky_x, 0), the stars rotated only off roll 0. The mirror's (b 1 / z, about the mirror's pivot):
 * ENH_CUT_MIR at segment seg (row k = base - seg at enh_aq_mz, not for k < 0) anchored as the front's rows with the
 * mirror's flag; ENH_CUT_STARTER (seg the starter's) on the raw row k = enh_aq_starter_k for k 0..5, at x on the
 * mirror's trough's left line plus 1, rotated off roll 0, turned only beyond 5 degrees (drv_rotate); ENH_CUT_MIR_STARS
 * at (the mirror's sky x, 0), rotated off roll 0. */
EnhObjGeo enh_obj_geo_aq(const EnhView *v, int kind, int seg, int sprite);
/* starter_draw_mirror_aq's raw row: base - starter_seg - 1 (row N - i of the row i it is drawn at) */
int enh_aq_starter_k(int base, int starter_seg);
