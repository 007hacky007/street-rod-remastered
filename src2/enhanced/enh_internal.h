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
    s16 opp_sx;                /* DS_opp_last_sx: the screen x it was drawn at */
    bool opp_fixed;            /* drawn at a fixed screen spot: the side views 0 and 1, contact, result 3 */
    s16 fall_dir;              /* DS_fall_dir */
    bool fallen;               /* DS_fallen: no front view drawn */
    s16 rail_phase;            /* DS_rail_phase_seg */
    s16 start_seg;             /* DS_start_seg */
    s16 ground_top_y;          /* DS_ground_top_y */
    s16 top_band_row;          /* DS_top_band_row */
    s16 police;                /* DS_police_state */
    EnhRowTab game_front;      /* read at enh_frame_built: the mirror overwrites rows 0..7 and all of YK later */
    EnhRowTab game_mirror;     /* read at enh_frame_presented */
} EnhSnap;

/* The smooth view at a moment: the road-curve part of the heading comes from YAW at pos. Positions in
 * segments (substep and the opponent's fraction are in 1/64 segment). */
typedef struct {
    double pos, lat, heading, opp_pos, opp_lat;   /* heading: DS_heading, unwrapped (not relative to the road) */
    double cam_y, hoff, bounce;                   /* DS_cam_y; DS_cur_height - HGT[seg]; DS_view_bounce_y */
    double roll, aq_dy, aq_def[6];                /* part C (0 in part B) */
    double opp_d;              /* the opponent's depth in 1/64 segment: DS_opp_last_d at the newest frame, moved on
                                  by the cars' motion since then in a smooth view */
    s16 mode, opp_sprite;
    s16 opp_seg, opp_sx, fall_dir, start_seg, rail_phase, ground_top_y;
    bool opp_fixed;
    bool town;
    bool exact;                /* the newest frame itself (the compare tool): the game's integer projection */
} EnhView;

bool enh_view_at(Uint64 now_ns, EnhView *v);
/* The newest frame's view, not interpolated (the compare tool). */
bool enh_view_newest(EnhView *v);

/* enh_road2.c: modes 0 (town) and 1 (street drag). */
void enh_draw_road(const EnhView *v);
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
EnhObjGeo enh_obj_geo(const EnhView *v, int kind, int seg, int sprite);   /* MIR and STARTER: ok = false until the mirror is drawn */
bool enh_in_mirror_rect(int x, int y);        /* the body F0h..13Fh x 14h..2Ch, or the mount 110h..11Fh x 12h..13h */
bool enh_in_glass(int x, int y);              /* F2h..13Dh x 16h..2Ah */
bool enh_tag_kept(int id, int opp_sprite, int x, int y);   /* the capture's region rule */
/* A picture starts: no cut drawn yet. */
void enh_cuts_begin(void);
/* Draws every cut not yet drawn in this picture whose pass is `pass` and whose segment is in lo..hi: the
 * opponent when `opp`, the others otherwise. The opponent's pass is the mirror's for pictures 9 and up. */
void enh_cuts_at(const EnhView *v, int pass, int lo, int hi, bool opp);
extern int enh_cut_log[ENH_OBJ_IDS], enh_cut_log_n;   /* ids in the order the last picture drew them (tests) */

/* The road colours of the compare tool's edges. */
extern const u8 enh_road_colours[2];
extern double enh_ground_y;
/* Page 1 rows 66h..C7h (the sky and horizon sheets) into the view's cache, once per presented frame. */
void enh_sheets_read(void);    /* the first row below the horizon strip of the last frame drawn */
