/* rm_lib.h -- ENH: Street Rod 2's library pictures for the remaster's art pack (common/remaster/rm_pack.h).
 *
 * SR2's pic_decode reads a picture's packed bytes into the tail of the destination bitmap and unpacks them
 * over themselves, so the key (the SHA-1 of the packed bytes, from the leading BCh) is taken before the
 * unpack and the picture registered after it. Slot = pic_index(id) + 1: LIB1 1..4, LIB2 5..216, LIB3
 * 217..299. All no-ops unless rm_pack_active(); slots 1..RM_MAX_SLOTS - 1. */
#ifndef RM_LIB_H
#define RM_LIB_H

#include "mem.h"

/* The SHA-1 of the psize packed bytes at `packed`, once per slot. plane != FFh: the picture is parked in
 * VRAM plane `plane` and is read from that plane directly (no latch or tag side effects); else from RAM. */
void rm2_pic_hash(int slot, FarPtr packed, u16 psize, u8 plane);
/* After the unpack: the w x h original indices of the 4 planes at `planes` (plane k at k * stride * h),
 * registered under the key taken. Skipped when the slot was never hashed or w, h are outside 1..640 x 1..400. */
void rm2_pic_register(int slot, FarPtr planes, int w, int h, int stride);
/* The key taken for `slot`, NULL if none (tests). */
const char *rm2_slot_sha1(int slot);

#endif
