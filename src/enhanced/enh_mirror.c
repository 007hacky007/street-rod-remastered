/* Enhanced view: the dashboard's left side mirror at the output resolution (enh_internal.h, ENHANCED.md).
 * From bsvdoom's change to upstream streetrod-enhanced. */
#include "enhanced/enhanced.h"
#include "enhanced/enh_internal.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/ui.h"
#include "mem.h"

/* the last copy, kept while a dialog covers the rear-view mirror */
static u32 cache[32 * ENH_MAX_RES_SCALE * 19 * ENH_MAX_RES_SCALE];
static bool cache_valid;
static int cache_mw, cache_mh;

void enh_side_mirror_reset(void) { cache_valid = false; }

typedef struct { int x0, y0, x1, y1; } Rect;

#define UI_DEPTH 0x8B8E                /* ui_menu.c: the UI stack's depth (max 4) and its screens */
#define UI_STACK 0x8B86

/* ui_bounds (ui_menu.c) without its writes to DGROUP: the rectangles [x0, x1) x [y0, y1) of the screens on the
 * UI stack that have records (the dialogs and menus); the screens without (the "click anywhere" waits) cover
 * nothing. */
static int dialogs(Rect out[4])
{
    int n = 0, depth = DSS(UI_DEPTH);
    for (int d = 0; d < depth && d < 4; d++) {
        u16 rec = ui_screen_rec(DSS((u16)(UI_STACK + 2 * d)));
        int x0 = 1000, y0 = 1000, x1 = 1, y1 = 1;
        for (int guard = 0; DSW((u16)(rec + UI_R_NEXT)) != 0 && guard < 256; guard++) {
            rec = UI_REC(DSW((u16)(rec + UI_R_NEXT)));
            x0 = SDL_min(x0, DSS((u16)(rec + UI_R_X0)));
            x1 = SDL_max(x1, DSS((u16)(rec + UI_R_X1)));
            y0 = SDL_min(y0, DSS((u16)(rec + UI_R_Y0)));
            y1 = SDL_max(y1, DSS((u16)(rec + UI_R_Y1)));
        }
        x0 = SDL_max(x0 - 3, 0);
        y0 = SDL_max(y0 - 3, 0);
        x1 = SDL_min(x1 + 3, 320);
        y1 = SDL_min(y1 + 3, 200);
        if (x1 > x0 && y1 > y0) out[n++] = (Rect){ x0, y0, x1, y1 };
    }
    return n;
}

static bool overlaps(const Rect *r, int n, int x0, int y0, int x1, int y1)
{
    for (int i = 0; i < n; i++)
        if (r[i].x0 < x1 && r[i].x1 > x0 && r[i].y0 < y1 && r[i].y1 > y0) return true;
    return false;
}

void enh_side_mirror(u32 *xrgb, int res_scale)
{
    int W = 320 * res_scale;
    Rect dlg[4];
    int n = dialogs(dlg);
    if (overlaps(dlg, n, 8, 103, 40, 122)) return;                   /* a dialog over the side mirror */
    int sx0 = 248 * res_scale, sy0 = 22 * res_scale, dx0 = 8 * res_scale, dy0 = 103 * res_scale;
    int mw = 32 * res_scale, mh = 19 * res_scale;
    if (!overlaps(dlg, n, 248, 22, 280, 41)) {                       /* the part the side mirror shows */
        for (int y = 0; y < mh; y++) {
            memcpy(cache + y * mw, xrgb + (size_t)(sy0 + y) * W + sx0, (size_t)mw * sizeof(u32));
            memcpy(xrgb + (size_t)(dy0 + y) * W + dx0, xrgb + (size_t)(sy0 + y) * W + sx0, (size_t)mw * sizeof(u32));
        }
        cache_valid = true;
        cache_mw = mw;
        cache_mh = mh;
    } else if (cache_valid && cache_mw == mw && cache_mh == mh) {
        for (int y = 0; y < mh; y++)
            memcpy(xrgb + (size_t)(dy0 + y) * W + dx0, cache + y * mw, (size_t)mw * sizeof(u32));
    }
}
