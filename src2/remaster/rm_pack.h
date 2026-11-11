/* rm_pack.h -- ENH: Street Rod 2 has no remastered pictures yet (plan 5). These stand-ins let the enhanced
 * view's shared drawing core (common/enhanced) build: no pack is ever active, so none of the art paths run. */
#ifndef SR2_RM_PACK_H
#define SR2_RM_PACK_H

#include "types.h"

#define RM_ART_SCALE 4
typedef struct { int w, h; u8 *orig; } RmPic;

static inline bool rm_pack_active(void) { return false; }
static inline RmPic *rm_pic(int slot) { (void)slot; return NULL; }
static inline int rm_selftest(void) { return 0; }
static inline void rm_selftest_race(bool differs) { (void)differs; }
static inline u32 rm_texel(const RmPic *p, int tu, int tv, const u32 rgb[16], int *alpha)
{
    (void)p; (void)tu; (void)tv; (void)rgb;
    if (alpha) *alpha = 0;
    return 0;
}

#endif
