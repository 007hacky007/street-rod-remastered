/* Street Rod 2's enhanced road view (enhanced.h): the options, the snapshots of the game's frames, the
 * smooth view between them and the composition. Street Rod 1's src/enhanced/enhanced.c with Street Rod 2's
 * symbols and units; the frame period is measured, not taken from a fixed tick (AdLib music changes the
 * tick rate). */
#include "enhanced/enhanced.h"
#include "enhanced/enh_internal.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "enhanced/enh_compare.h"
#include "game/race_int.h"
#include "host.h"
#include "mem.h"
#include "platform/ega.h"
#include "platform/video.h"

EnhOptions enh_opt = { false, 1, ENH_DEFAULT_AA, ENH_DEFAULT_MOTION_DELAY, true };

static EnhSeries frames;       /* per game frame: the ENH_F_ channels */
static EnhSeries phys;         /* per physics step in a race: pos, opp_pos */
static EnhSnap snap;           /* the newest frame's other state */
static bool active;            /* between enh_race_begin and enh_race_end, and enhancing this drive */
static u16 shown_start;        /* plane offset of the page the last road frame was presented in */
static bool presented;         /* a road frame is shown and nothing has replaced the page since */
static int compared;           /* frames the compare tool has logged */

static int base_layer = EGA_LAYER_BG;   /* the layer enh_draw_objects(0) returns to */

/* The objects of the frame shown: per id (enhanced.h's table), the pixels the game drew in the object layer, cut
 * out of the page, and where that object stood in the frame's view. */
typedef struct {
    EnhImage img;
    int x0, y0;
    int kind;                  /* ENH_CUT_ */
    int seg;                   /* the absolute segment it is drawn at in the band loop */
    int sprite;                /* the opponent's picture (DS_opp_sprite) */
    bool fixed;                /* drawn where the frame had it: the opponent at a fixed screen spot (snap.opp_fixed) */
    EnhObjGeo geo;
    bool used;
    bool hidden;               /* partly hidden in the frame (enh_cut_partly_hidden) */
    bool drawn;                /* in the picture being drawn */
    unsigned gen;              /* the series' generation it was cut in (series_gen) */
} ObjCut;
static ObjCut cuts[ENH_OBJ_IDS];
static EnhView cut_view;
static unsigned series_gen;    /* counts the series' resets (a jump, a new race): cuts of an older one are another road */
int enh_cut_log[ENH_OBJ_IDS], enh_cut_log_n;

#define NOMINAL_FRAME_NS (8 * (double)PIT_DIV_GAME * 1e9 / PIT_HZ)    /* pace_ticks(8) at the game rate */

void enh_ser_push(EnhSeries *s, Uint64 t, const double *v, double nominal)
{
    if (s->n > 0) {
        double d = (double)(t - s->t[0]);
        if (s->period <= 0) s->period = nominal;
        if (d > s->period * 0.3 && d < s->period * 3.0) s->period += (d - s->period) * 0.2;
    } else if (s->period <= 0)
        s->period = nominal;
    memmove(&s->t[1], &s->t[0], sizeof s->t[0] * 2);
    memmove(&s->v[1], &s->v[0], sizeof s->v[0] * 2);
    s->t[0] = t;
    memcpy(s->v[0], v, sizeof s->v[0]);
    if (s->n < 3) s->n++;
}

static double lerp(double a, double b, double x) { return a + (b - a) * x; }

/* As Street Rod 1's ser_at: the newest segment running motion_delay behind, blended from the one before. */
void enh_ser_at(const EnhSeries *s, Uint64 now, double *out)
{
    if (s->n == 1) { memcpy(out, s->v[0], sizeof s->v[0]); return; }
    double nominal = s->period, period = (double)(s->t[0] - s->t[1]);
    if (period < nominal * 0.5 || period > nominal * 2.0) period = nominal;
    double x = ((double)now - (double)s->t[0]) / period + 1.0 - enh_opt.motion_delay / 100.0;
    x = SDL_clamp(x, 0.0, 1.5);
    for (int k = 0; k < ENH_SER_N; k++) out[k] = lerp(s->v[1][k], s->v[0][k], x);
    if (s->n == 3 && x < 1.0) {
        double w = x * x * (3 - 2 * x), xo = SDL_min(x + 1.0, 1.5);
        for (int k = 0; k < ENH_SER_N; k++) out[k] = lerp(lerp(s->v[2][k], s->v[1][k], xo), out[k], w);
    }
}

void enh_ser_snap(EnhSeries *s, int k)
{
    for (int i = 1; i < s->n; i++) s->v[i][k] = s->v[0][k];
}

s16 enh_s16_of(double h) { return (s16)(u16)(s32)floor(h); }

double enh_unwrap(double prev, s16 prev_raw, s16 raw) { return prev + (s16)(u16)(raw - prev_raw); }

void enh_rowtab_read(EnhRowTab *t)
{
    for (int k = 0; k < 15; k++) {
        t->xl[k] = DSS((u16)(DS_row_xl + 2 * k));
        t->xr[k] = DSS((u16)(DS_row_xr + 2 * k));
        t->y[k] = DSS((u16)(DS_row_y + 2 * k));
        t->xol[k] = DSS((u16)(DS_row_xol + 2 * k));
        t->xor_[k] = DSS((u16)(DS_row_xor + 2 * k));
        t->ye[k] = DSS((u16)(DS_row_yeye + 2 * k));
        t->yk[k] = DSS((u16)(DS_row_ykerb + 2 * k));
        t->ybl[k] = DSS((u16)(DS_row_ybank_l + 2 * k));
        t->ybr[k] = DSS((u16)(DS_row_ybank_r + 2 * k));
    }
}

/* As Street Rod 1's: out-of-range values are clamped, not refused. */
void enh_init(bool enabled, int res_scale, int aa, int motion_delay, bool town)
{
    enh_opt.enabled = enabled;
    enh_opt.res_scale = SDL_clamp(res_scale, 1, ENH_MAX_RES_SCALE);
    enh_opt.aa = SDL_clamp(aa, 1, ENH_MAX_AA);
    while (enh_opt.res_scale * enh_opt.aa > ENH_MAX_SAMPLES) enh_opt.aa--;
    enh_opt.motion_delay = SDL_clamp(motion_delay, 0, 100);
    enh_opt.town = town;
    if (enabled && enh_compare_open()) atexit(enh_compare_close);   /* SR_ENH_COMPARE (enh_compare.h) */
}

bool enh_enabled(void) { return enh_opt.enabled; }
int  enh_res_scale(void) { return enh_opt.res_scale; }

/* The road modes drawn enhanced so far: the town (0) and the street drag (1). */
static bool mode_enhanced(s16 mode) { return mode == 0 || mode == 1; }

void enh_race_begin(void)
{
    enh_compare_race(DSS(DS_road_mode));
    series_gen++;
    frames.n = phys.n = 0;
    presented = false;
    base_layer = EGA_LAYER_BG;
    for (int id = 0; id < ENH_OBJ_IDS; id++) cuts[id].used = false;
    enh_cut_log_n = 0;
    enh_images_reset();
    active = enh_opt.enabled && mode_enhanced(DSS(DS_road_mode)) && (DSB(DS_racing) != 0 || enh_opt.town);
}

void enh_race_end(void)
{
    ega_set_layer(EGA_LAYER_OFF);
    active = false;
    presented = false;
    frames.n = phys.n = 0;
}

/* The car's position as the projection sees it: substep is in 1/64 segment, but road_project_front only takes
 * its upper bits (q = substep >> 3), so the original's view moves in steps of 1/8 segment. The smooth view
 * interpolates between those positions. */
static double car_pos(void) { return DSS(DS_seg) + (DSS(DS_substep) >> 3) / 8.0; }
static double opp_pos(void) { return DSS(DS_opp_seg) + DSS(DS_opp_seg_frac64) / 64.0; }

void enh_frame_built(void)
{
    if (!active) return;
    static s16 last_heading;   /* the raw s16 heading of the newest frame */
    double v[ENH_SER_N] = { 0 };
    s16 heading = DSS(DS_heading);
    v[ENH_F_LAT] = DSS(DS_cam_x);
    v[ENH_F_POS] = car_pos();
    v[ENH_F_OPP_POS] = opp_pos();
    v[ENH_F_OPP_LAT] = DSS(DS_opp_lat);
    v[ENH_F_CAM_Y] = DSS(DS_cam_y);
    v[ENH_F_BOUNCE] = DSS(DS_view_bounce_y);
    /* the height above the track where the game reads it (road_set_seg_ptrs: the strip at seg + C0h) */
    int hseg = DSS(DS_seg) + ((DSS(DS_road_mode) & 6) ? 0xC0 : 0);
    v[ENH_F_HOFF] = (s16)(u16)(DSW(DS_cur_height) - TRKW(TRK_HGT, SDL_clamp(hseg, 0, TRK_N - 1)));
    /* A jump (race start, reset, a drive-to entry) starts the motion anew. */
    if (frames.n > 0 && (SDL_fabs(v[ENH_F_POS] - frames.v[0][ENH_F_POS]) >= 2.0
                         || abs((s16)(u16)(heading - last_heading)) >= 0x40)) {
        frames.n = phys.n = 0;
        series_gen++;
    }
    /* The heading is kept as it is (road_project_front: dx = heading - YAW[seg + k]), unwrapped against the last
     * frame, so the view equals the original's at each frame and turns smoothly between frames. */
    v[ENH_F_HEADING] = frames.n > 0 ? enh_unwrap(frames.v[0][ENH_F_HEADING], last_heading, heading) : heading;
    last_heading = heading;
    enh_ser_push(&frames, host_tick_time_ns(), v, NOMINAL_FRAME_NS);
    enh_rowtab_read(&snap.game_front);   /* before road_project_mirror overwrites rows 0..7 and YK */
}

void enh_phys_step(void)
{
    if (!active) return;
    double v[ENH_SER_N] = { car_pos(), opp_pos() };
    if (phys.n > 0 && SDL_fabs(v[0] - phys.v[0][0]) >= 2.0) phys.n = 0;
    enh_ser_push(&phys, host_tick_time_ns(), v, NOMINAL_FRAME_NS * 12 / 8);
}

void enh_draw_begin(void)
{
    if (!active) return;
    u16 page = DSS(DS_back_page) == 1 ? 0x2000 : 0x4000;
    ega_clear_fg(page, ENH_VIEW_Y1 * EGA_BYTES_PER_LINE);
    ega_clear_obj(page, ENH_VIEW_Y1 * EGA_BYTES_PER_LINE);
    base_layer = EGA_LAYER_BG;
    ega_set_layer(EGA_LAYER_BG);
}

void enh_draw_sky(bool on)
{
    if (!active) return;
    ega_set_layer(on ? EGA_LAYER_SKY : base_layer);
}

void enh_mark_fg(int x0, int y0, int x1, int y1)
{
    if (!active) return;
    ega_mark_fg_rect(DSS(DS_back_page) == 1 ? 0x2000 : 0x4000, x0, y0, x1, y1);
}

void enh_draw_objects(int id)
{
    if (!active) return;
    if (id <= 0) { ega_set_layer(base_layer); return; }
    ega_set_obj((u8)id);
    ega_set_layer(EGA_LAYER_OBJ);
}

bool enh_in_mirror_rect(int x, int y)
{
    return (x >= 0xF0 && x <= 0x13F && y >= 0x14 && y <= 0x2C) || (x >= 0x110 && x <= 0x11F && y >= 0x12 && y <= 0x13);
}

bool enh_in_glass(int x, int y) { return x >= 0xF2 && x <= 0x13D && y >= 0x16 && y <= 0x2A; }

/* Front ids and the opponent's front pictures are kept outside the mirror (its backdrop covers what the front
 * drew there), the mirror's ids and the opponent's pictures 9 and up (drawn with the mirror's clip) inside its
 * glass. The Aqueduct's stars and clouds wait for part C. */
bool enh_tag_kept(int id, int opp_sprite, int x, int y)
{
    bool mirror = (id >= ENH_OBJ_MIR(1) && id <= ENH_OBJ_STARTER) || (id == ENH_OBJ_OPP && opp_sprite >= 9);
    bool front = (id >= 1 && id <= 14) || (id == ENH_OBJ_OPP && opp_sprite < 9);
    return mirror ? enh_in_glass(x, y) : front && !enh_in_mirror_rect(x, y);
}

static int cut_kind(int id)
{
    return id == ENH_OBJ_OPP ? ENH_CUT_OPP : id == ENH_OBJ_STARTER ? ENH_CUT_STARTER : id > ENH_OBJ_OPP ? ENH_CUT_MIR : ENH_CUT_ROW;
}

static int cut_pass(const ObjCut *c)
{
    if (c->kind == ENH_CUT_OPP) return c->sprite >= 9 ? ENH_PASS_MIRROR : ENH_PASS_FRONT;
    return c->kind == ENH_CUT_ROW ? ENH_PASS_FRONT : ENH_PASS_MIRROR;
}

/* b is drawn after a in the band loop (enh_draw_road): far to near, at one segment the row's objects first. */
static bool drawn_after(const ObjCut *a, const ObjCut *b)
{
    return b->seg < a->seg || (b->seg == a->seg && a->kind == ENH_CUT_ROW && b->kind == ENH_CUT_OPP);
}

/* The cut of `id` holds, within its rectangle, the foreground or a pixel of a cut of its pass drawn after it. */
static bool cut_covered(int id, const u8 *tag)
{
    const ObjCut *c = &cuts[id];
    const u8 *fgm = ega_fg();
    for (int y = c->y0; y < c->y0 + c->img.h; y++)
        for (int x = c->x0; x < c->x0 + c->img.w; x++) {
            u16 b = (u16)(shown_start + y * EGA_BYTES_PER_LINE + (x >> 3));
            if (fgm[b] >> (7 - (x & 7)) & 1) return true;
            int t = tag[(u32)b * 8 + (x & 7)];
            if (t <= 0 || t == id || t >= ENH_OBJ_IDS || !cuts[t].used || cut_pass(&cuts[t]) != cut_pass(c)) continue;
            if (enh_tag_kept(t, snap.opp_sprite, x, y) && drawn_after(c, &cuts[t])) return true;
        }
    return false;
}

/* Cuts each id's pixels (where enh_tag_kept keeps them) out of the page just shown. */
static void capture_objects(void)
{
    const u8 *tag = ega_obj();
    int y_end = SDL_min(ENH_VIEW_Y1, ega_line_compare());
    int x0[ENH_OBJ_IDS], y0[ENH_OBJ_IDS], x1[ENH_OBJ_IDS], y1[ENH_OBJ_IDS];
    for (int i = 0; i < ENH_OBJ_IDS; i++) { x0[i] = y0[i] = 1 << 20; x1[i] = y1[i] = -1; cuts[i].used = false; }
    s16 opp_sprite = snap.opp_sprite;
    for (int y = ENH_VIEW_Y0; y < y_end; y++)
        for (int x = 0; x < 320; x++) {
            int id = tag[((u32)(u16)(shown_start + y * EGA_BYTES_PER_LINE + (x >> 3))) * 8 + (x & 7)];
            if (id <= 0 || id >= ENH_OBJ_IDS || !enh_tag_kept(id, opp_sprite, x, y)) continue;
            if (x < x0[id]) x0[id] = x;
            if (x > x1[id]) x1[id] = x;
            if (y < y0[id]) y0[id] = y;
            if (y > y1[id]) y1[id] = y;
        }
    enh_view_newest(&cut_view);
    int base = (int)floor(cut_view.pos);
    for (int id = 1; id < ENH_OBJ_IDS; id++) {
        if (x1[id] < 0) continue;
        ObjCut *c = &cuts[id];
        c->kind = cut_kind(id);
        c->seg = c->kind == ENH_CUT_ROW ? base + id
               : c->kind == ENH_CUT_OPP ? snap.opp_seg
               : c->kind == ENH_CUT_MIR ? base - (id - ENH_OBJ_OPP)
               : DSS(DS_starter_seg);
        c->sprite = c->kind == ENH_CUT_OPP ? opp_sprite : 0;
        c->fixed = c->kind == ENH_CUT_OPP && snap.opp_fixed;
        c->geo = enh_obj_geo(&cut_view, c->kind, c->seg, c->sprite);
        if (!c->geo.ok) continue;
        int w = x1[id] - x0[id] + 1, h = y1[id] - y0[id] + 1;
        c->img.px = SDL_realloc(c->img.px, (size_t)w * h);
        c->img.w = w;
        c->img.h = h;
        c->img.rm = NULL;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                int x = x0[id] + i, y = y0[id] + j;
                u16 b = (u16)(shown_start + y * EGA_BYTES_PER_LINE + (x >> 3));
                int bit = 7 - (x & 7), col = 0;
                for (int k = 0; k < 4; k++) col |= (ega_plane(k)[b] >> bit & 1) << k;
                c->img.px[j * w + i] = tag[(u32)b * 8 + (x & 7)] == id && enh_tag_kept(id, opp_sprite, x, y) ? (u8)col : ENH_CLEAR;
            }
        c->x0 = x0[id];
        c->y0 = y0[id];
        c->gen = series_gen;
        c->used = true;
    }
    for (int id = 1; id < ENH_OBJ_IDS; id++)
        if (cuts[id].used) cuts[id].hidden = cut_covered(id, tag);
}

bool enh_cut_partly_hidden(int id) { return id > 0 && id < ENH_OBJ_IDS && cuts[id].used && cuts[id].hidden; }

void enh_cuts_begin(void)
{
    for (int id = 0; id < ENH_OBJ_IDS; id++) cuts[id].drawn = false;
    enh_cut_log_n = 0;
}

/* A cut at view v: moved and scaled from its place in the frame's view to the same place in v. A fixed cut
 * keeps its frame geometry. */
static void draw_cut(const EnhView *v, int id)
{
    ObjCut *c = &cuts[id];
    c->drawn = true;
    EnhObjGeo g = c->fixed ? c->geo : enh_obj_geo(v, c->kind, c->seg, c->sprite);
    if (!g.ok) return;
    const EnhObjGeo *f = &c->geo;
    double s = f->z / g.z;
    double xa = g.a + g.b * ((c->x0 - f->a) / f->b), xb = g.a + g.b * ((c->x0 + c->img.w - f->a) / f->b);
    double ya = g.yg + (c->y0 - f->yg) * s, yb = g.yg + (c->y0 + c->img.h - f->yg) * s;
    enh_draw_image(&c->img, xa, ya, xb, yb);
    if (enh_cut_log_n < ENH_OBJ_IDS) enh_cut_log[enh_cut_log_n++] = id;
}

void enh_cuts_at(const EnhView *v, int pass, int lo, int hi, bool opp)
{
    for (int id = 1; id < ENH_OBJ_IDS; id++) {
        const ObjCut *c = &cuts[id];
        if (!c->used || c->drawn || (c->kind == ENH_CUT_OPP) != opp || cut_pass(c) != pass) continue;
        if (c->seg >= lo && c->seg <= hi) draw_cut(v, id);
    }
}

/* One picture of view v: the front pass draws the road and the cuts at their rows (enh_road2.c). */
static void draw_view(const EnhView *v)
{
    enh_cuts_begin();
    enh_draw_road(v);
}

void enh_draw_fg(bool fg)
{
    if (!active) return;
    base_layer = fg ? EGA_LAYER_FG : EGA_LAYER_BG;
    ega_set_layer(base_layer);
}

static void compare_frame(void);
static void predict_cuts(void);

int enh_opp_drawn_sx(int sx, int width)
{
    s16 dx = (s16)(sx - (width >> 1));
    return sx - (dx & 7);
}

/* The newest frame's state that is not interpolated, after the frame was drawn. */
static void snap_fill(void)
{
    snap.mode = DSS(DS_road_mode);
    snap.town = DSB(DS_racing) == 0;
    snap.opp_sprite = DSS(DS_opp_sprite);
    snap.opp_seg = DSS(DS_opp_seg);
    snap.opp_d = DSS(DS_opp_last_d);
    snap.opp_sx = DSS(DS_opp_last_sx);
    if (snap.opp_sprite >= 2 && snap.opp_sprite <= 8 && snap.mode != 8) {   /* drv_blit_masked: whole bytes */
        FarPtr pic = ds_far(DS_opp_pic);
        snap.opp_sx = (s16)enh_opp_drawn_sx(snap.opp_sx, snap.opp_sprite > 3 ? desc_w(pic) : bitmap_visible_width(pic));
    }
    /* opponent_draw puts these at fixed screen spots */
    snap.opp_fixed = snap.opp_sprite == 0 || snap.opp_sprite == 1 || DSW(DS_opp_contact) != 0
                     || PHW(PLAYER, PH_RESULT) == 3;
    snap.fall_dir = DSS(DS_fall_dir);
    snap.fallen = DSW(DS_fallen) != 0;
    snap.rail_phase = DSS(DS_rail_phase_seg);
    snap.start_seg = DSS(DS_start_seg);
    snap.ground_top_y = DSS(DS_ground_top_y);
    snap.top_band_row = DSS(DS_top_band_row);
    snap.police = DSS(DS_police_state);
    enh_rowtab_read(&snap.game_mirror);
}

void enh_frame_presented(void)
{
    if (!active) return;
    ega_set_layer(EGA_LAYER_OFF);
    /* After the cliff fall road_draw_front draws nothing (it returns before enh_draw_begin): the page keeps the
     * original's picture, and nothing of it is cut or composed. */
    if (DSW(DS_fallen)) {
        snap.fallen = true;
        presented = false;
        enh_compare_skipped(DSS(DS_road_mode));
        return;
    }
    /* The page drawn by road_frame is shown: its view rows are the road frame. Whatever is written there from
     * now on (messages, the start light, the results) stays the original's picture. */
    shown_start = (u16)(DSS(DS_shown_page) * 0x2000);
    ega_clear_written(shown_start, ENH_VIEW_Y1 * EGA_BYTES_PER_LINE);
    presented = true;
    snap_fill();
    enh_sheets_read();
    if (enh_compare_on()) predict_cuts();
    capture_objects();
    if (enh_compare_on()) compare_frame();
}

static void view_of(const double *f, const double *p, EnhView *v)
{
    v->lat = f[ENH_F_LAT];
    v->heading = f[ENH_F_HEADING];
    v->pos = f[ENH_F_POS];
    v->opp_pos = f[ENH_F_OPP_POS];
    if (p) { v->pos = p[0]; v->opp_pos = p[1]; }
    v->opp_lat = f[ENH_F_OPP_LAT];
    v->cam_y = f[ENH_F_CAM_Y];
    v->hoff = f[ENH_F_HOFF];
    v->bounce = f[ENH_F_BOUNCE];
    v->roll = f[ENH_F_ROLL];
    v->aq_dy = f[ENH_F_AQ_DY];
    for (int k = 0; k < 6; k++) v->aq_def[k] = f[ENH_F_DEF0 + k];
    /* the depth the opponent was drawn at, moved on by the cars' motion since the newest frame */
    v->opp_d = snap.opp_d + 64.0 * ((v->opp_pos - v->pos) - (frames.v[0][ENH_F_OPP_POS] - frames.v[0][ENH_F_POS]));
    v->opp_sprite = snap.opp_sprite;
    v->opp_seg = snap.opp_seg;
    v->opp_sx = snap.opp_sx;
    v->opp_fixed = snap.opp_fixed;
    v->fall_dir = snap.fall_dir;
    v->start_seg = snap.start_seg;
    v->rail_phase = snap.rail_phase;
    v->ground_top_y = snap.ground_top_y;
    v->town = snap.town;
    v->mode = snap.mode;
    v->exact = false;
}

bool enh_view_at(Uint64 now_ns, EnhView *v)
{
    if (frames.n == 0) return false;
    double f[ENH_SER_N], p[ENH_SER_N];
    enh_ser_at(&frames, now_ns, f);
    bool use_phys = !snap.town && phys.n > 0;   /* in a race the cars move with the physics */
    if (use_phys) enh_ser_at(&phys, now_ns, p);
    view_of(f, use_phys ? p : NULL, v);
    return true;
}

bool enh_view_newest(EnhView *v)
{
    if (frames.n == 0) return false;
    view_of(frames.v[0], NULL, v);
    v->exact = true;
    return true;
}

/* ---------------------------------------------------------------- the compare tool (enh_compare.h) */

/* The re-projection check: before the new capture, each old cut placed in the new frame's exact view by the
 * drawing's own mapping (draw_cut); after it, compared with the new cut of the same kind and segment. */
typedef struct { bool ok; int kind, seg, sprite; double bottom, cx; } CutPred;
static CutPred preds[ENH_OBJ_IDS];

/* A cut the check can judge: placed by its geometry (not fixed), not cut off by its pass's border, and not partly
 * hidden: neither its rectangle holds what was drawn after it (enh_cut_partly_hidden) nor is its bottom more than
 * 1 px above the road row it stands on (nearer road, a wall). */
static bool cut_judged(const ObjCut *c, int y_end)
{
    if (!c->used || c->fixed) return false;
    int x1 = c->x0 + c->img.w - 1, y1 = c->y0 + c->img.h - 1;
    if (cut_pass(c) == ENH_PASS_FRONT) {
        if (c->x0 <= 0 || x1 >= 0x13F || c->y0 <= ENH_VIEW_Y0 || y1 >= y_end - 1) return false;
    } else if (c->x0 <= 0xF2 || x1 >= 0x13D || c->y0 <= 0x16 || y1 >= 0x2A)
        return false;
    return !c->hidden && c->y0 + c->img.h >= c->geo.yg - 1.0;
}

static int compare_y_end(void) { return SDL_min(ENH_VIEW_Y1, ega_line_compare()); }

static void predict_cuts(void)
{
    EnhView v;
    bool have = enh_view_newest(&v);
    for (int id = 1; id < ENH_OBJ_IDS; id++) {
        const ObjCut *c = &cuts[id];
        CutPred *p = &preds[id];
        p->ok = false;
        if (!have || c->gen != series_gen || !cut_judged(c, compare_y_end())) continue;
        EnhObjGeo g = enh_obj_geo(&v, c->kind, c->seg, c->sprite);
        if (!g.ok) continue;
        const EnhObjGeo *f = &c->geo;
        p->ok = true;
        p->kind = c->kind;
        p->seg = c->seg;
        p->sprite = c->sprite;
        p->bottom = g.yg + (c->y0 + c->img.h - f->yg) * (f->z / g.z);
        p->cx = g.a + g.b * ((c->x0 + c->img.w / 2.0 - f->a) / f->b);
    }
}

/* the largest error of the predicted cuts against the new ones; -1 when none matched */
static int reprojection_err(void)
{
    double err = -1;
    for (int i = 1; i < ENH_OBJ_IDS; i++) {
        const CutPred *p = &preds[i];
        if (!p->ok) continue;
        for (int id = 1; id < ENH_OBJ_IDS; id++) {
            const ObjCut *c = &cuts[id];
            if (!cut_judged(c, compare_y_end()) || c->kind != p->kind || c->seg != p->seg) continue;
            if (c->kind == ENH_CUT_OPP && c->sprite != p->sprite) continue;
            double e = SDL_fabs(p->bottom - (c->y0 + c->img.h));
            if (c->kind == ENH_CUT_OPP) e = SDL_max(e, SDL_fabs(p->cx - (c->x0 + c->img.w / 2.0)));
            if (e > err) err = e;
        }
    }
    return err < 0 ? -1 : (int)floor(err + 0.5);
}

/* the largest difference between the enhanced exact rows and the game's row tables */
static int rows_err(const EnhRowTab *a, const EnhRowTab *b)
{
    const s16 *ta[9] = { a->xl, a->xr, a->y, a->xol, a->xor_, a->ye, a->yk, a->ybl, a->ybr };
    const s16 *tb[9] = { b->xl, b->xr, b->y, b->xol, b->xor_, b->ye, b->yk, b->ybl, b->ybr };
    int err = 0;
    for (int t = 0; t < 9; t++)
        for (int k = 0; k < 15; k++) err = SDL_max(err, abs(ta[t][k] - tb[t][k]));
    return err;
}

#define NOT_ROAD 0xFD              /* a sky pixel in the edge pass: covered, never the road */

/* SR_ENH_COMPARE: the newest frame drawn again at scale 1, aa 1, against the shown page's view rows. */
static void compare_frame(void)
{
    EnhView v;
    if (!enh_view_newest(&v)) return;
    EnhOptions keep = enh_opt;
    enh_opt.res_scale = enh_opt.aa = 1;
    enh_buf_setup();
    draw_view(&v);
    static u8 orig[200 * 320], enh[200 * 320], orig_e[200 * 320], enh_e[200 * 320];
    static u8 cov_f[200 * 320], cov_fe[200 * 320], cov_m[200 * 320], cov_me[200 * 320];
    const u8 *fgm = ega_fg(), *tag = ega_obj();
    int mirror_cov = 0;
    for (int y = ENH_VIEW_Y0; y < ENH_VIEW_Y1; y++) {
        u16 row = (u16)(shown_start + y * EGA_BYTES_PER_LINE);
        for (int x = 0; x < 320; x++) {
            u16 b = (u16)(row + (x >> 3));
            int bit = 7 - (x & 7), c = 0, i = y * 320 + x;
            for (int k = 0; k < 4; k++) c |= (ega_plane(k)[b] >> bit & 1) << k;
            u8 t = tag[(u32)b * 8 + (x & 7)];
            bool cov = !(fgm[b] >> bit & 1), obj = t > 0 && t < ENH_OBJ_IDS, sky = t == EGA_TAG_SKY;
            orig[i] = (u8)c;
            enh[i] = enh_buf.px[(y - ENH_VIEW_Y0) * enh_buf.w + x];
            /* the edges: the road itself (an object next to the edge is not the edge, the sky never the road) */
            orig_e[i] = sky ? NOT_ROAD : orig[i];
            enh_e[i] = sky ? NOT_ROAD : enh[i];
            cov_f[i] = cov && !enh_in_mirror_rect(x, y);
            cov_fe[i] = cov_f[i] && !obj;
            cov_m[i] = cov && enh_in_glass(x, y);
            cov_me[i] = cov_m[i] && !obj;
            mirror_cov += cov_m[i];
        }
    }
    enh_opt = keep;
    enh_buf_setup();
    /* Front: mismatch over the whole view, edges from the ground down (in town the skyline has the road's colour
     * in it; road_draw_sky does not write DS_ground_top_y there). Mirror: its glass, edges from its far row. */
    int y_end = compare_y_end();   /* below the split the screen shows the dashboard */
    int ground = SDL_clamp(v.town ? 0x3F : snap.ground_top_y, ENH_VIEW_Y0, y_end);
    int m_ground = SDL_clamp(snap.game_mirror.y[7], 0x16, 0x2B);
    static const u8 town_mirror_road[1] = { 4 };    /* the kerbs: the town mirror leaves its road unfilled */
    const u8 *m_road = v.town ? town_mirror_road : enh_road_colours;
    int m_n = v.town ? 1 : 2;
    EnhCompare c, e, mc, me;
    enh_compare_rows(orig, enh, cov_f, ENH_VIEW_Y0, y_end, enh_road_colours, 2, &c);
    enh_compare_log_rows(true);
    enh_compare_rows(orig_e, enh_e, cov_fe, ground, y_end, enh_road_colours, 2, &e);
    enh_compare_rows(orig_e, enh_e, cov_me, m_ground, 0x2B, m_road, m_n, &me);
    enh_compare_log_rows(false);
    enh_compare_rows(orig, enh, cov_m, 0x16, 0x2B, m_road, m_n, &mc);
    c.edge_rows = e.edge_rows;
    c.edge_err_max = e.edge_err_max;
    c.m_rows = mc.rows;
    c.m_edge_rows = me.edge_rows;
    c.m_edge_err_max = me.edge_err_max;
    c.m_pixels = mc.pixels;
    c.m_mismatched = mc.mismatched;
    c.mirror_cov = mirror_cov;
    EnhRowTab rows;
    enh_rows_exact_front(&v, &rows);
    c.geo_err = rows_err(&rows, &snap.game_front);
    c.obj_err = reprojection_err();
    c.police = snap.police;
    c.opp_sprite = snap.opp_sprite;
    enh_compare_frame(v.mode, &c);
    compared++;
    /* SR_ENH_COMPARE_DUMP=<dir>: every 40th frame, the original (left) and the enhanced view (right) */
    const char *dir = SDL_getenv("SR_ENH_COMPARE_DUMP");
    static int bad_dumped;
    bool bad = (c.edge_err_max > 1 || c.geo_err > 0 || c.obj_err > 1) && bad_dumped < 10;
    if (dir && *dir && (compared % 40 == 1 || bad)) {
        if (bad) bad_dumped++;
        SDL_Surface *sf = SDL_CreateSurface(640, ENH_VIEW_Y1 - ENH_VIEW_Y0, SDL_PIXELFORMAT_XRGB8888);
        if (sf) {
            for (int y = ENH_VIEW_Y0; y < ENH_VIEW_Y1; y++) {
                u32 *row = (u32 *)((u8 *)sf->pixels + (size_t)(y - ENH_VIEW_Y0) * sf->pitch);
                for (int x = 0; x < 320; x++) {
                    int i = y * 320 + x;
                    u8 en = enh[i];
                    bool cov = cov_f[i] || cov_m[i];
                    row[x] = ega_palette[orig[i] & 15];
                    row[320 + x] = cov ? (en < 16 ? ega_palette[en] : 0xFF00FF) : ega_palette[orig[i] & 15] / 2 & 0x7F7F7F;
                }
            }
            char path[1024];
            SDL_snprintf(path, sizeof path, "%s/cmp%04d_mode%d.bmp", dir, compared, v.mode);
            SDL_SaveBMP(sf, path);
            SDL_DestroySurface(sf);
        }
    }
}

bool enh_compose(u32 *xrgb, const u32 rgb[16])
{
    if (!active || !presented || ega_start() != shown_start) return false;
    EnhView v;
    if (!enh_view_at(SDL_GetTicksNS(), &v)) return false;
    enh_cur_rgb = rgb;
    draw_view(&v);
    int y_end = SDL_min(ENH_VIEW_Y1, ega_line_compare());
    enh_resolve(xrgb, rgb, 16, shown_start, y_end);
    enh_compare_shown();
    return true;
}
