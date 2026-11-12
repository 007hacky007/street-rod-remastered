/* test_sr2_enh.c -- Street Rod 2's enhanced race view (src2/enhanced, src2/platform ENH parts): the output
 * scale, the coverage layers, the options.
 *   test_sr2_enh <SR2 game dir>
 * Headless with the SDL dummy drivers. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "enhanced/enhanced.h"
#include "enhanced/enh_compare.h"
#include "enhanced/enh_internal.h"
#include "game/race_int.h"
#include "host.h"
#include "mem.h"
#include "platform/ega.h"
#include "platform/vga.h"

static int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
            failures++;                                                                \
        }                                                                              \
    } while (0)
#define RUN(t) do { int b = failures; t(); printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)

static u32 small[320 * 200], big[1280 * 800];

/* A pattern in all four planes: every pixel's colour depends on its place. */
static void pattern(void)
{
    for (int k = 0; k < 4; k++)
        for (int o = 0; o < 200 * EGA_BYTES_PER_LINE; o++) ega_plane(k)[o] = (u8)(o * (k + 3) + k * 0x55);
}

static void sr2_enh_scale(void)
{
    enh_init(true, 1, 2, 100, true);
    ega_init();
    pattern();
    ega_show();
    CHECK(ega_compose(small));
    enh_init(true, 4, 2, 100, true);
    ega_show();                                     /* the output scale is taken here */
    CHECK(ega_compose(big));
    int bad = 0;
    for (int y = 0; y < 800 && !bad; y++)
        for (int x = 0; x < 1280; x++)
            if (big[y * 1280 + x] != small[(y / 4) * 320 + x / 4]) { bad = 1; printf("  differs at %d,%d\n", x, y); break; }
    CHECK(!bad);
}

static void sr2_enh_layers(void)
{
    enh_init(true, 1, 2, 100, true);
    ega_init();
    vga_mode_0d();
    vga_out(0x3C4, 2); vga_out(0x3C5, 0x0F);       /* map mask: all planes */
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);       /* bit mask: all pixels */
    ega_clear_written(0, 0x2000);
    ega_clear_fg(0, 0x2000);
    ega_set_layer(EGA_LAYER_BG);
    vga_wr(10, 0xAA);
    CHECK(ega_written()[10] == 0xFF);
    CHECK(ega_fg()[10] == 0);
    ega_set_layer(EGA_LAYER_FG);
    vga_wr(11, 0x0F);
    CHECK(ega_written()[11] == 0xFF);
    CHECK(ega_fg()[11] == 0x0F);                   /* only the pixels whose colour changed */
    ega_set_layer(EGA_LAYER_BG);
    vga_wr(11, 0x00);                               /* the background covers the foreground again */
    CHECK(ega_fg()[11] == 0);
    ega_set_layer(EGA_LAYER_OFF);
    ega_clear_written(0, 0x2000);
    CHECK(ega_written()[10] == 0 && ega_written()[11] == 0);
}

/* The object layer: an object's pixels carry its id; the background drawn over them later clears the tags. */
static void sr2_enh_object_tags(void)
{
    enh_init(true, 1, 2, 100, true);
    ega_init();
    vga_mode_0d();
    vga_out(0x3C4, 2); vga_out(0x3C5, 0x0F);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);
    ega_clear_obj(0, 0x2000);
    ega_set_layer(EGA_LAYER_BG);
    vga_wr(20, 0x00);
    ega_set_obj(7);
    ega_set_layer(EGA_LAYER_OBJ);
    vga_wr(20, 0xF0);                               /* the left four pixels change colour */
    const u8 *t = ega_obj();
    CHECK(t[20 * 8 + 0] == 7 && t[20 * 8 + 3] == 7);
    CHECK(t[20 * 8 + 4] == 0 && t[20 * 8 + 7] == 0);
    CHECK(!(ega_fg()[20] & 0xF0));                  /* an object is not foreground */
    ega_set_layer(EGA_LAYER_BG);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xC0);       /* nearer road over the two leftmost */
    vga_wr(20, 0x00);
    CHECK(t[20 * 8 + 0] == 0 && t[20 * 8 + 1] == 0 && t[20 * 8 + 2] == 7);
    ega_set_layer(EGA_LAYER_OFF);
}

/* A foreground write over an object clears its tags: the A-pillar or the hood over an object stays the
 * foreground's, not part of the object's cut. */
static void sr2_enh_fg_clears_tags(void)
{
    enh_init(true, 1, 2, 100, true);
    ega_init();
    vga_mode_0d();
    vga_out(0x3C4, 2); vga_out(0x3C5, 0x0F);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);
    ega_clear_obj(0, 0x2000);
    ega_set_layer(EGA_LAYER_BG);
    vga_wr(30, 0x00);
    ega_set_obj(5);
    ega_set_layer(EGA_LAYER_OBJ);
    vga_wr(30, 0xFF);
    ega_set_layer(EGA_LAYER_FG);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0x0F);
    (void)vga_rd(30);                               /* the latches keep the masked pixels */
    vga_wr(30, 0x00);                               /* the foreground over the right four */
    const u8 *t = ega_obj();
    CHECK(t[30 * 8 + 0] == 5 && t[30 * 8 + 3] == 5);
    CHECK(t[30 * 8 + 4] == 0 && t[30 * 8 + 7] == 0);
    ega_set_layer(EGA_LAYER_OFF);
}

static EnhView view_at(double pos)
{
    EnhView v;
    memset(&v, 0, sizeof v);
    v.pos = pos;
    v.lat = 0xA00;
    v.cam_y = 0x76C;
    v.opp_pos = pos + 5;
    v.opp_lat = 0x900;
    return v;
}

/* An object stays at its own segment when the view crosses into the next: its depth moves on smoothly. */
static void sr2_enh_obj_geo_continuous(void)
{
    EnhView a = view_at(65.99), b = view_at(66.01);
    EnhObjGeo ga = enh_obj_geo(&a, 75, -1), gb = enh_obj_geo(&b, 75, -1);
    printf("  z %.3f -> %.3f\n", ga.z, gb.z);
    CHECK(ga.ok && gb.ok);
    CHECK(SDL_fabs(ga.z - gb.z) < 0.5);
}

/* The opponent at the original's depth (opponent_draw): |d / 8| + 4, with 16 more for the side view (picture
 * 0) and 8 more for the rear view close behind (picture 2, d / 8 < -2). */
static void sr2_enh_opp_depth(void)
{
    EnhView v = view_at(100.0);
    v.opp_pos = 100.0 - 40.0 / 64;                 /* d = -40: just behind */
    v.opp_d = -40;
    EnhObjGeo g = enh_obj_geo(&v, -1, 2);
    printf("  rear, d -40: ok %d z %.2f\n", g.ok, g.z);
    CHECK(g.ok && SDL_fabs(g.z - 7.0) < 1e-6);
    v.opp_pos = 100.0 + 8.0 / 64;                  /* d = 8, the side view */
    v.opp_d = 8;
    g = enh_obj_geo(&v, -1, 0);
    CHECK(g.ok && SDL_fabs(g.z - 21.0) < 1e-6);
    v.opp_pos = 103.0;                             /* d = 192, ahead */
    v.opp_d = 192;
    g = enh_obj_geo(&v, -1, 5);
    CHECK(g.ok && SDL_fabs(g.z - 28.0) < 1e-6);
}

/* A channel that jumps (enh_ser_snap) takes its new value at once; the others still interpolate. */
static void sr2_enh_series_snap(void)
{
    enh_init(false, 1, 1, 100, true);
    EnhSeries s;
    memset(&s, 0, sizeof s);
    double v[ENH_SER_N] = { 0 };
    Uint64 t0 = 1000000000ull, ms = 10000000ull;
    v[0] = 0; v[1] = 0;
    enh_ser_push(&s, t0, v, 1e7);
    v[0] = 10; v[1] = 0;
    enh_ser_push(&s, t0 + ms, v, 1e7);
    v[0] = 20; v[1] = 100;
    enh_ser_push(&s, t0 + 2 * ms, v, 1e7);
    enh_ser_snap(&s, 1);
    double out[ENH_SER_N];
    enh_ser_at(&s, t0 + 2 * ms + ms / 2, out);
    printf("  ch0 %.3f ch1 %.3f\n", out[0], out[1]);
    CHECK(out[0] > 10 && out[0] < 20);
    CHECK(out[1] == 100);
}

/* The heading is unwrapped across the s16 range: a long race does not reset the motion. */
static void sr2_enh_heading_wrap(void)
{
    CHECK(enh_unwrap(32760.0, 32760, (s16)0x8002) == 32770.0);
    CHECK(enh_s16_of(32770.0) == (s16)0x8002);
    CHECK(enh_s16_of(-40000.5) == (s16)(s32)floor(-40000.5));
    s16 keep_mode = DSS(DS_road_mode), keep_seg = DSS(DS_seg), keep_sub = DSS(DS_substep), keep_head = DSS(DS_heading);
    u8 keep_racing = DSB(DS_racing);
    enh_init(true, 1, 1, 100, true);
    DSS(DS_road_mode) = 1;
    DSB(DS_racing) = 1;
    DSS(DS_seg) = 100;
    DSS(DS_substep) = 0;
    enh_race_begin();
    const u16 h[4] = { 0x7FF0, 0x7FF8, 0x8000, 0x8008 };
    for (int i = 0; i < 4; i++) { DSW(DS_heading) = h[i]; enh_frame_built(); }
    EnhView v;
    CHECK(enh_view_newest(&v));
    printf("  heading %.1f\n", v.heading);
    CHECK(v.heading == 32776.0);
    enh_race_end();
    DSS(DS_road_mode) = keep_mode; DSS(DS_seg) = keep_seg; DSS(DS_substep) = keep_sub; DSS(DS_heading) = keep_head;
    DSB(DS_racing) = keep_racing;
}

/* An object's row at a game frame is the game's own (integer projection, the frame's height), and between
 * frames it moves on smoothly on a slope. */
static void sr2_enh_obj_geo_exact_slope(void)
{
    u16 keep[31];
    for (int i = 90; i <= 120; i++) { keep[i - 90] = TRKW(TRK_HGT, i); TRKW(TRK_HGT, i) = (u16)(2 * i); }
    s16 keep_cam = DSS(DS_cam_y), keep_b = DSS(DS_view_bounce_y), keep_mode = DSS(DS_road_mode);
    DSS(DS_cam_y) = 0x76C;
    DSS(DS_view_bounce_y) = 0;
    DSS(DS_road_mode) = 1;
    EnhView v = view_at(100.875);
    v.mode = 1;
    v.exact = true;
    v.hoff = 0;
    EnhObjGeo g = enh_obj_geo(&v, 105, 0);
    double want = road_proj_y(37, 2000, (s16)(200 - 210)) + 0.5;     /* 37 = 8 * 5 + 4 - 7 */
    printf("  exact yg %.2f, the game's %.2f\n", g.yg, want);
    CHECK(g.ok && g.yg == want);
    EnhView a = view_at(100.99), b = view_at(101.01);
    a.mode = b.mode = 1;
    EnhObjGeo ga = enh_obj_geo(&a, 105, 0), gb = enh_obj_geo(&b, 105, 0);
    printf("  smooth yg %.3f -> %.3f\n", ga.yg, gb.yg);
    CHECK(ga.ok && gb.ok && SDL_fabs(ga.yg - gb.yg) < 0.5);
    for (int i = 90; i <= 120; i++) TRKW(TRK_HGT, i) = keep[i - 90];
    DSS(DS_cam_y) = keep_cam; DSS(DS_view_bounce_y) = keep_b; DSS(DS_road_mode) = keep_mode;
}

/* The opponent stands at the depth the game drew it at (DS_opp_last_d), not at one computed from opp_pos. */
static void sr2_enh_opp_drawn_d(void)
{
    EnhView v = view_at(100.0);
    v.mode = 1;
    v.exact = true;
    v.opp_pos = v.pos + 10;                         /* inconsistent on purpose: d is what was drawn */
    v.opp_seg = 104;
    v.opp_sx = 160;
    v.opp_d = 300;
    EnhObjGeo g = enh_obj_geo(&v, -1, 5);
    CHECK(g.ok && g.z == 41);
    v.opp_d = -20;
    g = enh_obj_geo(&v, -1, 2);
    CHECK(g.ok && g.z == 6);
    v.opp_d = -40;
    g = enh_obj_geo(&v, -1, 2);
    CHECK(g.ok && g.z == 7);
    EnhView a = view_at(100.0), b = view_at(100.125);
    a.mode = b.mode = 1;
    a.opp_d = 300;
    b.opp_d = 292;                                  /* the car 1/8 segment on */
    EnhObjGeo ga = enh_obj_geo(&a, -1, 5), gb = enh_obj_geo(&b, -1, 5);
    printf("  smooth z %.2f, %.2f\n", ga.z, gb.z);
    CHECK(ga.ok && gb.ok && ga.z == 41.5 && gb.z == 40.5);
}

/* After the cliff fall the game draws no front view: the enhanced view is not composed over the stale page. */
static void sr2_enh_fallen_not_composed(void)
{
    static const u32 rgb[16];
    s16 keep_mode = DSS(DS_road_mode), keep_page = DSS(DS_shown_page);
    u8 keep_racing = DSB(DS_racing);
    u16 keep_fallen = DSW(DS_fallen);
    enh_init(true, 1, 1, 100, true);
    ega_init();
    DSS(DS_road_mode) = 1;
    DSB(DS_racing) = 1;
    DSS(DS_shown_page) = 0;
    DSW(DS_fallen) = 0;
    enh_race_begin();
    enh_frame_built();
    enh_frame_presented();
    CHECK(enh_compose(small, rgb));
    DSW(DS_fallen) = 1;
    enh_frame_built();
    enh_frame_presented();
    CHECK(!enh_compose(small, rgb));
    enh_race_end();
    DSW(DS_fallen) = keep_fallen;
    DSS(DS_road_mode) = keep_mode; DSS(DS_shown_page) = keep_page; DSB(DS_racing) = keep_racing;
}

/* As Street Rod 1: out-of-range values are clamped; res scale x aa stays within 16 samples. */
static void sr2_enh_options(void)
{
    enh_init(true, 9, 0, 150, true);
    CHECK(enh_opt.res_scale == 8 && enh_opt.aa == 1 && enh_opt.motion_delay == 100);
    enh_init(true, 8, 4, -5, false);
    CHECK(enh_opt.res_scale == 8 && enh_opt.aa == 2 && enh_opt.motion_delay == 0 && !enh_opt.town);
    enh_init(false, 1, 2, 100, true);
    CHECK(!enh_enabled() && enh_res_scale() == 1);
}

/* Rows of a road: colour 8 between left and right (inclusive), 2 elsewhere. */
static void road_row(u8 *row, int left, int right)
{
    for (int x = 0; x < 320; x++) row[x] = x >= left && x <= right ? 8 : 2;
}

static void sr2_enh_compare_rows(void)
{
    static u8 orig[10 * 320], enh[10 * 320], cov[10 * 320];
    const u8 road[] = { 8 };
    EnhCompare c;
    for (int y = 0; y < 10; y++) { road_row(orig + y * 320, 100, 200); road_row(enh + y * 320, 100, 200); }
    memset(cov, 1, sizeof cov);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.rows == 10 && c.edge_rows == 10 && c.edge_err_max == 0 && c.mismatched == 0 && c.pixels == 3200);

    road_row(enh + 3 * 320, 101, 201);              /* one row 1 px off */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1 && c.mismatched == 2);

    road_row(enh + 5 * 320, 97, 200);               /* another 3 px off on the left */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 3 && c.mismatched == 5);

    memset(cov + 5 * 320, 0, 320);                  /* that row not covered: not counted */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1 && c.mismatched == 2 && c.pixels == 2880);

    memset(cov + 6 * 320 + 95, 0, 5);               /* the left edge of row 6 runs under the foreground */
    road_row(enh + 6 * 320, 98, 200);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1);                     /* row 6's left edge is not measured; its right edge is */
    road_row(enh + 6 * 320, 100, 200);
    memset(cov + 6 * 320 + 95, 1, 5);
    memset(cov + 6 * 320 + 101, 0, 8);              /* hidden between the two edges: the enhanced edge is not seen */
    road_row(enh + 6 * 320, 110, 200);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1);
    road_row(enh + 6 * 320, 100, 200);
    memset(cov + 6 * 320 + 101, 1, 8);

    memset(orig + 7 * 320, 2, 320);                 /* no road in the original's row: no edge there */
    memset(enh + 7 * 320, 2, 320);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_rows == 8 && c.edge_err_max == 1);
}

static void sr2_enh_compare_log(void)
{
    char path[1024];
    SDL_snprintf(path, sizeof path, "%stest_sr2_enh_compare.log", SDL_GetBasePath());
    SDL_setenv_unsafe("SR_ENH_COMPARE", path, 1);
    CHECK(enh_compare_open());
    EnhCompare a = { 84, 80, 1, 26880, 27, 0 }, b = { 84, 82, 0, 26880, 54, 0 };
    enh_compare_frame(1, &a);
    enh_compare_frame(1, &b);
    enh_compare_close();
    SDL_unsetenv_unsafe("SR_ENH_COMPARE");
    size_t len = 0;
    char *text = SDL_LoadFile(path, &len);
    CHECK(text != NULL);
    if (!text) return;
    printf("%s", text);
    CHECK(strstr(text, "frame 1 mode 1 rows 84 edge_rows 80 edge_err_max 1 mismatch 1 mirror_cov 0\n") != NULL);
    CHECK(strstr(text, "frame 2 mode 1 rows 84 edge_rows 82 edge_err_max 0 mismatch 2 mirror_cov 0\n") != NULL);
    CHECK(strstr(text, "frames 2 worst_edge 1 mismatch_p50 1 mismatch_max 2 mirror_cov_max 0 shown 0\n") != NULL);
    SDL_free(text);
    SDL_RemovePath(path);
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <SR2 game dir>\n", argv[0]); return 2; }
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    char exe[1024], err[256];
    SDL_snprintf(exe, sizeof exe, "%s/%s", argv[1], SR_EXE_NAME);
    if (!mem_load_exe(exe, err, sizeof err)) { printf("FAIL %s\n", err); return 1; }
    host_configure(&(HostConfig){ "test", 49716, true });
    if (!host_init(argv[1], 1, false)) { printf("FAIL host_init\n"); return 1; }
    RUN(sr2_enh_scale);
    RUN(sr2_enh_layers);
    RUN(sr2_enh_options);
    RUN(sr2_enh_object_tags);
    RUN(sr2_enh_fg_clears_tags);
    RUN(sr2_enh_obj_geo_continuous);
    RUN(sr2_enh_opp_depth);
    RUN(sr2_enh_series_snap);
    RUN(sr2_enh_heading_wrap);
    RUN(sr2_enh_obj_geo_exact_slope);
    RUN(sr2_enh_opp_drawn_d);
    RUN(sr2_enh_fallen_not_composed);
    RUN(sr2_enh_compare_rows);
    RUN(sr2_enh_compare_log);
    host_shutdown();
    return failures ? 1 : 0;
}
