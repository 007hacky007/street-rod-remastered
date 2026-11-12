/* rm_color.c -- see rm_color.h. */
#include "remaster/rm_color.h"

static double lum(u32 c) { return 0.299 * (c >> 16 & 255) + 0.587 * (c >> 8 & 255) + 0.114 * (c & 255); }

u32 rm_shade(u32 texel, u32 ref, u32 cur)
{
    texel &= 0xFFFFFF; ref &= 0xFFFFFF; cur &= 0xFFFFFF;
    if (cur == ref) return texel;
    double k = (lum(texel) + 1.0) / (lum(ref) + 1.0);
    /* Highlights fade with the colour: none on black (the game's fades), full when cur is as bright as ref. */
    double hl = (lum(cur) + 1.0) / (lum(ref) + 1.0);
    if (hl > 1.0) hl = 1.0;
    if (cur == 0) hl = 0.0;
    u32 out = 0;
    for (int sh = 16; sh >= 0; sh -= 8) {
        double c = cur >> sh & 255, v = k <= 1.0 ? c * k : c + (255.0 - c) * (1.0 - 1.0 / k) * hl;
        int iv = (int)(v + 0.5);
        out |= (u32)(iv < 0 ? 0 : iv > 255 ? 255 : iv) << sh;
    }
    return out;
}
