/* test_sr2_tags.c -- the remaster's per-pixel tags (common/remaster/rm_tags.c) through Street Rod 2's VGA
 * model and blit helpers (src2/platform ENH parts), as tests/test_tags.c does for Street Rod 1, and the
 * picture keys of src2/remaster/rm_lib.c with a pack the test writes (opened after the tag tests), and the
 * enhanced race view's cut objects drawn from that pack's art.
 *   test_sr2_tags <SR2 game dir>
 * Loads SR2's SR.EXE (mem_load_exe) for its DGROUP; the bitmaps live in segments 8000h-9FFFh. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "enhanced/enh_draw.h"
#include "enhanced/enhanced.h"
#include "mem.h"
#include "symbols.h"
#include "platform/ega.h"
#include "platform/platform.h"
#include "platform/vga.h"
#include "platform/video.h"
#include "remaster/rm_dump.h"
#include "remaster/rm_lib.h"
#include "remaster/rm_pack.h"
#include "remaster/rm_tags.h"

static int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
            failures++;                                                                \
        }                                                                              \
    } while (0)

#define RAM  0x8000         /* scratch segment for bitmaps */
#define RAM2 0x8100

static void setup(void)
{
    memset(mem + lin(0x8000, 0), 0, 0x20000);             /* segments 8000h-9FFFh, never the program image */
    for (int k = 0; k < 4; k++) memset(ega_plane(k), 0, EGA_PLANE_SIZE);
    vga_mode_0d();
    DSW(DS_g_nplanes) = 4;
    DSW(DS_g_px_shift) = 3;
    DSW(DS_g_px_factor) = 1;
    static const u8 lmask[9] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF };
    for (int i = 0; i < 9; i++) DSB(DS_g_lmask + i) = lmask[i];
    if (rm_tags_on()) rm_tags_reset();
}

/* A 16 x 2 bitmap at RAM:0 (stride 2, plane k at 4k), plane bits making colour (x + y) & 15. */
static void make_bitmap16(void)
{
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 16; x++)
            for (int k = 0; k < 4; k++)
                if ((x + y) >> k & 1) mem[lin(RAM, (u16)(k * 4 + y * 2 + x / 8))] |= (u8)(0x80 >> (x & 7));
}

static void copy_bitmap16_to_vram(void)
{
    for (int k = 0; k < 4; k++) {
        vga_out(0x3C4, 2);
        vga_out(0x3C5, (u8)(1 << k));
        for (int y = 0; y < 2; y++) vmovedata(RAM, (u16)(k * 4 + y * 2), 0xA000, (u16)(y * 0x28), 2);
    }
    vga_out(0x3C5, 0x0F);
}

/* VRAM byte off: pixel x of it shows picture `slot` pixel (base + x, 0). */
static void seed_vram_byte(u16 off, int slot, int base)
{
    for (int b = 0; b < 8; b++) rm_set_at(0xA000, off, b, rm_tag(slot, base + 7 - b, 0), 0);
}

/* A 320 x 200 screen page descriptor over A000:0. */
static FarPtr screen_desc(u16 seg)
{
    FarPtr d = far_make(seg, 0);
    bitmap_init(d, far_make(0xA000, 0), far_make(0, 0), 320, 200, DESC_TYPE_EGA);
    return d;
}

/* The same sequence with the tags off and on leaves the same pixels. */
static void run_sequence(u8 out[4][0x80])
{
    setup();
    make_bitmap16();
    copy_bitmap16_to_vram();
    vga_out(0x3CE, 5); vga_out(0x3CF, 1);
    vmovedata(0xA000, 0, 0xA000, 0x50, 2);
    vga_out(0x3CE, 5); vga_out(0x3CF, 2);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0x3C);
    vga_rd(0x51);
    vga_wr(0x51, 9);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);
    vga_out(0x3CE, 5); vga_out(0x3CF, 0);
    for (int k = 0; k < 4; k++) memcpy(out[k], ega_plane(k), 0x80);
}

static u8 off_planes[4][0x80];
static void sr2_off_changes_nothing(void)
{
    u8 on[4][0x80];
    run_sequence(on);
    CHECK(memcmp(on, off_planes, sizeof on) == 0);
}

/* SR1's seed_then_ram_to_vram_copy, latch_copy_moves_tags and text_write_clears. */
static void sr2_seed_copy_latch_text(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
    CHECK(rm_vtag(0, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(1, 0) == rm_tag(5, 15, 0));
    CHECK(rm_vtag(0x28, 7) == rm_tag(5, 0, 1));
    CHECK(rm_vtag(2, 7) == 0);

    vga_out(0x3CE, 5); vga_out(0x3CF, 1);                    /* write mode 1 */
    vmovedata(0xA000, 0, 0xA000, 100, 2);
    vga_out(0x3CE, 5); vga_out(0x3CF, 0);
    CHECK(rm_vtag(100, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(101, 0) == rm_tag(5, 15, 0));

    vga_out(0x3CE, 5); vga_out(0x3CF, 2);                    /* write mode 2 */
    vga_out(0x3CE, 8); vga_out(0x3CF, 0x0F);                 /* bit mask: the 4 right pixels */
    vga_rd(0);
    vga_wr(0, 7);
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);
    vga_out(0x3CE, 5); vga_out(0x3CF, 0);
    CHECK(rm_vtag(0, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(0, 4) == rm_tag(5, 3, 0));
    CHECK(rm_vtag(0, 3) == 0);
    CHECK(rm_vtag(0, 0) == 0);
}

/* An 8 x 1 sprite at RAM2 (planes 0..3, mask at 4: 0Fh = the right 4 pixels transparent) over a picture
 * already on the screen. */
static void sprite8_over_picture(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
    for (int k = 0; k < 4; k++) mem[lin(RAM2, (u16)k)] = 0xF0;    /* colour 15 on the left half */
    mem[lin(RAM2, 4)] = 0x0F;
    rm_seed_bitmap(far_make(RAM2, 0), 8, 1, 1, 7);
}

static void check_sprite8_over_picture(void)
{
    CHECK(rm_vtag(0, 7) == rm_tag(7, 0, 0));
    CHECK(rm_vunder(0, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(0, 4) == rm_tag(7, 3, 0));
    CHECK(rm_vtag(0, 3) == rm_tag(5, 4, 0));
    CHECK(rm_vunder(0, 3) == 0);
}

static void sr2_masked_or_sets_under(void)
{
    sprite8_over_picture();
    vga_out(0x3CE, 4); vga_out(0x3C4, 2);
    for (int k = 0; k < 4; k++) {
        vga_out(0x3CF, (u8)k);
        vga_out(0x3C5, (u8)(1 << k));
        rows_mask_or(far_make(0xA000, 0), RAM2, (u16)k, 4, 1, 1, 1, 0x28);
    }
    vga_out(0x3C5, 0x0F);
    check_sprite8_over_picture();
}

/* The same through ega_blit_masked mode 0 (rows_mask_blend). */
static void sr2_masked_blend_sets_under(void)
{
    sprite8_over_picture();
    FarPtr s = far_make(0x8200, 0);
    bitmap_init(s, far_make(RAM2, 0), far_make(RAM2, 4), 8, 1, 0);
    FarPtr d = screen_desc(0x8300);
    Rect r = { .w = 8, .h = 1, .sx = 0, .sy = 0, .dx = 0, .dy = 0 };
    CHECK(ega_blit_masked(s, d, &r, 0) == 0x0F);
    check_sprite8_over_picture();
}

/* bitmap_make_shifted by 3: the tag of source pixel u lands on destination pixel u + 3. */
static void sr2_make_shifted_moves_tags(void)
{
    FarPtr s = far_make(0x8200, 0), d = far_make(0x8300, 0);
    FarPtr sp = far_make(0x8200, 0x40), dp = far_make(0x8300, 0x40);
    for (int k = 0; k < 4; k++) mem[lin(sp.seg, (u16)(sp.off + k))] = 0xFF;
    mem[lin(sp.seg, (u16)(sp.off + 4))] = 0x00;                    /* mask: all opaque */
    bitmap_init(s, sp, far_add(sp, 4), 8, 1, 0);
    far_wr(d.seg, (u16)(d.off + DESC_PLANES), dp);
    rm_seed_bitmap(sp, 8, 1, 1, 9);
    bitmap_make_shifted(s, 3, d);
    CHECK(rm_mtag(far_lin(dp), 7 - 3) == rm_tag(9, 0, 0));
    CHECK(rm_mtag(far_lin(dp), 0) == rm_tag(9, 4, 0));
    CHECK(rm_mtag(far_lin(dp) + 1, 7) == rm_tag(9, 5, 0));
    CHECK(rm_mtag(far_lin(dp), 7) == 0);
}

/* The mouse pointer: a 24 x 384 bank at 8400:0 (stride 3, planes 0x480 apart) seeded as slot 2, its bit
 * mask F0h (the left 4 pixels drawn) at 8400:1800h (8400:2000h would be the save buffer 8600:0 itself),
 * over VRAM byte 0 showing slot 5. */
static void sr2_cursor_moves_tags(void)
{
    rm_seed_bitmap(far_make(0x8400, 0), 24, 384, 3, 2);
    mem[lin(0x8400, 0x1800)] = 0xF0;
    seed_vram_byte(0, 5, 0);
    ega_draw_cursor(far_make(0x8400, 0), far_make(0x8400, 0x1800), far_make(0x8600, 0), far_make(0xA000, 0), 0, 0, 1, 1);
    CHECK(rm_vtag(0, 7) == rm_tag(2, 0, 0));
    CHECK(rm_vunder(0, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(0, 3) == rm_tag(5, 4, 0));
    CHECK(rm_mtag(lin(0x8600, 0), 7) == rm_tag(5, 0, 0));
}

/* ega_blit_shifted: a 16 x 1 RAM bitmap (slot 9) to the screen at a pixel x, right and left shifts. */
static void sr2_blit_shifted_moves_tags(void)
{
    FarPtr s = far_make(0x8200, 0), sp = far_make(0x8200, 0x40);
    bitmap_init(s, sp, far_make(0, 0), 16, 1, 0);
    rm_seed_bitmap(sp, 16, 1, 2, 9);
    FarPtr d = screen_desc(0x8300);
    Rect r = { .w = 16, .h = 1, .sx = 0, .sy = 0, .dx = 3, .dy = 0 };
    ega_blit_shifted(s, d, &r);
    CHECK(rm_vtag(0, 7 - 3) == rm_tag(9, 0, 0));
    CHECK(rm_vtag(1, 7 - 3) == rm_tag(9, 8, 0));
    CHECK(rm_vtag(0, 7) == 0);
    Rect l = { .w = 8, .h = 1, .sx = 5, .sy = 0, .dx = 1, .dy = 0 };
    ega_blit_shifted(s, d, &l);
    CHECK(rm_vtag(0, 6) == rm_tag(9, 5, 0));
}

/* ega_save_rect: a latched copy from the screen into a RAM buffer carries the tags. */
static void sr2_save_rect_to_ram(void)
{
    seed_vram_byte(0, 5, 0);
    seed_vram_byte(1, 5, 8);
    ega_save_rect(far_make(0xA000, 0), 2, 1, far_make(0x8800, 0), 0x28);
    CHECK(rm_mtag(lin(0x8800, 0), 7) == rm_tag(5, 0, 0));
    CHECK(rm_mtag(lin(0x8800, 1), 0) == rm_tag(5, 15, 0));
}

/* ega_sprite_over_bg with a tail: a 16 x 2 sprite (slot 7) over a 16 x 2 background (slot 5) to the
 * screen, its second row the tail copied from VRAM 1000h, which holds holes (the ground fill). A copied
 * hole leaves the destination as it is, so the destination's tail row holds holes too (and one car pixel
 * over a hole, as test_tags.c's tail_holes_take_background): they take the background's tags. */
static void sr2_tail_holes(void)
{
    FarPtr spr = far_make(0x8200, 0), sp = far_make(0x8200, 0x40);
    bitmap_init(spr, sp, far_add(sp, 16), 16, 2, 0);              /* mask 00h: all opaque */
    rm_seed_bitmap(sp, 16, 2, 2, 7);
    FarPtr bg = far_make(0x8300, 0), bp = far_make(0x8300, 0x40);
    bitmap_init(bg, bp, far_make(0, 0), 16, 2, 0);
    rm_seed_bitmap(bp, 16, 2, 2, 5);
    FarPtr d = screen_desc(0x8400);
    rm_holes(0xA000, 0x1000, 2);
    rm_holes(0xA000, 0x28, 2);
    rm_set_at(0xA000, 0x29, 0, rm_tag(7, 1, 1), RM_HOLE);
    Tail t = { .off = 0x1000, .seg = 0xA000, .unused4 = 0, .rows = 1 };
    Rect r = { .w = 16, .h = 2, .sx = 0, .sy = 0, .dx = 0, .dy = 0 };
    ega_sprite_over_bg(spr, d, bg, &t, &r);
    for (int x = 0; x < 15; x++) CHECK(rm_vtag((u16)(0x28 + x / 8), 7 - (x & 7)) == rm_tag(5, x, 1));
    CHECK(rm_vtag(0x29, 0) == rm_tag(7, 1, 1));
    CHECK(rm_vunder(0x29, 0) == rm_tag(5, 15, 1));
    CHECK(rm_vtag(0, 7) == rm_tag(7, 0, 0));                       /* the main row: the sprite */
}

/* bitmap_composite_behind: s (slot 7, all opaque) behind d (slot 5, its left 4 pixels transparent). */
static void sr2_composite_behind_shows_behind(void)
{
    FarPtr s = far_make(0x8200, 0), sp = far_make(0x8200, 0x40);
    bitmap_init(s, sp, far_add(sp, 4), 8, 1, 0);
    for (int k = 0; k < 4; k++) mem[lin(sp.seg, (u16)(sp.off + k))] = 0xFF;
    rm_seed_bitmap(sp, 8, 1, 1, 7);
    FarPtr d = far_make(0x8300, 0), dp = far_make(0x8300, 0x40);
    bitmap_init(d, dp, far_add(dp, 4), 8, 1, 0);
    mem[lin(dp.seg, (u16)(dp.off + 4))] = 0xF0;
    rm_seed_bitmap(dp, 8, 1, 1, 5);
    bitmap_composite_behind(s, d, 0, 0);
    CHECK(rm_mtag(far_lin(dp), 7) == rm_tag(7, 0, 0));
    CHECK(rm_mtag(far_lin(dp), 3) == rm_tag(5, 4, 0));
}

/* bitmap_clear (the shrunk and rotated race sprites' destinations) clears the tags it writes. */
static void sr2_bitmap_clear_clears_tags(void)
{
    FarPtr b = far_make(0x8200, 0), bp = far_make(0x8200, 0x40);
    bitmap_init(b, bp, far_make(0, 0), 16, 2, 0);
    rm_seed_bitmap(bp, 16, 2, 2, 5);
    bitmap_clear(b);
    int left = 0;
    for (u32 i = 0; i < 16; i++)
        for (int bit = 0; bit < 8; bit++) left += rm_mtag(far_lin(bp) + i, bit) != 0;
    CHECK(left == 0);
}

/* pic_decode seeds the decoded bitmap as slot pic_index(id) + 1: picture 1 (8 x 1, unmasked) whose packed
 * bytes (BCh and 4 literals, one per plane) already sit in the destination's planes, as SR2 decodes. */
static const u8 decode_packed[5] = { 0xBC, 0x81, 0x42, 0x24, 0x18 };
static const u8 decode_row[8] = { 1, 2, 4, 8, 8, 4, 2, 1 };      /* what those planes show */

static FarPtr decode_fixture(void)
{
    DSS(DS_driver_id) = -2;
    DSB(DS_cursor_visible) = 0;                                    /* cursor_ctl(-4) / (-2) return at once */
    DSW(DS_picdir_seg) = 0x9000;
    FarPtr e = far_make(0x9000, 0);
    FarPtr data = far_make(0x8A00, 0x40), dst = far_make(0x8A00, 0);
    wr16(e.seg, 0x00, 8);                                          /* w */
    wr16(e.seg, 0x02, 1);                                          /* h */
    wr8(e.seg, 0x05, 0);                                           /* not masked */
    wr16(e.seg, 0x1A, 5);                                          /* packed size */
    wr8(e.seg, 0x1C, 0xFF);                                        /* not parked */
    far_wr(e.seg, 0x1E, data);                                     /* loaded: the destination's tail */
    for (int i = 0; i < 5; i++) wr8(data.seg, (u16)(data.off + i), decode_packed[i]);
    far_wr(dst.seg, (u16)(dst.off + DESC_PLANES), data);
    return dst;
}

static void sr2_decode_seeds_tags(void)
{
    FarPtr dst = decode_fixture(), data = far_make(0x8A00, 0x40);
    pic_decode(1, dst);
    for (int k = 0; k < 4; k++) CHECK(rd8(data.seg, (u16)(data.off + k)) == decode_packed[k + 1]);
    CHECK(rm_mtag(far_lin(data), 7) == rm_tag(1, 0, 0));
    CHECK(rm_mtag(far_lin(data) + 3, 0) == rm_tag(1, 7, 0));
}

/* ---- with a pack (rm_lib.c's hooks are no-ops before) */

/* A picture parked in VRAM is hashed from its plane directly: the latches stay as the last read left them. */
static void sr2_hash_parked_keeps_latches(void)
{
    static const u8 packed[5] = { 0xBC, 0x11, 0x22, 0x33, 0x44 };
    for (int i = 0; i < 5; i++) ega_plane(2)[0x7D00 + i] = packed[i];
    static const u8 other[4] = { 0x5A, 0x6B, 0x7C, 0x8D };
    for (int k = 0; k < 4; k++) ega_plane(k)[0x100] = other[k];
    vga_rd(0x100);                                                 /* the latches: 5Ah 6Bh 7Ch 8Dh */
    rm2_pic_hash(30, far_make(0xA7D0, 0), 5, 2);
    vga_out(0x3CE, 5); vga_out(0x3CF, 1);                          /* write mode 1: the latches */
    vga_wr(0x200, 0);
    vga_out(0x3CE, 5); vga_out(0x3CF, 0);
    char want[41];
    rm_sha1_hex(packed, 5, want);
    CHECK(rm2_slot_sha1(30) && strcmp(rm2_slot_sha1(30), want) == 0);
    for (int k = 0; k < 4; k++) CHECK(ega_plane(k)[0x200] == other[k]);
}

/* The key is the SHA-1 of the packed bytes before the in-place unpack overwrites them; the picture is
 * registered with its decoded pixels and the art of the pack. */
static void sr2_decode_hash_in_place(void)
{
    FarPtr dst = decode_fixture(), data = far_make(0x8A00, 0x40);
    pic_decode(1, dst);
    char want[41];
    rm_sha1_hex(decode_packed, 5, want);
    CHECK(rm2_slot_sha1(1) && strcmp(rm2_slot_sha1(1), want) == 0);
    RmPic *p = rm_pic(1);
    CHECK(p != NULL);
    if (p) {
        CHECK(p->w == 8 && p->h == 1);
        for (int x = 0; x < 8; x++) CHECK(p->orig[x] == decode_row[x]);
    }
    CHECK(rm_mtag(far_lin(data), 7) == rm_tag(1, 0, 0));
}

/* The enhanced race view cuts a row's objects out of the page shown: the cut keeps the VRAM tags of its pixels
 * that verify (the picture has art, the tag's original colour is the pixel's), so they are drawn from the art
 * (ENH_RGB). The tags are taken anew at each capture: a later frame without them draws the indices. Needs
 * picture 1 registered (sr2_decode_hash_in_place). */
static void sr2_enh_cut_keeps_verified_tags(void)
{
    static u32 xrgb[320 * 200];
    const int id = 3, y = 0x30;
    const u16 off = (u16)(y * EGA_BYTES_PER_LINE + 1);            /* pixels 8..15 of row 30h, page 0 */
    if (!rm_pic(1)) { CHECK(rm_pic(1) != NULL); return; }
    enh_init(true, 1, 1, 100, true);
    DSS(DS_road_mode) = 1;                                         /* the street drag, racing */
    DSB(DS_racing) = 1;
    DSS(DS_shown_page) = 0;
    ega_set_line_compare(200);
    enh_race_begin();
    enh_frame_built();
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);
    ega_set_obj((u8)id);
    ega_set_layer(EGA_LAYER_OBJ);                                  /* the row's object: decode_row's colours */
    for (int k = 0; k < 4; k++) {
        u8 bits = 0;
        for (int i = 0; i < 8; i++) bits |= (u8)((decode_row[i] >> k & 1) << (7 - i));
        vga_out(0x3C4, 2); vga_out(0x3C5, (u8)(1 << k));
        vga_wr(off, bits);
    }
    ega_set_layer(EGA_LAYER_OFF);
    for (int i = 0; i < 7; i++) rm_set_at(0xA000, off, 7 - i, rm_tag(1, i, 0), 0);
    rm_set_at(0xA000, off, 0, rm_tag(1, 1, 0), 0);                /* colour 1 under a tag whose original is 2 */
    enh_frame_presented();
    CHECK(enh_compose(xrgb, ega_palette));
    const u8 *px = enh_buf.px + (size_t)(y - ENH_VIEW_Y0) * enh_buf.w;
    const u32 *rgb = enh_buf.rgb ? enh_buf.rgb + (size_t)(y - ENH_VIEW_Y0) * enh_buf.w : NULL;
    CHECK(rgb != NULL);
    for (int i = 0; i < 7; i++) {
        CHECK(px[8 + i] == ENH_RGB);
        if (rgb && px[8 + i] == ENH_RGB) CHECK(rgb[8 + i] == ega_palette[decode_row[i]]);
    }
    CHECK(px[15] == decode_row[7]);                                /* not verified: the original's index */
    rm_tags_reset();                                               /* the next frame's pixels carry no tags */
    enh_frame_presented();
    CHECK(enh_compose(xrgb, ega_palette));
    for (int i = 0; i < 8; i++) CHECK(px[8 + i] == decode_row[i]);
    enh_race_end();
}

/* The pack: one picture, the key of decode_packed, 32 x 4 art. */
static bool open_pack(char *dir, size_t n)
{
    snprintf(dir, n, "sre-sr2-tags-%d", (int)SDL_GetTicks());     /* in the working directory */
    char path[1024], sha1[41];
    rm_sha1_hex(decode_packed, 5, sha1);
    SDL_CreateDirectory(dir);
    snprintf(path, sizeof path, "%s/pics", dir);
    SDL_CreateDirectory(path);
    snprintf(path, sizeof path, "%s/pack.json", dir);
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "{\"format\": 1, \"pictures\": {\"%s\": {\"ref\": [\"000000\",\"0000AA\",\"00AA00\",\"00AAAA\","
               "\"AA0000\",\"AA00AA\",\"AA5500\",\"AAAAAA\",\"555555\",\"5555FF\",\"55FF55\",\"55FFFF\",\"FF5555\","
               "\"FF55FF\",\"FFFF55\",\"FFFFFF\"]}}, \"shapes\": {}}\n", sha1);
    fclose(f);
    for (int idx = 0; idx < 2; idx++) {
        SDL_Surface *s = SDL_CreateSurface(32, 4, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 32; x++) {
                u8 c = decode_row[x / 4];
                u32 rgb = ega_palette[c];                          /* RGBA32 in memory: R, G, B, A */
                u32 v = idx ? 0xFF000000u | c * 0x010101u
                            : 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | rgb >> 16;
                ((u32 *)((u8 *)s->pixels + y * s->pitch))[x] = v;
            }
        snprintf(path, sizeof path, "%s/pics/%s%s", dir, sha1, idx ? ".idx.png" : ".png");
        SDL_SavePNG(s, path);
        SDL_DestroySurface(s);
    }
    return rm_pack_open(dir);
}

static void remove_pack(const char *dir)
{
    char path[1024], sha1[41];
    rm_sha1_hex(decode_packed, 5, sha1);
    snprintf(path, sizeof path, "%s/pics/%s.png", dir, sha1);
    SDL_RemovePath(path);
    snprintf(path, sizeof path, "%s/pics/%s.idx.png", dir, sha1);
    SDL_RemovePath(path);
    snprintf(path, sizeof path, "%s/pics", dir);
    SDL_RemovePath(path);
    snprintf(path, sizeof path, "%s/pack.json", dir);
    SDL_RemovePath(path);
    SDL_RemovePath(dir);
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <SR2 game dir>\n", argv[0]); return 2; }
    char exe[1024], err[256];
    snprintf(exe, sizeof exe, "%s/%s", argv[1], SR_EXE_NAME);
    if (!mem_load_exe(exe, err, sizeof err)) { printf("FAIL %s\n", err); return 1; }

    run_sequence(off_planes);                               /* tags off */
    rm_tags_enable();
#define RUN(t) do { setup(); int b = failures; t(); printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)
    RUN(sr2_off_changes_nothing);
    RUN(sr2_seed_copy_latch_text);
    RUN(sr2_masked_or_sets_under);
    RUN(sr2_make_shifted_moves_tags);
    RUN(sr2_masked_blend_sets_under);
    RUN(sr2_cursor_moves_tags);
    RUN(sr2_blit_shifted_moves_tags);
    RUN(sr2_save_rect_to_ram);
    RUN(sr2_tail_holes);
    RUN(sr2_composite_behind_shows_behind);
    RUN(sr2_bitmap_clear_clears_tags);
    RUN(sr2_decode_seeds_tags);

    char dir[256];
    if (!open_pack(dir, sizeof dir)) { printf("FAIL pack\n"); return 1; }
    RUN(sr2_hash_parked_keeps_latches);
    RUN(sr2_decode_hash_in_place);
    RUN(sr2_enh_cut_keeps_verified_tags);
    remove_pack(dir);
    return failures != 0;
}
