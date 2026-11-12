/* rm_pack.h -- the remaster art pack (design: docs/superpowers/specs/2026-11-10-hd-remaster-design.md).
 *
 * <dir>/pack.json  {"format": 1, "pictures": {"<sha1>": {"ref": [16 x "RRGGBB"]}}, "shapes": {...}}
 * <dir>/pics/<sha1>.png      RGBA, exactly 4x the original picture
 * <dir>/pics/<sha1>.idx.png  8-bit: the original palette index each texel belongs to
 *
 * sha1 = SHA-1 of the picture's packed bytes in the library (from the leading BCh, PE_SIZE bytes). A picture
 * without art keeps its original pixels.
 *
 * Developer aids (environment):
 *   SR_REMASTER_SELFTEST=1      count the replaced pixels and those whose colour differs from the original
 *                               (an identity pack must give 0); a line on stderr every 200 frames and at exit;
 *                               =2 also paints the replaced pixels in a colour per picture (coverage)
 *   SR_REMASTER_PALETTE_LOG=F   append to F, once per picture, the palette it is first shown with
 *   SR_REMASTER_TAGMAP=D        every SR_REMASTER_TAGMAP_N-th composed frame (default 25) as D/frameNNNN.bmp and
 *                               its tags as D/tagsNNNN.bmp (320 x 200: R + 256 G = top slot, B = under slot) */
#ifndef RM_PACK_H
#define RM_PACK_H

#include "mem.h"

#define RM_ART_SCALE 4
#define RM_MAX_SLOTS 512
#define RM_SHAPE_SLOT0 400       /* race-view shape t has slot RM_SHAPE_SLOT0 + t */

typedef struct {
    int w, h;                    /* the original picture */
    u8 *orig;                    /* w * h original indices */
    char sha1[41];
    int art;                     /* 0 not tried, 1 loaded, -1 none */
    u8 *rgba;                    /* 4w x 4h RGBA */
    u8 *idx;                     /* 4w x 4h indices */
    u32 ref[16];
    u32 pal_hash, pal_logged;  /* SR_REMASTER_PALETTE_LOG: the palette seen, its frames, the one logged */
    int pal_stable;
    unsigned long pal_frame;
} RmPic;

bool rm_pack_open(const char *dir);
bool rm_pack_active(void);
/* pic_decode: the packed bytes (from the leading BCh) and the decoded planes of picture `slot`. */
void rm_slot_register(int slot, FarPtr packed, u16 psize, FarPtr planes, int w, int h, int stride);
/* A picture registered by its key and w x h original indices: one that does not come from a library (the
 * race-view shapes), or one of Street Rod 2's library pictures, whose packed bytes are gone after its in-place
 * unpack (src2/remaster/rm_lib.c). */
void rm_slot_register_raw(int slot, const char sha1[41], const u8 *orig, int w, int h);
/* The registered picture of `slot` with its art loaded, or NULL. */
RmPic *rm_pic(int slot);
/* compose (ega.c): replaces verified tagged pixels of the S x S output frame. */
void rm_compose(u32 *xrgb, int scale, const u32 rgb[16], u16 start, int line_compare, u8 *const planes[4]);
void rm_pack_report(void);       /* the self-test line (at exit) */
int  rm_selftest(void);          /* SR_REMASTER_SELFTEST */
void rm_selftest_race(bool differs);   /* a race-view sample drawn from the art */
/* Art texel (tu, tv) (4x coordinates) through the palette-relative colour; *alpha its alpha. */
u32  rm_texel(const RmPic *p, int tu, int tv, const u32 rgb[16], int *alpha);

#endif
