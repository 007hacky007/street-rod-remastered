/* test_sr2_enh.c -- Street Rod 2's enhanced race view (src2/enhanced, src2/platform ENH parts): the output
 * scale, the coverage layers, the options.
 *   test_sr2_enh <SR2 game dir>
 * Headless with the SDL dummy drivers. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enhanced/enhanced.h"
#include "enhanced/enh_compare.h"
#include "enhanced/enh_aq2.h"
#include "enhanced/enh_internal.h"
#include "enhanced/enh_geo2.h"
#include "enhanced/enh_poly2.h"
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

/* Write mode 0 into all planes through the bit mask: the pixels of byte o in `mask` take colour 15. */
static void wr_pixels(u16 o, u8 mask)
{
    vga_out(0x3C4, 2); vga_out(0x3C5, 0x0F);
    vga_out(0x3CE, 8); vga_out(0x3CF, mask);
    (void)vga_rd(o);                                /* the latches keep the other pixels */
    vga_wr(o, 0xFF);
}

/* Pixels x..x+n-1 of row y on the page at `page` take colour 15 (written in whatever layer is current). */
static void wr_span(u16 page, int x, int y, int n)
{
    for (int i = x; i < x + n; i++) wr_pixels((u16)(page + y * EGA_BYTES_PER_LINE + (i >> 3)), (u8)(0x80 >> (i & 7)));
}

/* The same in colour col: the planes of col set, the others cleared. */
static void wr_span_col(u16 page, int x, int y, int n, u8 col)
{
    for (int i = x; i < x + n; i++) {
        u16 o = (u16)(page + y * EGA_BYTES_PER_LINE + (i >> 3));
        u8 m = (u8)(0x80 >> (i & 7));
        vga_out(0x3CE, 8); vga_out(0x3CF, m);
        vga_out(0x3C4, 2); vga_out(0x3C5, col & 15);
        (void)vga_rd(o);
        vga_wr(o, 0xFF);
        vga_out(0x3C4, 2); vga_out(0x3C5, ~col & 15);
        (void)vga_rd(o);
        vga_wr(o, 0x00);
    }
    vga_out(0x3C4, 2); vga_out(0x3C5, 0x0F);
}

static u8 tag_at(u16 page, int x, int y) { return ega_obj()[(u32)(u16)(page + y * EGA_BYTES_PER_LINE + (x >> 3)) * 8 + (x & 7)]; }
static bool fg_at(u16 page, int x, int y) { return ega_fg()[(u16)(page + y * EGA_BYTES_PER_LINE + (x >> 3))] >> (7 - (x & 7)) & 1; }

/* A rectangle kept the game's (ega_mark_fg_rect) is foreground and none of its pixels is an object's any more. */
static void sr2_enh_mark_fg_clears_tags(void)
{
    enh_init(true, 1, 1, 100, true);
    ega_init();
    vga_mode_0d();
    ega_clear_obj(0, 0x2000);
    ega_clear_fg(0, 0x2000);
    ega_set_obj(3);
    ega_set_layer(EGA_LAYER_OBJ);
    wr_span(0, 100, 50, 40);
    ega_set_layer(EGA_LAYER_OFF);
    ega_mark_fg_rect(0, 105, 50, 112, 50);
    int bad = 0;
    for (int x = 100; x < 140; x++) {
        bool in = x >= 105 && x <= 112;
        if (in ? !(fg_at(0, x, 50) && tag_at(0, x, 50) == 0) : !(tag_at(0, x, 50) == 3 && !fg_at(0, x, 50))) bad++;
    }
    CHECK(bad == 0);
}

typedef struct { s16 mode, seg, sub, opp_seg, opp_sprite, opp_d, shown, back, heading; u8 racing; u16 fallen; } Keep;
static Keep keep_ds(void)
{
    return (Keep){ DSS(DS_road_mode), DSS(DS_seg), DSS(DS_substep), DSS(DS_opp_seg), DSS(DS_opp_sprite),
                   DSS(DS_opp_last_d), DSS(DS_shown_page), DSS(DS_back_page), DSS(DS_heading), DSB(DS_racing),
                   DSW(DS_fallen) };
}
static void restore_ds(const Keep *k)
{
    DSS(DS_road_mode) = k->mode; DSS(DS_seg) = k->seg; DSS(DS_substep) = k->sub; DSS(DS_opp_seg) = k->opp_seg;
    DSS(DS_opp_sprite) = k->opp_sprite; DSS(DS_opp_last_d) = k->opp_d; DSS(DS_shown_page) = k->shown;
    DSS(DS_back_page) = k->back; DSS(DS_heading) = k->heading; DSB(DS_racing) = k->racing; DSW(DS_fallen) = k->fallen;
}

/* A street drag at segment 100, its frames drawn into page 2 (plane offset 4000h) and shown from page 0. */
static void race_setup(void)
{
    enh_init(true, 1, 1, 100, true);
    ega_init();
    vga_mode_0d();
    DSS(DS_road_mode) = 1;
    DSB(DS_racing) = 1;
    DSS(DS_seg) = 100;
    DSS(DS_substep) = 0;
    DSS(DS_heading) = 0;
    DSS(DS_shown_page) = 0;
    DSS(DS_back_page) = 2;
    DSW(DS_fallen) = 0;
    enh_race_begin();
}

/* enh_draw_objects(0) returns to the layer the objects were drawn in: the foreground after enh_draw_fg(true)
 * (the mirror, when it stays the game's), the background after enh_draw_begin. */
static void sr2_enh_base_layer(void)
{
    Keep k = keep_ds();
    race_setup();
    enh_draw_fg(true);
    enh_draw_objects(4);
    enh_draw_objects(0);
    wr_span(0x4000, 60, 40, 1);
    CHECK(fg_at(0x4000, 60, 40));
    enh_draw_begin();
    ega_mark_fg_rect(0x4000, 60, 40, 60, 40);
    enh_draw_objects(4);
    enh_draw_objects(0);
    wr_pixels((u16)(0x4000 + 40 * EGA_BYTES_PER_LINE + (60 >> 3)), 0x80 >> (60 & 7));
    CHECK(!fg_at(0x4000, 60, 40));
    enh_race_end();
    restore_ds(&k);
}

/* Which object pixels a frame's capture keeps: the front view's outside the mirror (its body and mount), the
 * mirror's inside its glass; the opponent by the picture it was drawn with (9 and up: the mirror's). */
static void sr2_enh_capture_regions(void)
{
    CHECK(!enh_in_mirror_rect(0xEF, 0x14));
    CHECK(enh_in_mirror_rect(0xF0, 0x14));
    CHECK(enh_in_mirror_rect(0x13F, 0x2C));
    CHECK(!enh_in_mirror_rect(0xF0, 0x2D));
    CHECK(!enh_in_mirror_rect(0xF0, 0x13));
    CHECK(enh_in_mirror_rect(0x110, 0x12));
    CHECK(!enh_in_mirror_rect(0x120, 0x12));
    CHECK(!enh_in_glass(0xF1, 0x16));
    CHECK(enh_in_glass(0xF2, 0x16));
    CHECK(enh_in_glass(0x13D, 0x2A));
    CHECK(!enh_in_glass(0x13E, 0x2A));
    CHECK(!enh_in_glass(0xF2, 0x2B));
    CHECK(!enh_tag_kept(3, 0, 0x100, 0x20));
    CHECK(enh_tag_kept(3, 0, 0x100, 0x30));
    CHECK(enh_tag_kept(ENH_OBJ_OPP, 10, 0x100, 0x20));
    CHECK(!enh_tag_kept(ENH_OBJ_OPP, 10, 0x80, 0x40));
    CHECK(!enh_tag_kept(ENH_OBJ_OPP, 5, 0x100, 0x20));
    CHECK(!enh_tag_kept(ENH_OBJ_MIR(2), 0, 0xF1, 0x20));
}

/* One frame of the street drag with three objects tagged in the shown page: row 3, row 5 and the opponent (its
 * segment 105, row 5's), composed. */
static void cut_order_frame(void)
{
    static const u32 rgb[16];
    race_setup();
    DSS(DS_opp_seg) = 105;
    DSS(DS_opp_sprite) = 5;
    DSS(DS_opp_last_d) = 320;
    ega_clear_obj(0, 0x2000);
    ega_set_layer(EGA_LAYER_OBJ);
    ega_set_obj(3); wr_span(0, 100, 70, 4);
    ega_set_obj(5); wr_span(0, 120, 60, 4);
    ega_set_obj(ENH_OBJ_OPP); wr_span(0, 140, 60, 4);
    ega_set_layer(EGA_LAYER_OFF);
    enh_frame_built();
    enh_frame_presented();
    CHECK(enh_compose(small, rgb));
}

/* The cuts are drawn at their own row, far to near, the opponent after its row's objects (road_draw_front). */
static void sr2_enh_cut_order(void)
{
    Keep k = keep_ds();
    cut_order_frame();
    printf("  log n %d: %d %d %d\n", enh_cut_log_n, enh_cut_log[0], enh_cut_log[1], enh_cut_log[2]);
    CHECK(enh_cut_log_n == 3 && enh_cut_log[0] == 5 && enh_cut_log[1] == ENH_OBJ_OPP && enh_cut_log[2] == 3);
    enh_race_end();
    restore_ds(&k);
}

/* A cut is partly hidden when its rectangle holds the foreground or pixels of a cut drawn after it: a nearer
 * row's, or the opponent's over the objects of its own row. Such cuts are not judged by the re-projection
 * check. */
static void sr2_enh_cut_hidden(void)
{
    static const u32 rgb[16];
    Keep k = keep_ds();
    race_setup();
    DSS(DS_opp_seg) = 105;
    DSS(DS_opp_sprite) = 5;
    DSS(DS_opp_last_d) = 320;
    ega_clear_obj(0, 0x2000);
    ega_clear_fg(0, 0x2000);
    ega_set_layer(EGA_LAYER_OBJ);
    ega_set_obj(5); wr_span(0, 130, 60, 20);        /* row 5: 130..149 */
    ega_set_obj(ENH_OBJ_OPP);                       /* the opponent over its middle: 136..143, rows 58..60 */
    for (int y = 58; y <= 60; y++) wr_span_col(0, 136, y, 8, 4);
    ega_set_obj(3); wr_span_col(0, 140, 59, 4, 2);  /* row 3 over the opponent's right half */
    ega_set_obj(8); wr_span(0, 100, 50, 4);         /* row 8: alone */
    ega_set_obj(9); wr_span(0, 60, 50, 4);          /* row 9: the foreground over its rectangle */
    wr_span(0, 70, 50, 2);
    ega_set_layer(EGA_LAYER_FG);
    wr_span(0, 65, 50, 1);
    ega_set_layer(EGA_LAYER_OFF);
    enh_frame_built();
    enh_frame_presented();
    CHECK(enh_compose(small, rgb));
    printf("  hidden: row5 %d opp %d row3 %d row8 %d row9 %d\n", enh_cut_partly_hidden(5),
           enh_cut_partly_hidden(ENH_OBJ_OPP), enh_cut_partly_hidden(3), enh_cut_partly_hidden(8),
           enh_cut_partly_hidden(9));
    CHECK(enh_cut_partly_hidden(5) && enh_cut_partly_hidden(ENH_OBJ_OPP));
    CHECK(!enh_cut_partly_hidden(3) && !enh_cut_partly_hidden(8));
    CHECK(enh_cut_partly_hidden(9));
    enh_race_end();
    restore_ds(&k);
}

/* The background drawn over an object later in the frame (Mulholland's rails and hillsides of a nearer row, the
 * game's own drawing) covers part of it: the cut is partly hidden for the re-projection check. The next frame
 * starts afresh. */
static void sr2_enh_cut_overdrawn(void)
{
    static const u32 rgb[16];
    Keep k = keep_ds();
    race_setup();
    DSS(DS_back_page) = 1;
    DSS(DS_shown_page) = 1;
    ega_set_start(0x2000);
    for (int f = 0; f < 2; f++) {
        enh_draw_begin();
        ega_set_obj(5);
        ega_set_layer(EGA_LAYER_OBJ);
        wr_span(0x2000, 130, 60, 20);               /* row 5: 130..149 */
        ega_set_obj(8);
        wr_span(0x2000, 100, 50, 4);                /* row 8: alone */
        enh_draw_objects(0);
        if (f == 0) wr_span_col(0x2000, 135, 60, 3, 2);   /* the background over row 5's middle */
        ega_set_layer(EGA_LAYER_OFF);
        enh_frame_built();
        enh_frame_presented();
        CHECK(enh_compose(small, rgb));
        printf("  frame %d: row5 %d row8 %d\n", f, enh_cut_partly_hidden(5), enh_cut_partly_hidden(8));
        CHECK(enh_cut_partly_hidden(5) == (f == 0));
        CHECK(!enh_cut_partly_hidden(8));
    }
    ega_set_start(0);
    enh_race_end();
    restore_ds(&k);
}

/* A new race carries nothing of the last one: no cuts, the base layer the background. */
static void sr2_enh_race_begin_resets(void)
{
    static const u32 rgb[16];
    Keep k = keep_ds();
    cut_order_frame();
    enh_draw_fg(true);                              /* the last race left its layer at the foreground */
    enh_race_begin();
    enh_draw_objects(0);
    ega_mark_fg_rect(0x4000, 60, 40, 60, 40);
    wr_pixels((u16)(0x4000 + 40 * EGA_BYTES_PER_LINE + (60 >> 3)), 0x80 >> (60 & 7));
    CHECK(!fg_at(0x4000, 60, 40));
    ega_clear_obj(0, 0x2000);                       /* the next frame draws no objects */
    ega_set_layer(EGA_LAYER_OFF);
    enh_frame_built();
    enh_frame_presented();
    CHECK(enh_compose(small, rgb));
    CHECK(enh_cut_log_n == 0);
    enh_race_end();
    restore_ds(&k);
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
    EnhObjGeo ga = enh_obj_geo(&a, ENH_CUT_ROW, 75, 0), gb = enh_obj_geo(&b, ENH_CUT_ROW, 75, 0);
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
    EnhObjGeo g = enh_obj_geo(&v, ENH_CUT_OPP, 100, 2);
    printf("  rear, d -40: ok %d z %.2f\n", g.ok, g.z);
    CHECK(g.ok && SDL_fabs(g.z - 7.0) < 1e-6);
    v.opp_pos = 100.0 + 8.0 / 64;                  /* d = 8, the side view */
    v.opp_d = 8;
    g = enh_obj_geo(&v, ENH_CUT_OPP, 100, 0);
    CHECK(g.ok && SDL_fabs(g.z - 21.0) < 1e-6);
    v.opp_pos = 103.0;                             /* d = 192, ahead */
    v.opp_d = 192;
    g = enh_obj_geo(&v, ENH_CUT_OPP, 103, 5);
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
    EnhObjGeo g = enh_obj_geo(&v, ENH_CUT_ROW, 105, 0);
    double want = road_proj_y(37, 2000, (s16)(200 - 210)) + 0.5;     /* 37 = 8 * 5 + 4 - 7 */
    printf("  exact yg %.2f, the game's %.2f\n", g.yg, want);
    CHECK(g.ok && g.yg == want);
    EnhView a = view_at(100.99), b = view_at(101.01);
    a.mode = b.mode = 1;
    EnhObjGeo ga = enh_obj_geo(&a, ENH_CUT_ROW, 105, 0), gb = enh_obj_geo(&b, ENH_CUT_ROW, 105, 0);
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
    EnhObjGeo g = enh_obj_geo(&v, ENH_CUT_OPP, 104, 5);
    CHECK(g.ok && g.z == 41);
    v.opp_d = -20;
    g = enh_obj_geo(&v, ENH_CUT_OPP, 104, 2);
    CHECK(g.ok && g.z == 6);
    v.opp_d = -40;
    g = enh_obj_geo(&v, ENH_CUT_OPP, 104, 2);
    CHECK(g.ok && g.z == 7);
    EnhView a = view_at(100.0), b = view_at(100.125);
    a.mode = b.mode = 1;
    a.opp_d = 300;
    b.opp_d = 292;                                  /* the car 1/8 segment on */
    EnhObjGeo ga = enh_obj_geo(&a, ENH_CUT_OPP, 105, 5), gb = enh_obj_geo(&b, ENH_CUT_OPP, 105, 5);
    printf("  smooth z %.2f, %.2f\n", ga.z, gb.z);
    CHECK(ga.ok && gb.ok && ga.z == 41.5 && gb.z == 40.5);
}

/* The opponent's front pictures are blitted at whole bytes (clip_rect: x >> 3): its anchor is where the picture
 * landed, not the projected sx. */
static void sr2_enh_opp_drawn_sx(void)
{
    CHECK(enh_opp_drawn_sx(163, 40) == 156);        /* dx 143 drawn at 136 */
    CHECK(enh_opp_drawn_sx(164, 40) == 164);        /* dx 144: on a byte */
    CHECK(enh_opp_drawn_sx(-2, 10) == -3);          /* dx -7 drawn at -8 */
    CHECK(enh_opp_drawn_sx(-3, 10) == -3);
}

/* The exact projections are the game's road_proj_x and road_proj_y, s16 arithmetic and all. */
static void sr2_enh_proj_exact(void)
{
    s16 keep_cx = DSS(DS_cam_x), keep_cy = DSS(DS_cam_y), keep_b = DSS(DS_view_bounce_y), keep_mode = DSS(DS_road_mode);
    DSS(DS_road_mode) = 1;
    static const s16 xs[] = { -2000, 0, 0x5DC, 0xCE4, 0x19C8 + 400 - 0xA00 }, cxs[] = { 0x500, 0xA00, 0xCE1 };
    static const s16 ys[] = { 2000, 0x76C, 0x79E, 2000 - 4000 }, cys[] = { 0x76C, 0x74A }, bs[] = { 0, 10 };
    int bad = 0, n = 0;
    for (int z = 4; z <= 240; z = z == 116 ? 240 : z + 1) {
        for (int i = 0; i < 5; i++)
            for (int dx = -0x60; dx <= 0x60; dx += 0x18)
                for (int c = 0; c < 3; c++) {
                    DSS(DS_cam_x) = cxs[c];
                    n++;
                    if (enh_proj_x_i(z, xs[i], dx, cxs[c]) != road_proj_x((s16)z, xs[i], (s16)dx)) bad++;
                }
        for (int i = 0; i < 4; i++)
            for (int dy = -40; dy <= 40; dy += 10)
                for (int c = 0; c < 2; c++)
                    for (int b = 0; b < 2; b++) {
                        DSS(DS_cam_y) = cys[c];
                        DSS(DS_view_bounce_y) = bs[b];
                        n++;
                        if (enh_proj_y_i(z, ys[i], dy, cys[c], bs[b]) != road_proj_y((s16)z, ys[i], (s16)dy)) bad++;
                    }
    }
    printf("  %d of %d differ\n", bad, n);
    CHECK(bad == 0);
    DSS(DS_cam_x) = keep_cx; DSS(DS_cam_y) = keep_cy; DSS(DS_view_bounce_y) = keep_b; DSS(DS_road_mode) = keep_mode;
}

/* Row 0's outer edges are the original's quirk (they move with cam_x), the others the world's outer edges. */
static void sr2_enh_rows_front_row0(void)
{
    static const u16 arrs[] = { TRK_C, TRK_L, TRK_R, TRK_YAW, TRK_HGT, TRK_BL, TRK_BR };
    u16 keep[7][30];
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 30; i++) { keep[a][i] = TRKW(arrs[a], 40 + i); TRKW(arrs[a], 40 + i) = 0; }
    s16 keep_cx = DSS(DS_cam_x), keep_cy = DSS(DS_cam_y), keep_b = DSS(DS_view_bounce_y), keep_mode = DSS(DS_road_mode);
    DSS(DS_cam_x) = 0xCE1;
    DSS(DS_cam_y) = 0x76C;
    DSS(DS_view_bounce_y) = 0;
    DSS(DS_road_mode) = 0;
    EnhView v;
    memset(&v, 0, sizeof v);
    v.pos = 50.0;
    v.lat = 0xCE1;
    v.cam_y = 0x76C;
    v.exact = true;
    EnhRowTab t;
    enh_rows_exact_front(&v, &t);
    printf("  xol0 %d xor0 %d xor1 %d y14 %d\n", t.xol[0], t.xor_[0], t.xor_[1], t.y[14]);
    CHECK(t.xor_[0] == road_proj_x(4, (s16)((400 - 0xCE1) + 0x19C8), 0));
    CHECK(t.xol[0] == road_proj_x(4, (s16)-(400 - 0xCE1), 0));
    CHECK(t.xor_[1] == road_proj_x(12, 400 + 0xCE4, 0));
    CHECK(t.y[14] == road_proj_y(116, 2000, 0));
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 30; i++) TRKW(arrs[a], 40 + i) = keep[a][i];
    DSS(DS_cam_x) = keep_cx; DSS(DS_cam_y) = keep_cy; DSS(DS_view_bounce_y) = keep_b; DSS(DS_road_mode) = keep_mode;
}

/* A town facade in exact mode is the game's quad_fill: clip_polygon's integer clip, then poly_fill with its edge
 * pixels. The band of rows 4 and 3 at frame 148 of the location-4 return route (seed 2): the right facade from
 * (284, 72) to (332, 76), its top at 60. Clipped at x 13Fh its bottom edge ends at (319, 74), and the original's
 * row 74 starts at x 311 (its Bresenham run), not where the unclipped edge crosses it. */
static void sr2_enh_town_face_clipped(void)
{
    EnhOptions keep = enh_opt;
    enh_opt.res_scale = enh_opt.aa = 1;
    enh_buf_setup();
    enh_clip_view();
    enh_fill_rows(ENH_VIEW_Y0, ENH_VIEW_Y1, 0);
    enh_town_face_exact(1, 332, 284, 76, 72, 60, 60, 3);
    static const int rows[] = { 60, 72, 73, 74 }, want[] = { 284, 284, 293, 311 };
    int bad = 0;
    for (int i = 0; i < 4; i++) {
        const u8 *r = enh_buf.px + (size_t)(rows[i] - ENH_VIEW_Y0) * enh_buf.w;
        int l = -1, rr = -1;
        for (int x = 0; x < 320; x++) if (r[x] == 3) { if (l < 0) l = x; rr = x; }
        printf("  row %d: %d..%d (the game's %d..319)\n", rows[i], l, rr, want[i]);
        if (l < 0 || abs(l - want[i]) > 1 || rr != 319) bad++;
    }
    const u8 *r75 = enh_buf.px + (size_t)(75 - ENH_VIEW_Y0) * enh_buf.w;
    for (int x = 0; x < 320; x++) if (r75[x] == 3) bad++;  /* nothing below the clipped corner */
    CHECK(bad == 0);
    enh_opt = keep;
    enh_buf_setup();
}

/* The town's storey end face (town_walls_front: drv_hspan from 2a - b to a - 1 on the left, b + 1 to 2b - a on the
 * right, a the near row's outer x, b the far row's) where row 0's outer edge quirk puts the near edge beyond the far
 * one. ega_hline rejects a span by its unsorted ends (x2 < 0 or x1 > 13Fh) before it swaps them, so the game draws
 * no end face there; the enhanced view must not fill it either (it flashed the town's sky in colour 2). A storey
 * starts at segment s (F & 4, not at s - 1): the end face is the only difference between F[s - 1] 0 and 4. Row 0's
 * band is s 51; the control, an end face drawn, is row 1's band (s 52). */
static int town_end_face_px(const EnhView *v, int side, int s)
{
    u16 arr = side ? TRK_R : TRK_L;
    TRKW(arr, s) = 4;
    enh_draw_road(v);
    static u8 with[320 * 200];
    size_t n = (size_t)enh_buf.w * enh_buf.h;
    memcpy(with, enh_buf.px, n);
    TRKW(arr, s - 1) = 4;                               /* the storey started a segment before: no end face */
    enh_draw_road(v);
    TRKW(arr, s - 1) = 0;
    TRKW(arr, s) = 0;
    int d = 0;                                          /* the end face's colour-2 pixels */
    for (size_t i = 0; i < n; i++) d += with[i] == 2 && enh_buf.px[i] != 2;
    return d;
}

static void sr2_enh_town_end_face_rejected(void)
{
    static const u16 arrs[] = { TRK_C, TRK_L, TRK_R, TRK_YAW, TRK_HGT, TRK_BL, TRK_BR };
    u16 keep[7][30];
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 30; i++) { keep[a][i] = TRKW(arrs[a], 40 + i); TRKW(arrs[a], 40 + i) = 0; }
    s16 keep_cx = DSS(DS_cam_x), keep_cy = DSS(DS_cam_y), keep_b = DSS(DS_view_bounce_y), keep_mode = DSS(DS_road_mode);
    DSS(DS_cam_y) = 0x76C;
    DSS(DS_view_bounce_y) = 0;
    DSS(DS_road_mode) = 0;
    EnhOptions keep_opt = enh_opt;
    enh_opt.res_scale = enh_opt.aa = 1;
    int bad = 0;
    for (int exact = 1; exact >= 0; exact--)
        for (int side = 0; side < 2; side++) {
            const char *name = exact ? "exact" : "smooth";
            int found = 0, ctrl = 0;
            for (int lat = 0; lat <= 0x1A00 && !(found && ctrl); lat += 8) {
                EnhView v;
                memset(&v, 0, sizeof v);
                v.pos = 50.0;
                v.lat = lat;
                v.cam_y = 0x76C;
                v.town = true;
                v.exact = exact;
                DSS(DS_cam_x) = (s16)lat;
                EnhRowTab t;
                enh_rows_exact_front(&v, &t);
                for (int k = 0; k < 2; k++) {
                    int a = side ? t.xor_[k] : t.xol[k], b = side ? t.xor_[k + 1] : t.xol[k + 1];
                    int x1 = side ? a + 1 : 2 * a - b, x2 = side ? 2 * a - b : a - 1;   /* the game's ends, unsorted */
                    int lo = x1 < x2 ? x1 : x2, hi = x1 < x2 ? x2 : x1;
                    bool reversed = x1 > x2, rejected = x2 < 0 || x1 > 0x13F, seen = hi >= 2 && lo <= 0x13D;
                    if (k == 0 && !found && reversed && rejected && seen) {
                        int d = town_end_face_px(&v, side, 51);
                        printf("  %s side %d lat %d: row 0 %d row 1 %d, span %d..%d rejected, %d px drawn (want 0)\n",
                               name, side, lat, a, b, x1, x2, d);
                        if (d) bad++;
                        found = 1;
                    }
                    if (k == 1 && !ctrl && !reversed && x1 >= 0 && x2 <= 0x13F && x2 - x1 > 4) {
                        int d = town_end_face_px(&v, side, 52);
                        printf("  %s side %d lat %d: row 1's span %d..%d drawn, %d px\n", name, side, lat, x1, x2, d);
                        if (!d) bad++;
                        ctrl = 1;
                    }
                }
            }
            if (!found || !ctrl) { printf("  %s side %d: no lat found (%d %d)\n", name, side, found, ctrl); bad++; }
        }
    CHECK(bad == 0);
    enh_opt = keep_opt;
    enh_buf_setup();
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 30; i++) TRKW(arrs[a], 40 + i) = keep[a][i];
    DSS(DS_cam_x) = keep_cx; DSS(DS_cam_y) = keep_cy; DSS(DS_view_bounce_y) = keep_b; DSS(DS_road_mode) = keep_mode;
}

/* road_draw_sky's top_band_row: from row 14 to row 1, the row whose y is the first strictly above all after it
 * (and above the horizon strip's bottom). */
static void sr2_enh_top_band_row(void)
{
    double y[15] = { 160, 100, 80, 70, 65, 62, 61, 60.5, 60.4, 60.3, 60.2, 60.1, 60, 59, 58 }, g = 0;
    CHECK(enh_top_band_row(y, 69, &g) == 14 && g == 58);
    double c[15] = { 160, 100, 80, 70, 65, 62, 61, 63, 63, 63, 63, 63, 63, 63, 63 };
    CHECK(enh_top_band_row(c, 69, &g) == 6 && g == 61);
    y[13] = y[14] = 50;
    CHECK(enh_top_band_row(y, 69, &g) == 14 && g == 50);
}

/* The centre dash's width is the game's per-row table, interpolated between rows in a smooth view. */
static void sr2_enh_dash_width(void)
{
    int bad = 0;
    for (int k = 1; k <= 14; k++)
        if (enh_dash_width(k, 3, true) != DSS((u16)(DS_dash_width + 2 * k))) bad++;
    CHECK(bad == 0);
    double t5 = DSS(DS_dash_width + 10), t4 = DSS(DS_dash_width + 8);
    CHECK(enh_dash_width(5, 4, false) == (t5 + t4) / 2);
}

/* An exact view's centre dash is road_draw_band_front's: each line's x from the edges stepped by slope16, not a
 * straight line between the rows (a 1-px step off it moves the dash next to the screen's edge). */
static void sr2_enh_dash_exact(void)
{
    EnhOptions keep = enh_opt;
    enh_opt.res_scale = enh_opt.aa = 1;
    enh_buf_setup();
    enh_clip_view();
    enh_fill_rows(ENH_VIEW_Y0, ENH_VIEW_Y1, 0);
    const int xlq = 150, xrq = 330, yq = 0x5F, xlp = 250, xrp = 314, yp = 0x50, w = 5;
    enh_dash_exact(xlq, xrq, yq, xlp, xrp, yp, w, 4);
    s16 sl = cdiv16(shl16((s16)(xlp - xlq), 4), (s16)(yp - yq)), sr = cdiv16(shl16((s16)(xrp - xrq), 4), (s16)(yp - yq));
    s16 axl = shl16(xlp, 4), axr = shl16(xrp, 4), xl = xlp, xr = xrp;
    int bad = 0;
    for (int y = yp; y < yq; y++) {
        int dx = ((xl + xr) >> 1) - (w >> 1), l = -1, r = -1;
        const u8 *row = enh_buf.px + (size_t)(y - ENH_VIEW_Y0) * enh_buf.w;
        for (int x = 0; x < 320; x++) if (row[x] == 4) { if (l < 0) l = x; r = x; }
        if (l != dx || r != dx + w) { printf("  line %02X: %d..%d, the game's %d..%d\n", y, l, r, dx, dx + w); bad++; }
        axl = add16(axl, sl); axr = add16(axr, sr);
        xl = (s16)(axl >> 4); xr = (s16)(axr >> 4);
    }
    CHECK(bad == 0);
    enh_opt = keep;
    enh_buf_setup();
}

/* sky_scroll's shift: where the sheet's column 0 lands. */
static void sr2_enh_sky_shift(void)
{
    CHECK(enh_sky_shift(5, 1) == 8);
    CHECK(enh_sky_shift(-5, 1) == 312);
    CHECK(enh_sky_shift(0, 1) == 320);
    CHECK(enh_sky_shift(165, 1) == 8);
    CHECK(enh_sky_shift(200, 2) == 160);
    CHECK(enh_sky_shift(-200, 2) == 160);
}

/* The drag strip (modes 2 and 4) reads the strip's own flags at the strip index and the rest at index + C0h, as
 * road_set_seg_ptrs points them; the other modes read the race road's arrays. */
static void sr2_enh_strip_tracks(void)
{
    CHECK(enh_track(2, TRK_C, 5) == TRKW(TRK_C2, 5));
    CHECK(enh_track(4, TRK_L, 5) == TRKW(TRK_L2, 5));
    CHECK(enh_track(4, TRK_R, 5) == TRKW(TRK_R2, 5));
    CHECK(enh_track(4, TRK_HGT, 5) == TRKW(TRK_HGT, 5 + 0xC0));
    CHECK(enh_track(1, TRK_C, 5) == TRKW(TRK_C, 5));
}

/* road_draw_barrier_walls: abs(n - row - seg + phase) % 10 below 5 is pink (0Bh), else white (4). */
static void sr2_enh_wall_colour(void)
{
    static const int sum[] = { 18, 17, 22, 27, 32 }, want[] = { 0x0B, 4, 0x0B, 4, 0x0B };
    for (int i = 0; i < 5; i++) {
        CHECK(enh_wall_colour(8, 14, 3, sum[i] - 3) == want[i]);
        CHECK(enh_wall_colour(8, 14, 9, sum[i] - 9) == want[i]);
    }
    CHECK(enh_wall_colour(8, 7, 3, 20) == 4);
}

/* enh_clip_poly_i is clip_polygon point for point, on the view's clip and on the mirror's. */
static void sr2_enh_clip_poly_i(void)
{
    s16 keep[4] = { DSS(DS_clip_xmin), DSS(DS_clip_xmax), DSS(DS_clip_ymin), DSS(DS_clip_ymax) };
    static const s16 clips[2][4] = { { 0, 0x13F, 0x12, 0x65 }, { 0xF0, 0x13F, 0x16, 0x2A } };
    static const s16 polys[2][4][8] = {
        { { 10, 30, 10, 40, 50, 45, 50, 30 },               /* inside */
          { 300, 40, 300, 60, 343, 70, 341, 29 },           /* crossing the right edge */
          { -23, 90, -20, 121, 31, 110, 30, 80 },           /* crossing the bottom and the left */
          { 400, 10, 420, 10, 420, 0, 400, 0 } },           /* wholly outside */
        { { 0x100, 0x18, 0x100, 0x20, 0x110, 0x22, 0x110, 0x18 },
          { 0x130, 0x18, 0x130, 0x28, 0x153, 0x29, 0x151, 0x17 },
          { 0xE3, 0x20, 0xE1, 0x41, 0x105, 0x30, 0x100, 0x1A },
          { 10, 10, 30, 10, 30, 0, 10, 0 } } };
    int bad = 0;
    for (int c = 0; c < 2; c++) {
        DSS(DS_clip_xmin) = clips[c][0]; DSS(DS_clip_xmax) = clips[c][1];
        DSS(DS_clip_ymin) = clips[c][2]; DSS(DS_clip_ymax) = clips[c][3];
        for (int p = 0; p < 4; p++) {
            s16 want[128], got[128], nw = 0;
            clip_polygon(polys[c][p], 4, want, &nw);
            int ng = enh_clip_poly_i(polys[c][p], 4, got, clips[c][0], clips[c][1], clips[c][2], clips[c][3]);
            bool same = ng == nw && memcmp(want, got, sizeof(s16) * 2 * (size_t)(nw > 0 ? nw : 0)) == 0;
            if (!same) { printf("  clip %d poly %d: %d points, the game's %d\n", c, p, ng, nw); bad++; }
            if (p == 0 && ng != 4) bad++;
            if ((p == 1 || p == 2) && (ng < 4 || memcmp(want, polys[c][p], sizeof(s16) * 8) == 0)) bad++;
            if (p == 3 && ng != 0) bad++;
        }
    }
    CHECK(bad == 0);
    DSS(DS_clip_xmin) = keep[0]; DSS(DS_clip_xmax) = keep[1]; DSS(DS_clip_ymin) = keep[2]; DSS(DS_clip_ymax) = keep[3];
}

/* the samples of colour col in the view (res 1, aa 1), and whether each one lies in the rectangles given */
static int count_col(u8 col, const int (*in)[4], int nin, int *outside)
{
    int n = 0;
    *outside = 0;
    for (int y = ENH_VIEW_Y0; y < ENH_VIEW_Y1; y++)
        for (int x = 0; x < 320; x++) {
            if (enh_buf.px[(size_t)(y - ENH_VIEW_Y0) * enh_buf.w + x] != col) continue;
            n++;
            bool ok = false;
            for (int i = 0; i < nin; i++) ok |= x >= in[i][0] && x <= in[i][1] && y >= in[i][2] && y <= in[i][3];
            if (!ok) (*outside)++;
        }
    return n;
}

/* poly_fill's edge pixels included: each row from the polygon's leftmost to its rightmost point within the row,
 * plus a pixel; a concave (y-monotone) polygon as poly_fill fills it, not its convex hull. */
static void sr2_enh_poly_incl(void)
{
    EnhOptions keep = enh_opt;
    enh_opt.res_scale = enh_opt.aa = 1;
    enh_buf_setup();
    enh_clip_view();
    int out = 0;
    enh_fill_rows(ENH_VIEW_Y0, ENH_VIEW_Y1, 0);
    double ax[4] = { 10, 10, 11, 11 }, ay[4] = { 20.5, 30.5, 30.5, 20.5 };
    enh_fill_poly_incl(ax, ay, 4, 7);
    static const int ra[1][4] = { { 10, 11, 20, 30 } };
    int n = count_col(7, ra, 1, &out);
    printf("  quad %d samples (%d outside)\n", n, out);
    CHECK(n == 22 && out == 0);
    enh_fill_rows(ENH_VIEW_Y0, ENH_VIEW_Y1, 0);
    double bx[4] = { 40, 60, 60, 40 }, by[4] = { 50.5, 50.5, 50.5, 50.5 };
    enh_fill_poly_incl(bx, by, 4, 7);
    static const int rb[1][4] = { { 40, 60, 50, 50 } };
    n = count_col(7, rb, 1, &out);
    printf("  line %d samples (%d outside)\n", n, out);
    CHECK(n == 21 && out == 0);
    enh_fill_rows(ENH_VIEW_Y0, ENH_VIEW_Y1, 0);
    /* the L-shaped hexagon (10,10),(20,10),(20,15),(15,15),(15,20),(10,20), 30 rows lower (the view starts at
     * row 12h) */
    double cx[6] = { 10, 20, 20, 15, 15, 10 }, cy[6] = { 40.5, 40.5, 45.5, 45.5, 50.5, 50.5 };
    enh_fill_poly_incl(cx, cy, 6, 7);
    static const int rc[2][4] = { { 10, 20, 40, 45 }, { 10, 15, 46, 50 } };
    n = count_col(7, rc, 2, &out);
    printf("  hexagon %d samples (%d outside)\n", n, out);
    CHECK(n == 96 && out == 0);                             /* its convex hull would fill more */
    enh_opt = keep;
    enh_buf_setup();
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

/* ---------------------------------------------------------------- Mulholland */

/* Mulholland's rows: the outer edges at w 200 (world 514h and DACh), row 0's dx clamped to +-30h in the front
 * view and beyond +-3Ch to +-46h in the mirror. */
static void sr2_enh_mulholland_rows(void)
{
    static const u16 arrs[] = { TRK_C, TRK_L, TRK_R, TRK_YAW, TRK_HGT, TRK_BL, TRK_BR };
    u16 keep[7][40];
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 40; i++) { keep[a][i] = TRKW(arrs[a], 30 + i); TRKW(arrs[a], 30 + i) = 0; }
    s16 keep_cx = DSS(DS_cam_x), keep_cy = DSS(DS_cam_y), keep_b = DSS(DS_view_bounce_y), keep_mode = DSS(DS_road_mode);
    DSS(DS_cam_x) = 0xA00;
    DSS(DS_cam_y) = 0x76C;
    DSS(DS_view_bounce_y) = 0;
    DSS(DS_road_mode) = 0x10;
    EnhView v;
    memset(&v, 0, sizeof v);
    v.pos = 50.375;                                 /* q 3 */
    v.lat = 0xA00;
    v.cam_y = 0x76C;
    v.mode = 0x10;
    v.exact = true;
    EnhRowTab t;
    enh_rows_exact_front(&v, &t);
    CHECK(t.xol[1] == road_proj_x(12 - 3, 0x514, 0) && t.xor_[1] == road_proj_x(12 - 3, 0xDAC, 0));
    v.heading = 0x50;
    enh_rows_exact_front(&v, &t);
    printf("  front xl0 %d (the game's %d)\n", t.xl[0], road_proj_x(4, 0x5DC, 0x30));
    CHECK(t.xl[0] == road_proj_x(4, 0x5DC, 0x30));
    v.heading = -0x50;
    memset(&t, 0, sizeof t);
    enh_rows_exact_mirror(&v, &t);
    CHECK(t.xl[0] == enh_proj_x_mirror_i(8, 0x5DC, 0x46, 0xA00));
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 40; i++) TRKW(arrs[a], 30 + i) = keep[a][i];
    DSS(DS_cam_x) = keep_cx; DSS(DS_cam_y) = keep_cy; DSS(DS_view_bounce_y) = keep_b; DSS(DS_road_mode) = keep_mode;
}

/* front_hillsides' screen constants of a row, read from DS at k = 15 - row (row 1's run into DS_dash_width). */
static void sr2_enh_hill_consts(void)
{
    s16 w0, h0, w1, h1, e;
    enh_hill_consts(14, &w0, &h0, &w1, &h1, &e);
    CHECK(w0 == DSS(0x7E22) && h0 == DSS(0x7E24) && w1 == DSS(0x7E26) && h1 == DSS(0x7E28) && e == DSS(0x7E3E));
    enh_hill_consts(1, &w0, &h0, &w1, &h1, &e);
    CHECK(w0 == DSS(0x7E1E + 56));
}

/* road_draw_sky_mulholland's dy: at most 8, at least -8 unless the car falls off the edge. */
static void sr2_enh_mulholland_sky_dy(void)
{
    CHECK(enh_mulholland_sky_dy(12, 0) == 8);
    CHECK(enh_mulholland_sky_dy(-12, 0) == -8);
    CHECK(enh_mulholland_sky_dy(-12, 1) == -12);
    CHECK(enh_mulholland_sky_dy(3, 0) == 3);
}

/* guard_rail's post parity: cmod16(phase - row - seg + n, 2) != 0 (odd, or -1 as cmod16 gives it). */
static void sr2_enh_rail_post(void)
{
    CHECK(enh_rail_post(100, 3, 90, 14));
    CHECK(!enh_rail_post(100, 4, 90, 14));
    CHECK(enh_rail_post(5, 3, 10, 7));
}

/* road_draw_front's rail phase at the view's base: the farthest row k (14..1) where a rail starts (R[base + k] &
 * 8000h, not R[base + k - 1]) gives base + k; none in view keeps the fallback. */
static void sr2_enh_rail_phase(void)
{
    u16 keep[40];
    for (int i = 0; i < 40; i++) { keep[i] = TRKW(TRK_R, 30 + i); TRKW(TRK_R, 30 + i) = 0; }
    Cam c;
    memset(&c, 0, sizeof c);
    c.mode = 0x10;
    c.base = 40;
    CHECK(enh_rail_phase(&c, 123) == 123);
    for (int i = 45; i < 48; i++) TRKW(TRK_R, i) = 0x8000;      /* a rail from row 5 */
    for (int i = 52; i < 60; i++) TRKW(TRK_R, i) = 0x8000;      /* and one from row 12 */
    CHECK(enh_rail_phase(&c, 123) == 52);
    for (int i = 30; i < 70; i++) TRKW(TRK_R, i) = 0x8000;      /* rails everywhere: no start in view */
    CHECK(enh_rail_phase(&c, 123) == 123);
    for (int i = 0; i < 40; i++) TRKW(TRK_R, 30 + i) = keep[i];
}

/* A line one original pixel thick through the pixels' centres, both ends included: at res 1 the game's Bresenham
 * line (ega_line: x-major, one pixel per column, a tie stepped towards the far end), whichever end comes first. */
static void sr2_enh_line1(void)
{
    EnhOptions keep = enh_opt;
    enh_opt.res_scale = enh_opt.aa = 1;
    enh_buf_setup();
    enh_clip_view();
    static const int want[11] = { 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25 };
    for (int pass = 0; pass < 2; pass++) {
        enh_fill_rows(ENH_VIEW_Y0, ENH_VIEW_Y1, 0);
        if (pass == 0) enh_line1(10, 20, 20, 25, 7); else enh_line1(20, 25, 10, 20, 7);
        int n = 0, bad = 0;
        for (int y = ENH_VIEW_Y0; y < ENH_VIEW_Y1; y++)
            for (int x = 0; x < 320; x++) {
                if (enh_buf.px[(size_t)(y - ENH_VIEW_Y0) * enh_buf.w + x] != 7) continue;
                n++;
                if (x < 10 || x > 20 || want[x - 10] != y) bad++;
            }
        printf("  line %d samples, %d off the game's\n", n, bad);
        CHECK(n == 11 && bad == 0);
    }
    enh_fill_rows(ENH_VIEW_Y0, ENH_VIEW_Y1, 0);
    enh_line1(30, 40, 30, 40, 7);
    int n = 0;
    for (size_t i = 0; i < (size_t)enh_buf.w * enh_buf.h; i++) n += enh_buf.px[i] == 7;
    CHECK(n == 1 && enh_buf.px[(size_t)(40 - ENH_VIEW_Y0) * enh_buf.w + 30] == 7);
    enh_opt = keep;
    enh_buf_setup();
}

/* ---------------------------------------------------------------- the rear-view mirror */

/* road_project_mirror's depths: row 0 at 8, rows 1..4 at 8k + q, rows 5..7 frozen at 40, 48, 56 (the k == 5
 * `z -= q`). */
static void sr2_enh_mirror_depths(void)
{
    static const int qs[] = { 0, 3, 7 };
    int bad = 0;
    for (int i = 0; i < 3; i++) {
        int q = qs[i];
        const double want[8] = { 8, 8 + q, 16 + q, 24 + q, 32 + q, 40, 48, 56 };
        for (int k = 0; k < 8; k++) if (enh_mirror_z(k, q) != want[k]) bad++;
    }
    CHECK(bad == 0);
}

/* The exact mirror projections are the game's road_proj_x_mirror and road_proj_y_mirror. */
static void sr2_enh_proj_mirror_exact(void)
{
    s16 keep_cx = DSS(DS_cam_x), keep_cy = DSS(DS_cam_y), keep_mode = DSS(DS_road_mode);
    DSS(DS_road_mode) = 1;
    static const s16 xs[] = { -2000, 0, 0x5DC, 0xCE4, 400 + 0xCE4, 0x5DC - 400 }, cxs[] = { 0x500, 0xA00, 0xCE1 };
    static const s16 ys[] = { 2000, 0x76C, 0x79E, 2000 - 4000 }, cys[] = { 0x76C, 0x74A };
    int bad = 0, n = 0;
    for (int z = 2; z <= 56; z++) {
        for (int i = 0; i < 6; i++)
            for (int dx = -0x60; dx <= 0x60; dx += 0x18)
                for (int c = 0; c < 3; c++) {
                    DSS(DS_cam_x) = cxs[c];
                    n++;
                    if (enh_proj_x_mirror_i(z, xs[i], dx, cxs[c]) != road_proj_x_mirror((s16)z, xs[i], (s16)dx)) bad++;
                }
        for (int i = 0; i < 4; i++)
            for (int dy = -40; dy <= 40; dy += 10)
                for (int c = 0; c < 2; c++) {
                    DSS(DS_cam_y) = cys[c];
                    n++;
                    if (enh_proj_y_mirror_i(z, ys[i], dy, cys[c]) != road_proj_y_mirror((s16)z, ys[i], (s16)dy)) bad++;
                }
    }
    printf("  %d of %d differ\n", bad, n);
    CHECK(bad == 0);
    DSS(DS_cam_x) = keep_cx; DSS(DS_cam_y) = keep_cy; DSS(DS_road_mode) = keep_mode;
}

/* road_project_mirror's YK quirk: row 0 writes YK[0], row k >= 1 both YK[2k - 1] and YK[2k]. */
static void sr2_enh_mirror_yk(void)
{
    static const u16 arrs[] = { TRK_C, TRK_L, TRK_R, TRK_YAW, TRK_HGT, TRK_BL, TRK_BR };
    u16 keep[7][30];
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 30; i++) { keep[a][i] = TRKW(arrs[a], 40 + i); TRKW(arrs[a], 40 + i) = 0; }
    EnhView v;
    memset(&v, 0, sizeof v);
    v.pos = 55.375;                                 /* q 3 */
    v.lat = 0xA00;
    v.cam_y = 0x76C;
    v.mode = 1;
    v.exact = true;
    EnhRowTab t;
    memset(&t, 0, sizeof t);
    enh_rows_exact_mirror(&v, &t);
    int bad = t.yk[0] != enh_proj_y_mirror_i(8, 0x79E, 0, 0x76C);
    for (int k = 1; k < 8; k++) {
        int want = enh_proj_y_mirror_i((int)enh_mirror_z(k, 3), 0x79E, 0, 0x76C);
        if (t.yk[2 * k - 1] != want || t.yk[2 * k] != want) bad++;
    }
    printf("  yk %d %d %d .. %d\n", t.yk[0], t.yk[1], t.yk[2], t.yk[14]);
    CHECK(bad == 0);
    CHECK(t.yk[0] == 0x24 + 50 / 8 && t.yk[1] == 0x24 + 50 / 11 && t.yk[14] == 0x24 + 50 / 56);
    for (int a = 0; a < 7; a++)
        for (int i = 0; i < 30; i++) TRKW(arrs[a], 40 + i) = keep[a][i];
}

/* The opponent behind (pictures 9 and up) stands at the mirror's depth -d / 8, at the x it was drawn at. */
static void sr2_enh_mirror_opp_geo(void)
{
    EnhView v = view_at(100.0);
    v.mode = 1;
    v.exact = true;
    v.opp_seg = 98;
    v.opp_d = -100;
    v.opp_sx = 0x120;
    EnhObjGeo g = enh_obj_geo(&v, ENH_CUT_OPP, 98, 10);
    printf("  ok %d z %.2f a %.2f b %.4f\n", g.ok, g.z, g.a, g.b);
    CHECK(g.ok && g.z == 12 && g.a == 0x120 && g.b == 1.0 / 12);
}

/* The mirror is drawn into the background; only its frame and mount stay the game's. */
static void sr2_enh_mirror_hooks(void)
{
    Keep k = keep_ds();
    race_setup();
    enh_draw_begin();
    enh_mirror_begin();
    wr_span(0x4000, 0x100, 0x20, 1);
    CHECK(!fg_at(0x4000, 0x100, 0x20));
    enh_mirror_end();
    CHECK(fg_at(0x4000, 0xF0, 0x20) && fg_at(0x4000, 0x100, 0x14) && fg_at(0x4000, 0x115, 0x12));
    CHECK(fg_at(0x4000, 0x100, 0x2C) && fg_at(0x4000, 0x13F, 0x20));
    CHECK(!fg_at(0x4000, 0x100, 0x20) && tag_at(0x4000, 0x100, 0x20) == 0);
    CHECK(!fg_at(0x4000, 0x105, 0x12));            /* beside the mount: the front view's sky */
    wr_span_col(0x4000, 0x80, 0x40, 1, 3);          /* the hood and the cockpit after it stay the game's */
    CHECK(fg_at(0x4000, 0x80, 0x40));
    ega_set_layer(EGA_LAYER_OFF);
    enh_race_end();
    restore_ds(&k);
}

/* The mirror's objects are drawn at their own row, far (row 7) to near (row 1). */
static void sr2_enh_mirror_cut_order(void)
{
    static const u32 rgb[16];
    Keep k = keep_ds();
    race_setup();
    DSS(DS_opp_seg) = 120;
    DSS(DS_opp_sprite) = 5;
    DSS(DS_opp_last_d) = 0x500;
    ega_clear_obj(0, 0x2000);
    ega_set_layer(EGA_LAYER_OBJ);
    ega_set_obj(ENH_OBJ_MIR(2)); wr_span(0, 0x110, 0x26, 3);
    ega_set_obj(ENH_OBJ_MIR(5)); wr_span(0, 0x100, 0x1E, 3);
    ega_set_layer(EGA_LAYER_OFF);
    enh_frame_built();
    enh_frame_presented();
    CHECK(enh_compose(small, rgb));
    printf("  log n %d: %d %d\n", enh_cut_log_n, enh_cut_log[0], enh_cut_log[1]);
    CHECK(enh_cut_log_n == 2 && enh_cut_log[0] == ENH_OBJ_MIR(5) && enh_cut_log[1] == ENH_OBJ_MIR(2));
    enh_race_end();
    restore_ds(&k);
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

    /* A road-colour run shorter than ENH_EDGE_MIN_RUN is not the road's edge (the town ground between a kerb
     * and a facade): */
    for (int y = 0; y < 10; y++) { road_row(orig + y * 320, 100, 200); road_row(enh + y * 320, 100, 200); }
    memset(cov, 1, sizeof cov);
    orig[2 * 320 + 260] = orig[2 * 320 + 261] = 8;
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_rows == 10 && c.edge_err_max == 0);
    road_row(orig + 2 * 320, 100, 200);
    memset(orig + 4 * 320, 2, 320);                 /* a row whose only road is 2 px wide: no edge row */
    memset(enh + 4 * 320, 2, 320);
    orig[4 * 320 + 150] = orig[4 * 320 + 151] = enh[4 * 320 + 150] = enh[4 * 320 + 151] = 8;
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_rows == 9 && c.edge_err_max == 0);

    /* Mulholland's truss over the bridge's road (colour 10 bars, SR_SEED 4): the last road run between the bars is
     * 3 px in the original and 2 px in the enhanced view, its right edge 1 px in. A short run that overlaps a run
     * of ENH_EDGE_MIN_RUN or more in the other picture is still the road: the edge is 1 px off, not the next bar's
     * run 20 px in. */
    for (int y = 0; y < 10; y++) { road_row(orig + y * 320, 100, 200); road_row(enh + y * 320, 100, 200); }
    memset(cov, 1, sizeof cov);
    memset(orig + 3 * 320 + 175, 10, 23);           /* bars 175..197, road 198..200 */
    memset(enh + 3 * 320 + 175, 10, 23);
    enh[3 * 320 + 200] = 2;                         /* road 198..199 */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    printf("  truss row: edge_err_max %d (want 1)\n", c.edge_err_max);
    CHECK(c.edge_rows == 10 && c.edge_err_max == 1);
    orig[3 * 320 + 199] = orig[3 * 320 + 200] = 2;  /* the original's run only 1 px: the enhanced 2-px run is no edge */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 0);
}

/* The SKY layer: every pixel written is the sky's, whether its colour changed or not; the background drawn
 * over it later is not. */
static void sr2_enh_sky_tags(void)
{
    enh_init(true, 1, 2, 100, true);
    ega_init();
    vga_mode_0d();
    vga_out(0x3C4, 2); vga_out(0x3C5, 0x0F);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);
    ega_clear_obj(0, 0x2000);
    ega_clear_fg(0, 0x2000);
    ega_set_layer(EGA_LAYER_FG);
    vga_wr(40, 0xFF);                               /* foreground first */
    ega_set_obj(9);
    ega_set_layer(EGA_LAYER_SKY);
    vga_wr(40, 0xFF);                               /* the same colour again */
    const u8 *t = ega_obj();
    int sky = 0;
    for (int b = 0; b < 8; b++) sky += t[40 * 8 + b] == EGA_TAG_SKY;
    CHECK(sky == 8);
    CHECK(ega_fg()[40] == 0);
    ega_set_layer(EGA_LAYER_BG);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xF0);
    vga_wr(40, 0x00);
    CHECK(t[40 * 8 + 0] == 0 && t[40 * 8 + 3] == 0 && t[40 * 8 + 4] == EGA_TAG_SKY);
    ega_set_layer(EGA_LAYER_OFF);
}

static void sr2_enh_compare_log(void)
{
    char path[1024];
    SDL_snprintf(path, sizeof path, "%stest_sr2_enh_compare.log", SDL_GetBasePath());
    SDL_setenv_unsafe("SR_ENH_COMPARE", path, 1);
    CHECK(enh_compare_open());
    EnhCompare a = { .rows = 84, .edge_rows = 80, .edge_err_max = 1, .pixels = 26880, .mismatched = 27,
                     .m_rows = 21, .m_edge_rows = 10, .m_edge_err_max = 0, .m_pixels = 1596, .m_mismatched = 16,
                     .mirror_cov = 1596, .geo_err = 0, .obj_err = 1, .police = 0, .opp_sprite = 5 };
    EnhCompare b = { .rows = 84, .edge_rows = 82, .edge_err_max = 0, .pixels = 26880, .mismatched = 54,
                     .m_rows = 21, .m_edge_rows = 12, .m_edge_err_max = 1, .m_pixels = 1596, .m_mismatched = 0,
                     .mirror_cov = 1596, .geo_err = 0, .obj_err = -1, .police = 2, .opp_sprite = 10 };
    EnhCompare c = { .rows = 82, .edge_rows = 30, .edge_err_max = 1, .pixels = 26240, .mismatched = 0,
                     .m_rows = 21, .m_edge_rows = 0, .m_edge_err_max = 0, .m_pixels = 1596, .m_mismatched = 3,
                     .mirror_cov = 1596, .geo_err = 0, .obj_err = 0, .police = 0, .opp_sprite = 0 };
    enh_compare_frame(1, &a);
    enh_compare_frame(1, &b);
    enh_compare_frame(0, &c);
    enh_compare_skipped(16);
    enh_compare_close();
    SDL_unsetenv_unsafe("SR_ENH_COMPARE");
    size_t len = 0;
    char *text = SDL_LoadFile(path, &len);
    CHECK(text != NULL);
    if (!text) return;
    printf("%s", text);
    static const char want[] =
        "frame 1 mode 1 rows 84 edge_rows 80 edge_err_max 1 mismatch 1 mirror_rows 21 mirror_edge_rows 10 "
        "mirror_edge_err_max 0 mirror_mismatch 10 mirror_cov 1596 geo_err 0 obj_err 1 police 0 opp_sprite 5\n"
        "frame 2 mode 1 rows 84 edge_rows 82 edge_err_max 0 mismatch 2 mirror_rows 21 mirror_edge_rows 12 "
        "mirror_edge_err_max 1 mirror_mismatch 0 mirror_cov 1596 geo_err 0 obj_err -1 police 2 opp_sprite 10\n"
        "frame 3 mode 0 rows 82 edge_rows 30 edge_err_max 1 mismatch 0 mirror_rows 21 mirror_edge_rows 0 "
        "mirror_edge_err_max 0 mirror_mismatch 2 mirror_cov 1596 geo_err 0 obj_err 0 police 0 opp_sprite 0\n"
        "mode 0 frames 1 skipped 0 worst_edge 1 mismatch_p50 0 mismatch_max 0 mirror_worst_edge 0 "
        "mirror_mismatch_p50 2 mirror_mismatch_max 2 mirror_cov_min 1596 geo_max 0 obj_max 0 shown 0\n"
        "mode 1 frames 2 skipped 0 worst_edge 1 mismatch_p50 1 mismatch_max 2 mirror_worst_edge 1 "
        "mirror_mismatch_p50 0 mirror_mismatch_max 10 mirror_cov_min 1596 geo_max 0 obj_max 1 shown 0\n"
        "mode 16 frames 0 skipped 1 worst_edge 0 mismatch_p50 0 mismatch_max 0 mirror_worst_edge 0 "
        "mirror_mismatch_p50 0 mirror_mismatch_max 0 mirror_cov_min 0 geo_max 0 obj_max 0 shown 0\n"
        "frames 3 skipped 1 shown 0\n";
    CHECK(strcmp(text, want) == 0);
    SDL_free(text);
    SDL_RemovePath(path);
}

/* ================================================================================ the Aqueduct (part C) */

/* The track segment and DGROUP (contiguous, the track below) as they were before an Aqueduct test. */
static u8 aq_keep[(0x3E08 - TRK_SEG) * 16 + 0x10000];
static void aq_save(void) { memcpy(aq_keep, mp(SEG(TRK_SEG), 0), sizeof aq_keep); }
static void aq_restore(void) { memcpy(mp(SEG(TRK_SEG), 0), aq_keep, sizeof aq_keep); }

typedef struct { s16 cx, heading, sub, height, pitch, bounce, def[6]; } AqCase;
static const AqCase aq_cases[4] = {
    { 0xA00, 0, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0x400, 7, 40, 0, 2, 0, { 157, 45, 45, 0, 0, 45 } },
    { 0xE00, -12, 63, 0, -3, 10, { 102, -408, -408, -34, -272, 102 } },
    { 0xA00, 3, 16, 0, 1, 36, { 20, 10, -30, 50, 0, 0 } },     /* the bounce (critic D2) */
};

static void aq_set(const AqCase *c)
{
    static const u16 defs[6] = { DS_aq_def_floor_h, DS_aq_def_xl, DS_aq_def_xr, DS_aq_def_top_h, DS_aq_def_top_xl,
                                 DS_aq_def_top_xr };
    DSS(DS_road_mode) = 8;
    DSS(DS_seg) = 100;
    DSS(DS_cam_y) = 0x76C;
    for (int i = 0; i < 6; i++) DSS(defs[i]) = c->def[i];
    DSS(DS_cur_height) = c->height;
    DSS(DS_aq_pitch) = c->pitch;
    DSS(DS_heading) = c->heading;
    DSS(DS_cam_x) = c->cx;
    DSS(DS_substep) = c->sub;
    DSS(DS_view_bounce_y) = c->bounce;
}

/* An exact view of the case, level (roll 0, pivot 0). */
static EnhView aq_view(const AqCase *c)
{
    EnhView v;
    memset(&v, 0, sizeof v);
    v.mode = 8;
    v.exact = true;
    v.lat = c->cx;
    v.heading = c->heading;
    v.pos = 100 + (c->sub >> 3) / 8.0;
    v.aq_dy = (s16)(c->height - c->pitch);
    for (int i = 0; i < 6; i++) v.aq_def[i] = c->def[i];
    v.bounce = c->bounce;
    v.cam_y = 0x76C;
    v.rot_sin = 0;
    v.rot_cos = sin256(90);
    return v;
}

static void aq_roll(EnhView *v, s16 roll, s16 cx, s16 cy)
{
    v->roll = v->roll_raw = roll;
    v->rot_sin = sin256(roll);
    v->rot_cos = sin256((s16)(roll + 90));
    v->piv_x = cx;
    v->piv_y = cy;
    DSS(DS_rot_enable) = roll;
    DSS(DS_rot_sin) = v->rot_sin;
    DSS(DS_rot_cos) = v->rot_cos;
    DSS(DS_rot_cx) = cx;
    DSS(DS_rot_cy) = cy;
}

#define RWT(a, i) DSS((u16)((a) + 2 * (i)))

static int aq_rows_differ(const EnhRowTab *t, int n)
{
    int bad = 0;
    for (int i = 0; i <= n; i++)
        bad += t->y[i] != RWT(DS_row_y, i) || t->xl[i] != RWT(DS_row_xl, i) || t->xr[i] != RWT(DS_row_xr, i)
               || t->ybl[i] != RWT(DS_row_ybank_l, i) || t->ye[i] != RWT(DS_row_yeye, i)
               || t->xol[i] != RWT(DS_row_xol, i) || t->xor_[i] != RWT(DS_row_xor, i);
    return bad;
}

static int aq_trough_differs(const AqRows *r)
{
    return (r->tnl != DSS(AQ_TROUGH_NEAR_XL)) + (r->tnr != DSS(AQ_TROUGH_NEAR_XR)) + (r->tfl != DSS(AQ_TROUGH_FAR_XL))
           + (r->tfr != DSS(AQ_TROUGH_FAR_XR)) + (r->tny != DSS(AQ_TROUGH_NEAR_Y)) + (r->tfy != DSS(AQ_TROUGH_FAR_Y));
}

static void sr2_enh_aq_depths(void)
{
    static const double qs[3] = { 0, 3, 7 };
    int bad = 0;
    for (int j = 0; j < 3; j++) {
        double q = qs[j];
        for (int i = 0; i < 15; i++) {
            double want = i == 0 ? 4 : i <= 11 ? 8 * i + 4 - q : 100 + 8 * (i - 12);
            if (enh_aq_z(i, q) != want) bad++;
        }
        for (int i = 0; i < 8; i++) {
            double want = i == 0 ? 2 : i <= 4 ? 8 * i + q : 8 * i;
            if (enh_aq_mz(i, q) != want) bad++;
        }
    }
    CHECK(bad == 0);
    CHECK(enh_aq_z(11, 3) == 89 && enh_aq_z(14, 3) == 116 && enh_aq_mz(4, 7) == 39 && enh_aq_mz(5, 7) == 40);
}

/* aq_project_front's 15 rows and its trough, with cam_x below 564h and above D5Ch (clamped), a bounce. */
static void sr2_enh_aq_rows_exact(void)
{
    aq_save();
    for (int k = 0; k < 4; k++) {
        aq_set(&aq_cases[k]);
        aq_project_front();
        EnhView v = aq_view(&aq_cases[k]);
        EnhRowTab t;
        memset(&t, 0, sizeof t);
        enh_rows_exact_aq(&v, false, &t);
        AqCam c;
        enh_aq_cam(&v, false, &c);
        AqRows r;
        enh_aq_rows(&c, &r);
        int bad = aq_rows_differ(&t, 14), tb = aq_trough_differs(&r);
        printf("  case %d: rows differ %d trough %d (y14 %d xl0 %d xtr14 %d)\n", k, bad, tb, t.y[14], t.xl[0], t.xor_[14]);
        CHECK(bad == 0);
        CHECK(tb == 0);
    }
    aq_restore();
}

/* aq_project_mirror's 8 rows: the saved deformation, the wall shifts for the wall tops, XC with +heading. */
static void sr2_enh_aq_mirror_rows_exact(void)
{
    aq_save();
    for (int k = 0; k < 4; k++) {
        const AqCase *ac = &aq_cases[k];
        aq_set(ac);
        aq_project_front();
        DSS(DS_aq_saved_floor_h) = ac->def[0];
        DSS(DS_aq_saved_xl) = ac->def[1];
        DSS(DS_aq_saved_xr) = ac->def[2];
        DSS(DS_aq_saved_top_h) = ac->def[3];
        DSS(DS_aq_wall_l_shift) = 32;
        DSS(DS_aq_wall_r_shift) = -48;
        aq_project_mirror();
        EnhView v = aq_view(ac);
        EnhRowTab t;
        memset(&t, 0, sizeof t);
        enh_rows_exact_aq(&v, true, &t);
        AqCam c;
        enh_aq_cam(&v, true, &c);
        AqRows r;
        enh_aq_rows(&c, &r);
        int bad = aq_rows_differ(&t, 7), tb = aq_trough_differs(&r);
        printf("  case %d: rows differ %d trough %d (y7 %d xc0 %d xtl7 %d)\n", k, bad, tb, t.y[7], t.ybl[0], t.xol[7]);
        CHECK(bad == 0);
        CHECK(tb == 0);
    }
    aq_restore();
}

/* The exact rotation is rotate_about_centre's; the smooth one is within 3 px of it near the view: the game's two
 * mul8 floors take up to 2 px together, the sine table's rounding up to 0.5 (|dx| + |dy|) / 256 more. */
static void sr2_enh_aq_rot(void)
{
    aq_save();
    static const s16 rolls[] = { 0, 4, -4, 30, 48, -30, 104, -256 };
    static const s16 xs[] = { -900, -20, 0, 0x7A, 160, 0x13F, 700 }, ys[] = { -200, 0x12, 0x3F, 0x65, 400 };
    int bad = 0, far = 0, n = 0;
    double worst = 0;
    for (int r = 0; r < 8; r++) {
        EnhView v = aq_view(&aq_cases[0]);
        aq_roll(&v, rolls[r], 0x7A, 0x3F);
        AqCam ce, cs;
        enh_aq_cam(&v, false, &ce);
        v.exact = false;
        enh_aq_cam(&v, false, &cs);
        for (int i = 0; i < 7; i++)
            for (int j = 0; j < 5; j++) {
                s16 gx, gy;
                double ox, oy;
                rotate_about_centre(xs[i], ys[j], &gx, &gy);
                enh_aq_rot(&ce, xs[i], ys[j], &ox, &oy);
                n++;
                if (ox != gx || oy != gy) bad++;
                if (abs(xs[i] - 0x7A) > 320 || abs(ys[j] - 0x3F) > 320) continue;
                enh_aq_rot(&cs, xs[i], ys[j], &ox, &oy);
                double e = fmax(fabs(ox - gx), fabs(oy - gy));
                if (e > worst) worst = e;
                if (e > 3) far++;
            }
    }
    printf("  exact %d of %d differ; smooth worst %.2f, %d beyond 3 px\n", bad, n, worst, far);
    CHECK(bad == 0);
    CHECK(far == 0);
    aq_restore();
}

/* A roll step is the game's step wrapped to -180..180; one of more than 60 degrees (the bounce's snap back) is a
 * jump. */
static void sr2_enh_aq_roll_unwrap(void)
{
    bool snap = true;
    CHECK(enh_aq_roll_unwrap(0, 0, 48, &snap) == 48 && !snap);
    CHECK(enh_aq_roll_unwrap(48, 48, 80, &snap) == 80 && !snap);
    CHECK(enh_aq_roll_unwrap(332, 332, 4, &snap) == 364 && !snap);
    snap = false;
    CHECK(enh_aq_roll_unwrap(304, 304, 48, &snap) == 408 && snap);
    snap = false;
    CHECK(enh_aq_roll_unwrap(-302, -302, -30, &snap) == -390 && snap);
    CHECK(enh_aq_roll_unwrap(0, 0, -4, &snap) == -4 && !snap);
}

static void sr2_enh_aq_sky_x(void)
{
    static const struct { double h; int s, c, m; } t[4] = { { 5, 5, 10, -2 }, { -5, -5, -10, 3 }, { 170, 10, 20, -5 },
                                                            { 16400, -48, -96, 24 } };
    for (int i = 0; i < 4; i++) {
        double s = enh_aq_sky_x(t[i].h, ENH_AQ_STARS, true), c = enh_aq_sky_x(t[i].h, ENH_AQ_CLOUDS, true);
        double m = enh_aq_sky_x(t[i].h, ENH_AQ_MIR_STARS, true);
        printf("  heading %.0f: %.0f %.0f %.0f\n", t[i].h, s, c, m);
        CHECK(s == t[i].s && c == t[i].c && m == t[i].m);
    }
    CHECK(enh_aq_sky_x(5.5, ENH_AQ_STARS, false) == 5.5);
    CHECK(enh_aq_sky_x(5.5, ENH_AQ_CLOUDS, false) == 11);
    CHECK(enh_aq_sky_x(5.5, ENH_AQ_MIR_STARS, false) == -2.75);
}

/* The flags of row i: the front's TRK_AQ[base + i], read as aq_flag(14 - i) reads them; the mirror's
 * AQM[179h - base - (7 - i)] = AQ[base - 1 - i] (the critic's identity), as aq_flag(7 - i) reads them. */
static void sr2_enh_aq_flags(void)
{
    aq_save();
    for (int i = 0; i < 0x172; i++) TRKW(TRK_AQ, i) = TRKW(TRK_AQM, i) = 0;
    aq_build();
    EnhView v = aq_view(&aq_cases[0]);
    AqCam c;
    enh_aq_cam(&v, false, &c);
    FarPtr p = trk_ptr(TRK_AQ, 100);
    p.off = (u16)(p.off + 0x1C);
    int bad = 0, set = 0;
    for (int i = 0; i <= 14; i++) {
        u16 f = enh_aq_flag(&c, i);
        set += f != 0;
        if (f != TRKW(TRK_AQ, 100 + i) || f != rd16(p.seg, (u16)(p.off - 2 * (14 - i)))) bad++;
    }
    AqCam m;
    enh_aq_cam(&v, true, &m);
    p = far_make(SEG(TRK_SEG), (u16)(0x6570 - 2 * 100));
    p.off = (u16)(p.off + 0x10);
    for (int i = 0; i <= 7; i++) {
        u16 f = enh_aq_flag(&m, i);
        set += f != 0;
        if (f != TRKW(TRK_AQ, 100 - 1 - i) || f != rd16(p.seg, (u16)(p.off - 2 * (7 - i)))) bad++;
    }
    printf("  %d differ, %d set\n", bad, set);
    CHECK(bad == 0);
    CHECK(set > 0);
    aq_restore();
}

/* opp_bank_variant's bank: the opponent's lane against the walls, the roll (and a quarter of a small left roll). */
static void sr2_enh_aq_opp_bank(void)
{
    aq_save();
    static const s16 lats[] = { 0x400, 0x5A0, 0x600, 0x900, 0xCC0, 0xD40, 0xE00 }, sxs[] = { 0, 40, -60 };
    static const s16 rolls[] = { 0, 48, -30, -100, 200 };
    int bad = 0, steep = 0, far = 0;
    for (int a = 0; a < 7; a++)
        for (int b = 0; b < 3; b++)
            for (int r = 0; r < 5; r++) {
                DSS(DS_opp_lat) = lats[a];
                DSS(DS_aq_saved_xl) = sxs[b];
                DSS(DS_aq_saved_xr) = sxs[b];
                DSS(DS_rot_enable) = rolls[r];
                opp_bank_variant();
                EnhView v = aq_view(&aq_cases[0]);
                v.aq_def[1] = v.aq_def[2] = sxs[b];
                v.roll = v.roll_raw = rolls[r];
                AqCam c;
                enh_aq_cam(&v, false, &c);
                double e = enh_aq_opp_bank(&c, lats[a]);
                if (e != DSS(DS_opp_bank)) bad++;
                if (abs(DSS(DS_opp_bank)) >= 56) steep++;
                v.exact = false;
                enh_aq_cam(&v, false, &c);
                if (fabs(enh_aq_opp_bank(&c, lats[a]) - e) > 2) far++;
            }
    printf("  %d differ, %d steep, %d smooth beyond 2\n", bad, steep, far);
    CHECK(bad == 0);
    CHECK(steep > 0);
    CHECK(far == 0);
    aq_restore();
}

/* opponent_draw's wy: the floor, lifted up the walls by the opponent's lane. */
static void sr2_enh_aq_opp_wy(void)
{
    aq_save();
    DSS(DS_aq_wall_l_shift) = 0;
    DSS(DS_aq_wall_r_shift) = 0;
    EnhView v = aq_view(&aq_cases[0]);
    AqCam c;
    enh_aq_cam(&v, false, &c);
    printf("  %.0f %.0f %.0f\n", enh_aq_opp_wy(&c, 0x500), enh_aq_opp_wy(&c, 0xD00), enh_aq_opp_wy(&c, 0xA00));
    CHECK(enh_aq_opp_wy(&c, 0x500) == 1864);
    CHECK(enh_aq_opp_wy(&c, 0xD00) == 1994);
    CHECK(enh_aq_opp_wy(&c, 0xA00) == 2000);
    DSS(DS_aq_wall_l_shift) = 0x20;
    static const s16 d[6] = { 20, 10, 0, 50, 0, 0 };
    for (int i = 0; i < 6; i++) v.aq_def[i] = d[i];
    enh_aq_cam(&v, false, &c);
    printf("  %.0f\n", enh_aq_opp_wy(&c, 0x500));
    CHECK(enh_aq_opp_wy(&c, 0x500) == 1854);
    aq_restore();
}

/* opponent_draw's place in mode 8: its depth, the projection of its picture, rotated about the pivot of the pass
 * that draws it. */
static void sr2_enh_aq_opp_anchor(void)
{
    aq_save();
    const AqCase *ac = &aq_cases[2];
    aq_set(ac);
    aq_project_front();
    EnhView v = aq_view(ac);
    aq_roll(&v, 48, 0x7A, 0x3F);
    AqCam f, m;
    enh_aq_cam(&v, false, &f);
    enh_aq_cam(&v, true, &m);
    s16 dy = (s16)(ac->height - ac->pitch), rx, ry;
    double ax, ay, z;
    /* picture 5 ahead, the front's pass */
    s16 lat = 0x900, wy = (s16)enh_aq_opp_wy(&f, lat);
    CHECK(enh_aq_opp_anchor(&f, &m, 300, 5, lat, 0, false, &ax, &ay, &z));
    rotate_about_centre(road_proj_x(41, lat, ac->heading), road_proj_y(41, wy, dy), &rx, &ry);
    printf("  5: z %.0f (%.0f, %.0f) want (%d, %d)\n", z, ax, ay, rx, ry);
    CHECK(z == 41 && ax == rx && ay == ry);
    /* picture 2 off the right side: averaged with the last sx, its y clamped */
    lat = 0xF00;
    wy = (s16)enh_aq_opp_wy(&f, lat);
    s16 sx = road_proj_x(4, lat, ac->heading), sy = road_proj_y(4, wy, dy);
    CHECK(sx > 0x13F);
    sx = (s16)((sx + 150) / 2);
    if (sy > 100) sy = 100;
    rotate_about_centre(sx, sy, &rx, &ry);
    CHECK(enh_aq_opp_anchor(&f, &m, 0, 2, lat, 150, false, &ax, &ay, &z));
    printf("  2: z %.0f (%.0f, %.0f) want (%d, %d)\n", z, ax, ay, rx, ry);
    CHECK(z == 4 && ax == rx && ay == ry);
    /* picture 10 behind, the mirror's pass: the mirror's projection, about the mirror's pivot */
    lat = 0x900;
    wy = (s16)enh_aq_opp_wy(&f, lat);
    DSS(DS_rot_cx) = (s16)m.pcx;
    DSS(DS_rot_cy) = (s16)m.pcy;
    rotate_about_centre(road_proj_x_mirror(12, lat, (s16)(10 - ac->heading)), road_proj_y_mirror(12, wy, (s16)-dy), &rx, &ry);
    CHECK(enh_aq_opp_anchor(&f, &m, -100, 10, lat, 0, true, &ax, &ay, &z));
    printf("  10: z %.0f (%.0f, %.0f) want (%d, %d), mirror pivot (%.0f, %.0f)\n", z, ax, ay, rx, ry, m.pcx, m.pcy);
    CHECK(z == 12 && ax == rx && ay == ry);
    /* picture 3 just behind, drawn by the mirror's pass: the front's projection about the mirror's pivot */
    sx = road_proj_x(9, lat, ac->heading);
    sy = road_proj_y(9, wy, dy);
    if (sy > 100) sy = 100;
    rotate_about_centre(sx, sy, &rx, &ry);
    CHECK(enh_aq_opp_anchor(&f, &m, -40, 3, lat, 0, true, &ax, &ay, &z));
    printf("  3: z %.0f (%.0f, %.0f) want (%d, %d)\n", z, ax, ay, rx, ry);
    CHECK(z == 9 && ax == rx && ay == ry);
    CHECK(!enh_aq_opp_anchor(&f, &m, 0, 13, lat, 0, false, &ax, &ay, &z));
    CHECK(!enh_aq_opp_anchor(&f, &m, 0, -1, lat, 0, false, &ax, &ay, &z));
    aq_restore();
}

/* The rotated rows as aq_rotate_rows(14) leaves them (k = 14 - i), and the trough. */
static void sr2_enh_aqtab_exact(void)
{
    aq_save();
    const AqCase *ac = &aq_cases[2];
    aq_set(ac);
    aq_project_front();
    EnhView v = aq_view(ac);
    aq_roll(&v, 48, 0x7A, 0x3F);
    for (int i = 0; i <= 14; i++) {
        int k = 14 - i;
        s16 x, y;
        rotate_about_centre(RWT(DS_row_xl, i), RWT(DS_row_y, i), &x, &y);   RWT(AQ_R_LX, k) = x;  RWT(AQ_R_LY, k) = y;
        rotate_about_centre(RWT(DS_row_xr, i), RWT(DS_row_y, i), &x, &y);   RWT(AQ_R_RX, k) = x;  RWT(AQ_R_RY, k) = y;
        rotate_about_centre(RWT(DS_row_xol, i), RWT(DS_row_yeye, i), &x, &y); RWT(AQ_R_TLX, k) = x; RWT(AQ_R_TLY, k) = y;
        rotate_about_centre(RWT(DS_row_xor, i), RWT(DS_row_yeye, i), &x, &y); RWT(AQ_R_TRX, k) = x; RWT(AQ_R_TRY, k) = y;
    }
    EnhAqTab mine, read;
    memset(&mine, 0, sizeof mine);
    memset(&read, 0, sizeof read);
    enh_aqtab_read(&read);
    enh_aqtab_exact(&v, false, &mine);
    int e = enh_aqtab_err(&mine, &read, 14);
    printf("  err %d (lx0 %d/%d try14 %d/%d)\n", e, mine.lx[0], read.lx[0], mine.try_[14], read.try_[14]);
    CHECK(e == 0);
    CHECK(read.roll == 48 && read.cx == 0x7A && read.cy == 0x3F);
    aq_restore();
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
    RUN(sr2_enh_mark_fg_clears_tags);
    RUN(sr2_enh_base_layer);
    RUN(sr2_enh_capture_regions);
    RUN(sr2_enh_cut_order);
    RUN(sr2_enh_race_begin_resets);
    RUN(sr2_enh_cut_hidden);
    RUN(sr2_enh_cut_overdrawn);
    RUN(sr2_enh_obj_geo_continuous);
    RUN(sr2_enh_opp_depth);
    RUN(sr2_enh_series_snap);
    RUN(sr2_enh_heading_wrap);
    RUN(sr2_enh_obj_geo_exact_slope);
    RUN(sr2_enh_opp_drawn_d);
    RUN(sr2_enh_opp_drawn_sx);
    RUN(sr2_enh_fallen_not_composed);
    RUN(sr2_enh_proj_exact);
    RUN(sr2_enh_rows_front_row0);
    RUN(sr2_enh_town_face_clipped);
    RUN(sr2_enh_town_end_face_rejected);
    RUN(sr2_enh_top_band_row);
    RUN(sr2_enh_dash_width);
    RUN(sr2_enh_sky_shift);
    RUN(sr2_enh_dash_exact);
    RUN(sr2_enh_strip_tracks);
    RUN(sr2_enh_wall_colour);
    RUN(sr2_enh_clip_poly_i);
    RUN(sr2_enh_poly_incl);
    RUN(sr2_enh_mulholland_rows);
    RUN(sr2_enh_hill_consts);
    RUN(sr2_enh_mulholland_sky_dy);
    RUN(sr2_enh_rail_post);
    RUN(sr2_enh_rail_phase);
    RUN(sr2_enh_line1);
    RUN(sr2_enh_mirror_depths);
    RUN(sr2_enh_proj_mirror_exact);
    RUN(sr2_enh_mirror_yk);
    RUN(sr2_enh_mirror_opp_geo);
    RUN(sr2_enh_mirror_hooks);
    RUN(sr2_enh_mirror_cut_order);
    RUN(sr2_enh_sky_tags);
    RUN(sr2_enh_compare_rows);
    RUN(sr2_enh_compare_log);
    RUN(sr2_enh_aq_depths);
    RUN(sr2_enh_aq_rows_exact);
    RUN(sr2_enh_aq_mirror_rows_exact);
    RUN(sr2_enh_aq_rot);
    RUN(sr2_enh_aq_roll_unwrap);
    RUN(sr2_enh_aq_sky_x);
    RUN(sr2_enh_aq_flags);
    RUN(sr2_enh_aq_opp_bank);
    RUN(sr2_enh_aq_opp_wy);
    RUN(sr2_enh_aq_opp_anchor);
    RUN(sr2_enh_aqtab_exact);
    host_shutdown();
    return failures ? 1 : 0;
}
