/* rm_lib.c -- see rm_lib.h. */
#include "remaster/rm_lib.h"

#include <stdlib.h>

#include "platform/ega.h"
#include "platform/vga.h"
#include "remaster/rm_dump.h"
#include "remaster/rm_pack.h"

static char keys[RM_MAX_SLOTS][41];      /* "" = not hashed */

void rm2_pic_hash(int slot, FarPtr packed, u16 psize, u8 plane)
{
    if (!rm_pack_active() || slot <= 0 || slot >= RM_MAX_SLOTS || keys[slot][0]) return;
    u8 *buf = malloc(psize ? psize : 1);
    if (!buf) return;
    for (u16 i = 0; i < psize; i++) {
        u16 off = (u16)(packed.off + i);
        /* the parked plane read directly: vga_rd would load the latches (and their tags) */
        buf[i] = plane != 0xFF ? ega_plane(plane & 3)[vram_off(packed.seg, off)] : rd8(packed.seg, off);
    }
    rm_sha1_hex(buf, psize, keys[slot]);
    free(buf);
}

void rm2_pic_register(int slot, FarPtr planes, int w, int h, int stride)
{
    if (!rm_pack_active() || slot <= 0 || slot >= RM_MAX_SLOTS || !keys[slot][0]) return;
    if (w <= 0 || h <= 0 || w > 640 || h > 400) return;
    u8 *orig = malloc((size_t)w * h);
    if (!orig) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = 0;
            for (int k = 0; k < 4; k++)
                c |= (rd8(planes.seg, (u16)(planes.off + k * stride * h + y * stride + (x >> 3))) >> (7 - (x & 7)) & 1) << k;
            orig[y * w + x] = (u8)c;
        }
    rm_slot_register_raw(slot, keys[slot], orig, w, h);       /* once: it keeps the first registration */
    free(orig);
}

const char *rm2_slot_sha1(int slot)
{
    return slot > 0 && slot < RM_MAX_SLOTS && keys[slot][0] ? keys[slot] : NULL;
}
