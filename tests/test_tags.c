/* test_tags.c -- the remaster's per-pixel tags (src/remaster/rm_tags.c) through the VGA model and the
 * blit helpers. */
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "symbols.h"
#include "platform/ega.h"
#include "platform/platform.h"
#include "platform/vga.h"
#include "platform/video.h"
#include "remaster/rm_tags.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define RAM 0x8000          /* scratch segment for bitmaps */

static void setup(void)
{
    memset(mem, 0, MEM_SIZE);
    vga_mode_0d();
    DSW(DS_g_nplanes) = 4;
    DSW(DS_g_px_shift) = 3;
    DSW(DS_g_px_factor) = 1;
    static const u8 lmask[9] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF };
    for (int i = 0; i < 9; i++) DSB(DS_g_lmask + i) = lmask[i];
    if (rm_tags_on()) rm_tags_reset();
}

/* A 16 x 2 bitmap at RAM:0 (stride 2, plane k at 4k), plane bits making colour (x & 15). */
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

static void seed_then_ram_to_vram_copy(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
    CHECK(rm_vtag(0, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(1, 0) == rm_tag(5, 15, 0));
    CHECK(rm_vtag(0x28, 7) == rm_tag(5, 0, 1));
    CHECK(rm_vtag(2, 7) == 0);
}

static void latch_copy_moves_tags(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
    vga_out(0x3CE, 5); vga_out(0x3CF, 1);                    /* write mode 1 */
    vmovedata(0xA000, 0, 0xA000, 100, 2);
    vga_out(0x3CE, 5); vga_out(0x3CF, 0);
    CHECK(rm_vtag(100, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(101, 0) == rm_tag(5, 15, 0));
}

static void text_write_clears(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
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

static void set_reset_fill_clears(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
    vga_out(0x3CE, 0); vga_out(0x3CF, 4);                    /* set/reset colour 4 */
    vga_out(0x3CE, 1); vga_out(0x3CF, 0x0F);                 /* enabled on all planes */
    vga_rd(0x28);
    vga_wr(0x28, 0xFF);
    vga_out(0x3CE, 1); vga_out(0x3CF, 0);
    for (int b = 0; b < 8; b++) CHECK(rm_vtag(0x28, b) == 0);
}

/* An 8 x 1 sprite at RAM2 (planes 0..3, mask at 4: 0Fh = the right 4 pixels transparent) over a picture
 * already on the screen. */
#define RAM2 0x8100
static void masked_or_sets_under(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
    for (int k = 0; k < 4; k++) mem[lin(RAM2, (u16)k)] = 0xF0;    /* colour 15 on the left half */
    mem[lin(RAM2, 4)] = 0x0F;
    rm_seed_bitmap(far_make(RAM2, 0), 8, 1, 1, 7);
    vga_out(0x3CE, 4); vga_out(0x3C4, 2);
    for (int k = 0; k < 4; k++) {
        vga_out(0x3CF, (u8)k);
        vga_out(0x3C5, (u8)(1 << k));
        rows_mask_or(far_make(0xA000, 0), RAM2, (u16)k, 4, 1, 1, 1, 0x28);
    }
    vga_out(0x3C5, 0x0F);
    CHECK(rm_vtag(0, 7) == rm_tag(7, 0, 0));
    CHECK(rm_vunder(0, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(0, 4) == rm_tag(7, 3, 0));
    CHECK(rm_vtag(0, 3) == rm_tag(5, 4, 0));
    CHECK(rm_vunder(0, 3) == 0);
}

/* bitmap_make_shifted by 3: the tag of source pixel u lands on destination pixel u + 3. */
static void shift_moves_tags(void)
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

/* A hole (the ground-colour fill behind a car's wheel rows) written through the latches leaves the tags
 * that were there: it stands for "transparent". */
static void hole_copy_keeps_destination(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);
    copy_bitmap16_to_vram();
    rm_holes(0xA000, 200, 1);
    rm_set_at(0xA000, 200, 0, rm_tag(7, 1, 1), RM_HOLE);     /* one car pixel over the fill */
    vga_out(0x3CE, 5); vga_out(0x3CF, 1);
    vmovedata(0xA000, 200, 0xA000, 0, 1);
    vga_out(0x3CE, 5); vga_out(0x3CF, 0);
    CHECK(rm_vtag(0, 7) == rm_tag(5, 0, 0));
    CHECK(rm_vtag(0, 1) == rm_tag(5, 6, 0));
    CHECK(rm_vtag(0, 0) == rm_tag(7, 1, 1));
    CHECK(rm_vunder(0, 0) == rm_tag(5, 7, 0));
}

/* After the wheel rows are copied to the page, their holes show the background the sprite is drawn over,
 * and car pixels over a hole blend with that background. */
static void tail_holes_take_background(void)
{
    make_bitmap16();
    rm_seed_bitmap(far_make(RAM, 0), 16, 2, 2, 5);           /* the background bitmap in RAM */
    rm_holes(0xA000, 0x50, 2);
    rm_set_at(0xA000, 0x51, 0, rm_tag(7, 1, 1), RM_HOLE);
    rm_tail_holes(far_make(0xA000, 0x50), 0x28, far_make(RAM, 2), 2, 2, 1);
    CHECK(rm_vtag(0x50, 7) == rm_tag(5, 0, 1));
    CHECK(rm_vunder(0x50, 7) == 0);
    CHECK(rm_vtag(0x51, 1) == rm_tag(5, 14, 1));
    CHECK(rm_vtag(0x51, 0) == rm_tag(7, 1, 1));
    CHECK(rm_vunder(0x51, 0) == rm_tag(5, 15, 1));
}

/* The same sequence with the tags off and on leaves the same pixels. */
static u8 run_sequence_planes[4][0x80];
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

int main(void)
{
    run_sequence(run_sequence_planes);                      /* tags off */
    rm_tags_enable();
    u8 on[4][0x80];
    run_sequence(on);
    int before = failures;
    CHECK(memcmp(on, run_sequence_planes, sizeof on) == 0);
    printf("%s off_changes_nothing\n", failures == before ? "ok  " : "FAIL");

#define RUN(t) do { setup(); int b = failures; t(); printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)
    RUN(seed_then_ram_to_vram_copy);
    RUN(latch_copy_moves_tags);
    RUN(text_write_clears);
    RUN(set_reset_fill_clears);
    RUN(masked_or_sets_under);
    RUN(shift_moves_tags);
    RUN(hole_copy_keeps_destination);
    RUN(tail_holes_take_background);
    return failures != 0;
}
