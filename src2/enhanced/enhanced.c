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
#include "remaster/rm_pack.h"
#include "remaster/rm_tags.h"

EnhOptions enh_opt = { false, 1, ENH_DEFAULT_AA, ENH_DEFAULT_MOTION_DELAY, true };

/* A smoothly interpolated series of samples: [0] = the newest, [1] and [2] the ones before. */
#define SER_N 5
typedef struct {
    Uint64 t[3];
    double v[3][SER_N];
    int n;
    double period;             /* the measured period, smoothed (ns) */
} Series;

static Series frames;          /* per game frame: lat, heading, pos, opp_pos, opp_lat */
static Series phys;            /* per physics step in a race: pos, opp_pos */
static EnhSnap snap;           /* the newest frame's other state */
static bool active;            /* between enh_race_begin and enh_race_end, and enhancing this drive */
static u16 shown_start;        /* plane offset of the page the last road frame was presented in */
static bool presented;         /* a road frame is shown and nothing has replaced the page since */
static int compared;           /* frames the compare tool has logged */

/* The objects of the frame shown: per id (row 1..14, the opponent), the pixels the game drew in the object
 * layer, cut out of the page, and where that row stood in the frame's view. */
#define OBJ_IDS (ENH_OBJ_OPP + 1)
typedef struct {
    EnhImage img;
    int x0, y0;
    int seg;                   /* the row's absolute segment, -1 for the opponent */
    int sprite;                /* the opponent's picture (DS_opp_sprite) */
    bool fixed;                /* drawn where the frame had it: the opponent's side views (opponent_draw places them
                                  at fixed screen spots) */
    EnhObjGeo geo;
    bool used;
} ObjCut;
static ObjCut cuts[OBJ_IDS];
static EnhView cut_view;

#define NOMINAL_FRAME_NS (8 * (double)PIT_DIV_GAME * 1e9 / PIT_HZ)    /* pace_ticks(8) at the game rate */

static void ser_push(Series *s, Uint64 t, const double *v, double nominal)
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
static void ser_at(const Series *s, Uint64 now, double *out)
{
    if (s->n == 1) { memcpy(out, s->v[0], sizeof s->v[0]); return; }
    double nominal = s->period, period = (double)(s->t[0] - s->t[1]);
    if (period < nominal * 0.5 || period > nominal * 2.0) period = nominal;
    double x = ((double)now - (double)s->t[0]) / period + 1.0 - enh_opt.motion_delay / 100.0;
    x = SDL_clamp(x, 0.0, 1.5);
    for (int k = 0; k < SER_N; k++) out[k] = lerp(s->v[1][k], s->v[0][k], x);
    if (s->n == 3 && x < 1.0) {
        double w = x * x * (3 - 2 * x), xo = SDL_min(x + 1.0, 1.5);
        for (int k = 0; k < SER_N; k++) out[k] = lerp(lerp(s->v[2][k], s->v[1][k], xo), out[k], w);
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
    frames.n = phys.n = 0;
    presented = false;
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
    /* The heading is kept as it is (road_project_front: dx = heading - YAW[seg + k]), unwrapped against the last
     * frame, so the view equals the original's at each frame and turns smoothly between frames. */
    double v[SER_N] = { DSS(DS_cam_x), DSS(DS_heading), car_pos(), opp_pos(), DSS(DS_opp_lat) };
    /* A jump (race start, reset, a drive-to entry) starts the motion anew. */
    if (frames.n > 0 && (SDL_fabs(v[2] - frames.v[0][2]) >= 2.0 || SDL_fabs(v[1] - frames.v[0][1]) >= 0x40))
        frames.n = phys.n = 0;
    if (frames.n > 0) v[1] = frames.v[0][1] + (s16)((s16)v[1] - (s16)frames.v[0][1]);
    ser_push(&frames, host_tick_time_ns(), v, NOMINAL_FRAME_NS);
    snap.height = DSS(DS_cur_height);
    snap.cam_y = DSS(DS_cam_y);
    snap.bounce = DSS(DS_view_bounce_y);
    snap.mode = DSS(DS_road_mode);
    snap.town = DSB(DS_racing) == 0;
}

void enh_phys_step(void)
{
    if (!active) return;
    double v[SER_N] = { car_pos(), opp_pos(), 0, 0, 0 };
    if (phys.n > 0 && SDL_fabs(v[0] - phys.v[0][0]) >= 2.0) phys.n = 0;
    ser_push(&phys, host_tick_time_ns(), v, NOMINAL_FRAME_NS * 12 / 8);
}

void enh_draw_begin(void)
{
    if (!active) return;
    u16 page = DSS(DS_back_page) == 1 ? 0x2000 : 0x4000;
    ega_clear_fg(page, ENH_VIEW_Y1 * EGA_BYTES_PER_LINE);
    ega_clear_obj(page, ENH_VIEW_Y1 * EGA_BYTES_PER_LINE);
    ega_set_layer(EGA_LAYER_BG);
}

void enh_mark_fg(int x0, int y0, int x1, int y1)
{
    if (!active) return;
    u16 page = DSS(DS_back_page) == 1 ? 0x2000 : 0x4000;
    u8 *fgm = (u8 *)ega_fg();
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) fgm[(u16)(page + y * EGA_BYTES_PER_LINE + (x >> 3))] |= (u8)(0x80 >> (x & 7));
}

void enh_draw_objects(int id)
{
    if (!active) return;
    if (id <= 0) { ega_set_layer(EGA_LAYER_BG); return; }
    ega_set_obj((u8)id);
    ega_set_layer(EGA_LAYER_OBJ);
}

/* Cuts each id's pixels out of the page just shown. */
static void capture_objects(void)
{
    const u8 *tag = ega_obj();
    int y_end = SDL_min(ENH_VIEW_Y1, ega_line_compare());
    int x0[OBJ_IDS], y0[OBJ_IDS], x1[OBJ_IDS], y1[OBJ_IDS];
    for (int i = 0; i < OBJ_IDS; i++) { x0[i] = y0[i] = 1 << 20; x1[i] = y1[i] = -1; cuts[i].used = false; }
    for (int y = ENH_VIEW_Y0; y < y_end; y++)
        for (int x = 0; x < 320; x++) {
            int id = tag[((u32)(u16)(shown_start + y * EGA_BYTES_PER_LINE + (x >> 3))) * 8 + (x & 7)];
            if (id <= 0 || id >= OBJ_IDS) continue;
            if (x < x0[id]) x0[id] = x;
            if (x > x1[id]) x1[id] = x;
            if (y < y0[id]) y0[id] = y;
            if (y > y1[id]) y1[id] = y;
        }
    enh_view_newest(&cut_view);
    for (int id = 1; id < OBJ_IDS; id++) {
        if (x1[id] < 0) continue;
        ObjCut *c = &cuts[id];
        bool opp = id == ENH_OBJ_OPP;
        c->seg = opp ? -1 : (int)floor(cut_view.pos) + id;
        c->sprite = opp ? DSS(DS_opp_sprite) : 0;
        c->fixed = opp && (c->sprite == 0 || c->sprite == 1);
        c->geo = enh_obj_geo(&cut_view, c->seg, c->sprite);
        if (!c->geo.ok) continue;
        int w = x1[id] - x0[id] + 1, h = y1[id] - y0[id] + 1;
        c->img.px = SDL_realloc(c->img.px, (size_t)w * h);
        c->img.w = w;
        c->img.h = h;
        /* ENH: remaster: the VRAM tags of the cut pixels, each kept where its picture has art and the tag's
         * original colour is the pixel's (as attach_art, enh_sprite.c); NULL when none */
        SDL_free(c->img.rm);
        c->img.rm = rm_pack_active() ? SDL_calloc((size_t)w * h, sizeof *c->img.rm) : NULL;
        int tagged = 0;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                int x = x0[id] + i, y = y0[id] + j;
                u16 b = (u16)(shown_start + y * EGA_BYTES_PER_LINE + (x >> 3));
                int bit = 7 - (x & 7), col = 0;
                for (int k = 0; k < 4; k++) col |= (ega_plane(k)[b] >> bit & 1) << k;
                bool mine = tag[(u32)b * 8 + (x & 7)] == id;
                c->img.px[j * w + i] = mine ? (u8)col : ENH_CLEAR;
                RmTag t = mine && c->img.rm ? rm_vtag(b, bit) : 0;
                RmPic *p = t ? rm_pic(rm_tag_slot(t)) : NULL;
                int u = rm_tag_u(t), v = rm_tag_v(t);
                if (!p || u >= p->w || v >= p->h || p->orig[v * p->w + u] != col) continue;
                c->img.rm[j * w + i] = t;
                tagged++;
            }
        if (!tagged) { SDL_free(c->img.rm); c->img.rm = NULL; }
        c->x0 = x0[id];
        c->y0 = y0[id];
        c->used = true;
    }
}

/* The cut objects at view v: each moved and scaled from its row in the frame's view to the same row in v, the
 * farthest first. */
static void draw_objects(const EnhView *v)
{
    int order[OBJ_IDS], n = 0;
    EnhObjGeo now[OBJ_IDS];
    for (int id = 1; id < OBJ_IDS; id++) {
        if (!cuts[id].used) continue;
        now[id] = cuts[id].fixed ? cuts[id].geo : enh_obj_geo(v, cuts[id].seg, cuts[id].sprite);
        if (!now[id].ok) continue;
        int k = n++;
        while (k > 0 && now[order[k - 1]].z < now[id].z) { order[k] = order[k - 1]; k--; }
        order[k] = id;
    }
    for (int i = 0; i < n; i++) {
        const ObjCut *c = &cuts[order[i]];
        const EnhObjGeo *f = &c->geo, *g = &now[order[i]];
        double s = f->z / g->z;
        double xa = g->a + g->b * ((c->x0 - f->a) / f->b), xb = g->a + g->b * ((c->x0 + c->img.w - f->a) / f->b);
        double ya = g->yg + (c->y0 - f->yg) * s, yb = g->yg + (c->y0 + c->img.h - f->yg) * s;
        enh_draw_image(&c->img, xa, ya, xb, yb);
    }
}

void enh_draw_fg(bool fg)
{
    if (active) ega_set_layer(fg ? EGA_LAYER_FG : EGA_LAYER_BG);
}

static void compare_frame(void);

void enh_frame_presented(void)
{
    if (!active) return;
    ega_set_layer(EGA_LAYER_OFF);
    /* The page drawn by road_frame is shown: its view rows are the road frame. Whatever is written there from
     * now on (messages, the start light, the results) stays the original's picture. */
    shown_start = (u16)(DSS(DS_shown_page) * 0x2000);
    ega_clear_written(shown_start, ENH_VIEW_Y1 * EGA_BYTES_PER_LINE);
    presented = true;
    snap.opp_sprite = DSS(DS_opp_sprite);
    capture_objects();
    if (enh_compare_on()) compare_frame();
}

static void view_of(const double *f, const double *p, EnhView *v)
{
    v->lat = f[0];
    v->heading = f[1];
    v->pos = f[2];
    v->opp_pos = f[3];
    if (p) { v->pos = p[0]; v->opp_pos = p[1]; }
    v->opp_lat = f[4];
    v->opp_sprite = snap.opp_sprite;
    v->town = snap.town;
    v->height = snap.height;
    v->cam_y = snap.cam_y;
    v->bounce = snap.bounce;
    v->mode = snap.mode;
    v->exact = false;
}

bool enh_view_at(Uint64 now_ns, EnhView *v)
{
    if (frames.n == 0) return false;
    double f[SER_N], p[SER_N];
    ser_at(&frames, now_ns, f);
    bool use_phys = !snap.town && phys.n > 0;   /* in a race the cars move with the physics */
    if (use_phys) ser_at(&phys, now_ns, p);
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

/* SR_ENH_COMPARE: the newest frame drawn again at scale 1, aa 1, against the shown page's view rows. */
static void compare_frame(void)
{
    EnhView v;
    if (!enh_view_newest(&v)) return;
    EnhOptions keep = enh_opt;
    enh_opt.res_scale = enh_opt.aa = 1;
    /* ENH: palette indices only: the last composed frame's palette left set would draw the art (ENH_RGB) */
    const u32 *keep_rgb = enh_cur_rgb;
    enh_cur_rgb = NULL;
    enh_buf_setup();
    enh_draw_road(&v);
    draw_objects(&v);
    enh_cur_rgb = keep_rgb;
    static u8 orig[200 * 320], enh[200 * 320], cov[200 * 320], cov_edge[200 * 320];
    const u8 *fgm = ega_fg(), *tag = ega_obj();
    int mirror_cov = 0;
    for (int y = ENH_VIEW_Y0; y < ENH_VIEW_Y1; y++) {
        u16 row = (u16)(shown_start + y * EGA_BYTES_PER_LINE);
        for (int x = 0; x < 320; x++) {
            u16 b = (u16)(row + (x >> 3));
            int bit = 7 - (x & 7), c = 0;
            for (int k = 0; k < 4; k++) c |= (ega_plane(k)[b] >> bit & 1) << k;
            orig[y * 320 + x] = (u8)c;
            enh[y * 320 + x] = enh_buf.px[(y - ENH_VIEW_Y0) * enh_buf.w + x];
            cov[y * 320 + x] = !(fgm[b] >> bit & 1);
            /* the road's edges are measured on the road itself: an object next to the edge is not the edge */
            cov_edge[y * 320 + x] = cov[y * 320 + x] && !tag[(u32)b * 8 + (x & 7)];
            if (cov[y * 320 + x] && x >= 0xF0 && y <= 0x2C) mirror_cov++;
        }
    }
    enh_opt = keep;
    enh_buf_setup();
    EnhCompare c;
    /* Mismatched pixels: in town over the whole view (its sky is fixed), in a race from below the horizon strip
     * (the sky turns slower than the original's, SKY_SCROLL, as Street Rod 1). Road edges: from the ground down
     * (the town's skyline has the road's colour in it). */
    int y_end = SDL_min(ENH_VIEW_Y1, ega_line_compare());   /* below the split the screen shows the dashboard */
    int ground = SDL_clamp((int)ceil(enh_ground_y), ENH_VIEW_Y0, y_end);
    EnhCompare edges;
    enh_compare_rows(orig, enh, cov, v.town ? ENH_VIEW_Y0 : ground, y_end, enh_road_colours, 2, &c);
    enh_compare_rows(orig, enh, cov_edge, ground, y_end, enh_road_colours, 2, &edges);
    c.edge_rows = edges.edge_rows;
    c.edge_err_max = edges.edge_err_max;
    c.mirror_cov = mirror_cov;
    enh_compare_frame(v.mode, &c);
    compared++;
    /* SR_ENH_COMPARE_DUMP=<dir>: every 40th frame, the original (left) and the enhanced view (right) */
    const char *dir = SDL_getenv("SR_ENH_COMPARE_DUMP");
    static int bad_dumped;
    bool bad = c.edge_err_max > 5 && bad_dumped < 5;
    if (dir && *dir && (compared % 40 == 1 || bad)) {
        if (bad) bad_dumped++;
        SDL_Surface *sf = SDL_CreateSurface(640, ENH_VIEW_Y1 - ENH_VIEW_Y0, SDL_PIXELFORMAT_XRGB8888);
        if (sf) {
            for (int y = ENH_VIEW_Y0; y < ENH_VIEW_Y1; y++) {
                u32 *row = (u32 *)((u8 *)sf->pixels + (size_t)(y - ENH_VIEW_Y0) * sf->pitch);
                for (int x = 0; x < 320; x++) {
                    u8 e = enh[y * 320 + x];
                    row[x] = ega_palette[orig[y * 320 + x] & 15];
                    row[320 + x] = cov[y * 320 + x] ? (e < 16 ? ega_palette[e] : 0xFF00FF) : ega_palette[orig[y * 320 + x] & 15] / 2 & 0x7F7F7F;
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
    enh_draw_road(&v);
    draw_objects(&v);
    int y_end = SDL_min(ENH_VIEW_Y1, ega_line_compare());
    enh_resolve(xrgb, rgb, 16, shown_start, y_end);
    enh_compare_shown();
    return true;
}
