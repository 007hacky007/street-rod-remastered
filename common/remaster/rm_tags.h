/* rm_tags.h -- the remaster's per-pixel tags: for every pixel of video memory and of the RAM bitmaps, which
 * picture pixel it shows (slot = picture directory index + 1, u, v), plus the tag that was under it when a
 * masked picture was drawn over it (for blending the remastered edges).
 *
 * Tags follow the data like the hardware does: latch reads load latch tags, write mode 1 writes them, the
 * bits outside the bit mask keep them, armed CPU copies (rm_copy_begin) carry the tags of the last byte
 * read, and every other write clears the bits it writes. The helpers that combine or shift pixels on the
 * CPU suspend the hooks (rm_suspend) and set the tags themselves.
 *
 * Nothing is allocated and every hook returns at once while the tags are off (no --remaster-dir): the
 * game runs exactly as without them. */
#ifndef RM_TAGS_H
#define RM_TAGS_H

#include "mem.h"

typedef u32 RmTag;                       /* bit 31 valid, 18-26 slot, 9-17 u, 0-8 v; 0 = none */

static inline RmTag rm_tag(int slot, int u, int v)
{
    return 0x80000000u | (u32)(slot & 0x1FF) << 18 | (u32)(u & 0x1FF) << 9 | (u32)(v & 0x1FF);
}
static inline int rm_tag_slot(RmTag t) { return (int)(t >> 18 & 0x1FF); }
static inline int rm_tag_u(RmTag t)    { return (int)(t >> 9 & 0x1FF); }
static inline int rm_tag_v(RmTag t)    { return (int)(t & 0x1FF); }

/* A hole: a pixel the game filled with the ground colour behind a sprite (the wheel rows of a driving car
 * are built over a plain fill and copied opaque). It stands for "transparent": writing a hole leaves the
 * destination's tags, and a pixel drawn over a hole gets the destination's tag as its under. Never shown. */
#define RM_HOLE_SLOT 0x1FF
#define RM_HOLE (0x80000000u | (u32)RM_HOLE_SLOT << 18)

extern bool rm_tags_active;              /* on and not suspended: the inline hooks test this */

bool rm_tags_on(void);
void rm_tags_enable(void);
void rm_tags_reset(void);                /* all tags none (tests) */

/* Tags of pixel `bit` (7 = leftmost) of a video memory byte / a RAM byte (linear address). */
RmTag rm_vtag(u16 off, int bit);
RmTag rm_vunder(u16 off, int bit);
RmTag rm_mtag(u32 lin, int bit);
RmTag rm_munder(u32 lin, int bit);
/* The same through seg:off (video memory for A000-AFFF, RAM otherwise). */
RmTag rm_tag_at(u16 seg, u16 off, int bit);
RmTag rm_under_at(u16 seg, u16 off, int bit);
void  rm_set_at(u16 seg, u16 off, int bit, RmTag top, RmTag under);

/* Hooks of the VGA model (vga.c) and of vrd/vwr on RAM (vga.h). */
void rm_on_vram_read(u16 off);
/* A video memory write: cpu_bits = the pixels whose value comes from the CPU side (the bit mask; none in
 * write mode 1), data = the value written is the CPU byte itself (mode 0, no set/reset, no function). */
void rm_on_vram_write(u16 off, u8 cpu_bits, bool data);
void rm_on_ram_read(u32 lin);
void rm_on_ram_write(u32 lin);

/* While armed, CPU writes take the tags of the last byte read (a plain copy). */
void rm_copy_begin(void);
void rm_copy_end(void);
/* Nested: while suspended, writes leave the tags alone (the helper sets them itself). */
void rm_suspend(void);
void rm_resume(void);

/* pic_decode: the w x h bitmap at `planes` (plane k at k * stride * h) shows picture `slot`. */
void rm_seed_bitmap(FarPtr planes, int w, int h, int stride, int slot);
/* One pixel drawn opaque from a source pixel over a destination pixel: top = the source's top, under = the
 * destination's old top (kept if the destination already shows that source pixel). */
void rm_over(u16 dseg, u16 doff, int dbit, u16 sseg, u16 soff, int sbit);
/* One pixel copied as it is (top and under). */
void rm_copy_px(u16 dseg, u16 doff, int dbit, u16 sseg, u16 soff, int sbit);
void rm_clear_px(u16 seg, u16 off, int bit);
/* A byte of a masked blit (mask bit 1 = transparent): the opaque pixels as rm_over. */
void rm_masked_byte(u16 dseg, u16 doff, u16 sseg, u16 soff, u8 mask);
/* A byte of sprite-over-background (dst = (mask & bg) | src): opaque pixels top = source, under =
 * background; transparent pixels the background as it is. */
void rm_composite_byte(u16 dseg, u16 doff, u16 sseg, u16 soff, u16 bseg, u16 boff, u8 mask);
/* The n bytes at seg:off (all 8 pixels) are holes. */
void rm_holes(u16 seg, u16 off, u16 n);
/* Wheel rows copied to the screen: in rows x wb bytes at dst (row stride dstr), holes take the tags of the
 * background byte at the same place of bg (row stride bgstr), and pixels over a hole get it as under. */
void rm_tail_holes(FarPtr dst, u16 dstr, FarPtr bg, u16 bgstr, s16 wb, s16 rows);
/* The pixels set in `bits` copied as they are from s to d (same pixel positions). */
void rm_copy_bits(u16 dseg, u16 doff, u16 sseg, u16 soff, u8 bits);

#endif
