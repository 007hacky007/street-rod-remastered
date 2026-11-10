/* rm_color.h -- the remaster's palette-relative colour. */
#ifndef RM_COLOR_H
#define RM_COLOR_H

#include "types.h"

/* A remastered texel drawn for palette index i: ref = the colour i had when the art was drawn, cur = the
 * colour i has now (0xRRGGBB). Same colour: the texel as drawn. Otherwise the texel's shading (its
 * brightness against ref) applied to cur: darker texels scale cur down, brighter ones blend it toward
 * white, less so the darker cur is than ref (none on black). */
u32 rm_shade(u32 texel, u32 ref, u32 cur);

#endif
