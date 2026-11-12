#pragma once
/* Shared declarations of src2/enhanced (Street Rod 2's enhanced view). */
#include <SDL3/SDL_stdinc.h>

#include "types.h"

#include "enh_draw.h"
#include "enhanced.h"

/* A smoothly interpolated series of samples: [0] = the newest, [1] and [2] the ones before. One channel per
 * quantity of a game frame; part C fills the roll and the Aqueduct's channels (0 until then). */
enum { ENH_F_LAT, ENH_F_HEADING, ENH_F_POS, ENH_F_OPP_POS, ENH_F_OPP_LAT, ENH_F_CAM_Y, ENH_F_HOFF, ENH_F_BOUNCE,
       ENH_F_ROLL, ENH_F_AQ_DY, ENH_F_DEF0, ENH_SER_N = ENH_F_DEF0 + 6 };   /* DEF0..+5: def_floor_h, def_xl, def_xr, def_top_h, def_top_xl, def_top_xr */
typedef struct {
    Uint64 t[3];
    double v[3][ENH_SER_N];
    int n;
    double period;             /* the measured period, smoothed (ns) */
} EnhSeries;
void enh_ser_push(EnhSeries *s, Uint64 t, const double *v, double nominal_ns);
void enh_ser_at(const EnhSeries *s, Uint64 now, double *out);
void enh_ser_snap(EnhSeries *s, int k);       /* channel k jumped: the older samples take its new value */
s16 enh_s16_of(double h);                     /* (s16)(s32)floor(h): wraps, never undefined */
double enh_unwrap(double prev, s16 prev_raw, s16 raw);   /* prev + (s16)(raw - prev_raw) */

/* The game's row tables of a frame (road_project_front, road_project_mirror). */
typedef struct { s16 xl[15], xr[15], y[15], xol[15], xor_[15], ye[15], yk[15], ybl[15], ybr[15]; } EnhRowTab;
void enh_rowtab_read(EnhRowTab *t);           /* DS_row_xl, _xr, _y, _xol, _xor, _yeye, _ykerb, _ybank_l, _ybank_r now */

/* The newest frame's state that is not interpolated, filled when the frame is presented. */
typedef struct {
    s16 mode;                  /* DS_road_mode */
    bool town;                 /* the town cruise (DS_racing = 0) */
    s16 opp_sprite;            /* DS_opp_sprite after the frame was drawn: the opponent's view */
    s16 opp_seg;               /* DS_opp_seg */
    s16 opp_d;                 /* DS_opp_last_d: the depth the opponent was drawn at (1/64 segment) */
    s16 opp_sx;                /* the screen x it was drawn at: DS_opp_last_sx, for pictures 2..8 where the picture
                                  landed (enh_opp_drawn_sx) */
    bool opp_fixed;            /* drawn at a fixed screen spot: the side views 0 and 1, contact, result 3 */
    s16 fall_dir;              /* DS_fall_dir */
    bool fallen;               /* DS_fallen: no front view drawn */
    s16 rail_phase;            /* DS_rail_phase_seg */
    s16 start_seg;             /* DS_start_seg */
    s16 ground_top_y;          /* DS_ground_top_y */
    s16 top_band_row;          /* DS_top_band_row */
    s16 police;                /* DS_police_state */
    s16 before[5];             /* the DS words just before T_XL, T_XR, T_Y, T_YBL and T_YBR, read at enh_frame_built:
                                  front_hillsides and mirror_hillsides read them as row 1's "row -1" */
    EnhRowTab game_front;      /* read at enh_frame_built: the mirror overwrites rows 0..7 and all of YK later */
    EnhRowTab game_mirror;     /* read at enh_frame_presented */
} EnhSnap;

/* opponent_draw's front pictures 2..8 are blitted at whole bytes (clip_rect: x >> 3): the screen x their anchor
 * landed at, from the projected sx and the width the picture is centred by (2, 3: its visible width; 4..8: its
 * width). */
int enh_opp_drawn_sx(int sx, int width);

/* The smooth view at a moment: the road-curve part of the heading comes from YAW at pos. Positions in
 * segments (substep and the opponent's fraction are in 1/64 segment). */
typedef struct {
    double pos, lat, heading, opp_pos, opp_lat;   /* heading: DS_heading, unwrapped (not relative to the road) */
    double cam_y, hoff, bounce;                   /* DS_cam_y; DS_cur_height - HGT[seg]; DS_view_bounce_y */
    double roll, aq_dy, aq_def[6];                /* part C (0 in part B) */
    double roll_raw, piv_x, piv_y;                /* the Aqueduct: the roll in the game's range, the rotation's
                                                     pivot (0 until the frame state fills them) */
    s16 rot_sin, rot_cos, opp_sx_prev;            /* the frame's DS_rot_sin, DS_rot_cos; DS_opp_last_sx before it */
    double opp_d;              /* the opponent's depth in 1/64 segment: DS_opp_last_d at the newest frame, moved on
                                  by the cars' motion since then in a smooth view */
    s16 mode, opp_sprite;
    s16 opp_seg, opp_sx, fall_dir, start_seg, rail_phase, ground_top_y;
    s16 before[5];             /* EnhSnap.before */
    bool opp_fixed;
    bool town;
    bool exact;                /* the newest frame itself (the compare tool): the game's integer projection */
} EnhView;

/* The Aqueduct's rotated rows (race_aq.c's reversed arrays, k = 0 the farthest), its trough (unrotated) and the
 * rotation of a frame. */
typedef struct { s16 lx[15], ly[15], rx[15], ry[15], tlx[15], tly[15], trx[15], try_[15];
                 s16 tnl, tnr, tfl, tfr, tny, tfy; s16 roll, sn, cs, cx, cy; } EnhAqTab;
void enh_aqtab_read(EnhAqTab *t);     /* the rotated arrays, the trough, DS_rot_enable, _sin, _cos, _cx, _cy now */
/* An exact view's rows 0..n (14 front, 7 mirror) rotated into k = n - i, the trough unrotated (as the game keeps
 * it), the view's roll, sin, cos and pivot. */
void enh_aqtab_exact(const EnhView *v, bool mirror, EnhAqTab *t);
int enh_aqtab_err(const EnhAqTab *a, const EnhAqTab *b, int n);   /* the largest difference, rows 0..n and trough */

bool enh_view_at(Uint64 now_ns, EnhView *v);
/* The newest frame's view, not interpolated (the compare tool). */
bool enh_view_newest(EnhView *v);

/* enh_road2.c: modes 0 (town), 1 (street drag), 2 and 4 (the Grudge drag strip), 10h (Mulholland). */
void enh_draw_road(const EnhView *v);
/* The word of track array arr the game reads for index i in road mode `mode` (road_set_seg_ptrs): on the drag
 * strip (modes 2, 4) TRK_C, TRK_L and TRK_R come from TRK_C2, TRK_L2 and TRK_R2 at i (not clamped) and the other
 * arrays from i + C0h; in the other modes array arr at i, clamped to the track. */
u16 enh_track(int mode, u16 arr, int i);
/* road_draw_barrier_walls' colour: abs(n - row - seg + phase) % 10 below 5 pink (0Bh), else white (4); n 14 in the
 * front view, 7 in the mirror, seg the car's segment. */
int enh_wall_colour(int phase, int n, int row, int seg);
/* The game's integer projections: road_proj_x with DS_cam_x = cx (modes other than 8: no clamp), road_proj_y
 * with DS_cam_y = cam_y and DS_view_bounce_y = bounce. */
int enh_proj_x_i(int z, int X, int dx, int cx);
int enh_proj_y_i(int z, int Y, int dy, int cam_y, int bounce);
/* road_project_front's 15 rows of an exact view, all nine tables, from the camera the drawing uses. */
void enh_rows_exact_front(const EnhView *v, EnhRowTab *t);
/* An exact view's town facade or storey as town_walls_front's quad_fill draws it: side 0 left, 1 right; n the near
 * row, f the far one; x the face's outer x, y its bottom, yt its top (integer rows). */
void enh_town_face_exact(int side, int xn, int xf, int yn, int yf, int ytf, int ytn, u8 col);
/* The same clipped to x0..x1, y0..y1 (inclusive): clip_polygon on another clip (the mirror's F0h..13Fh x 16h..2Ah). */
void enh_town_face_clip(int side, int xn, int xf, int yn, int yf, int ytf, int ytn, u8 col, int x0, int y0, int x1, int y1);
/* road_draw_sky's top_band_row: from row 14 down to row 1, a row whose line y is above the ground so far
 * (strictly) lowers the ground to it; bands of rows above the result are not drawn. y: the rows' line y, ground0:
 * the horizon strip's bottom; *ground: the ground's top row. */
int enh_top_band_row(const double y[15], double ground0, double *ground);
/* The centre dash's width of the band whose far row is k: DS_dash_width[k]; in a smooth view interpolated towards
 * row k - 1 as the row nears it (q = 8 frac(pos), 0..8). */
double enh_dash_width(int k, double q, bool exact);
/* An exact view's centre dash of the band from row p (far: its line y yp, edges xlp, xrp) down to row q (near), w
 * wide, as road_draw_band_front draws it: on each line from yp to yq - 1 (and above 66h) the edges stepped as the
 * game steps them (slope16, 1/16 px), x from ((xl + xr) >> 1) - (w >> 1) to that + w, where xl < 13Fh. */
void enh_dash_exact(int xlq, int xrq, int yq, int xlp, int xrp, int yp, int w, u8 col);
/* sky_scroll's shift a (race_road.c): the screen x of the sheet's column 0, for heading h and shift sh (1 the sky,
 * 2 the horizon strip). */
int enh_sky_shift(s16 h, int sh);
/* enh_mirror2.c: the rear-view mirror (road_project_mirror, road_draw_mirror), in its glass F2h..13Dh x 16h..2Ah.
 * Row k (0..7) at segment base - k, depth enh_mirror_z; the flags of a band one segment off (base - k - 1). */
double enh_mirror_z(int k, double q);   /* 8; 8k + q for k 1..4; 40, 48, 56 for k 5..7 (the k == 5 freeze); 8k beyond */
int enh_proj_x_mirror_i(int z, int X, int dx, int cx);      /* road_proj_x_mirror (modes other than 8: no clamp) */
int enh_proj_y_mirror_i(int z, int Y, int dy, int cam_y);   /* road_proj_y_mirror (no bounce) */
/* road_project_mirror's rows 0..7 of an exact view, and YK 0..14 with its quirk (row k >= 1 writes 2k - 1, 2k). */
void enh_rows_exact_mirror(const EnhView *v, EnhRowTab *t);
void enh_draw_mirror(const EnhView *v);
/* enh_hills2.c: Mulholland (road mode 10h). road_draw_sky_mulholland's dy (the climb from 4 segments back): at most 8,
 * at least -8 while the car is not falling off the edge (fall_dir 0). */
int enh_mulholland_sky_dy(int dy, int fall_dir);
/* front_hillsides' screen constants of row `row`, the DS words 7E1Eh, 7E20h, 7E22h, 7E24h and 7E3Ah + 4 (15 - row)
 * (the near rows' run into DS_dash_width), read at run time. */
void enh_hill_consts(int row, s16 *w0, s16 *h0, s16 *w1, s16 *h1, s16 *e);
/* guard_rail's post (Mulholland): drawn when cmod16(phase - row - seg + n, 2) != 0; n 14 in the front view, 7 in
 * the mirror, phase DS_rail_phase_seg (the front's in both), seg the car's segment. */
bool enh_rail_post(int phase, int row, int seg, int n);
/* The captured objects (enhanced.c). A cut is one id's pixels of the frame shown; its kind says how it is placed,
 * its segment (absolute) where in the band loop it is drawn. */
enum { ENH_CUT_ROW, ENH_CUT_OPP, ENH_CUT_MIR, ENH_CUT_STARTER };
enum { ENH_PASS_FRONT, ENH_PASS_MIRROR };
/* Where an object stands in a view: its depth, the screen row of the road under it, and the screen x of world x
 * X as a + b X. ENH_CUT_ROW: an object on the road at the (absolute) segment seg, at its row's depth;
 * ENH_CUT_OPP: the opponent, at the depth opponent_draw gives its picture `sprite` from v->opp_d (|d / 8| + 4,
 * 16 more for the side view 0, 8 more for the rear view 2 close behind), x relative to its lane (seg unused, the
 * view's opp_seg is the game's). */
typedef struct { double z, yg, a, b; bool ok; } EnhObjGeo;
EnhObjGeo enh_obj_geo(const EnhView *v, int kind, int seg, int sprite);   /* the mirror's: enh_obj_geo_mirror */
/* enh_obj_geo of the mirror's cuts: ENH_CUT_MIR and ENH_CUT_STARTER at segment seg (k = base - seg, depth
 * enh_mirror_z, 8k beyond row 7; the starter's bottom pinned at row 27h), ENH_CUT_OPP with picture >= 9 at
 * -d / 8 (opponent_draw: x by proj_x_mirror(z, opp_lat, 10 - dx), the exact view's at opp_sx; no row clamp). */
EnhObjGeo enh_obj_geo_mirror(const EnhView *v, int kind, int seg, int sprite);
bool enh_in_mirror_rect(int x, int y);        /* the body F0h..13Fh x 14h..2Ch, or the mount 110h..11Fh x 12h..13h */
bool enh_in_glass(int x, int y);              /* F2h..13Dh x 16h..2Ah */
bool enh_tag_kept(int id, int opp_sprite, int x, int y);   /* the capture's region rule */
/* A picture starts: no cut drawn yet. */
void enh_cuts_begin(void);
/* Draws every cut not yet drawn in this picture whose pass is `pass` and whose segment is in lo..hi: the
 * opponent when `opp`, the others otherwise. The opponent's pass is the mirror's for pictures 9 and up. */
void enh_cuts_at(const EnhView *v, int pass, int lo, int hi, bool opp);
extern int enh_cut_log[ENH_OBJ_IDS], enh_cut_log_n;   /* ids in the order the last picture drew them (tests) */
/* The frame's cut of `id` is partly hidden: its rectangle holds the foreground, or pixels of a cut drawn after it
 * in the band loop (a nearer segment's, or at its own segment the opponent over the row's objects), or the game's
 * background drawing later in the frame covered some of its pixels (ega_overdrawn: a nearer row's hillside, rail
 * or truss). The compare tool's re-projection check does not judge such cuts. */
bool enh_cut_partly_hidden(int id);

/* The road colours of the compare tool's edges. */
extern const u8 enh_road_colours[2];
extern double enh_ground_y;
/* Page 1 rows 66h..C7h (the sky and horizon sheets) into the view's cache, once per presented frame. */
void enh_sheets_read(void);    /* the first row below the horizon strip of the last frame drawn */
