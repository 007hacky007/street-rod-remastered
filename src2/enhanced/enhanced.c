/* Street Rod 2's enhanced road view (enhanced.h): the options, the hooks and the composition. */
#include "enhanced/enhanced.h"
#include "enhanced/enh_internal.h"

#include <SDL3/SDL.h>

EnhOptions enh_opt = { false, 1, ENH_DEFAULT_AA, ENH_DEFAULT_MOTION_DELAY, true };

/* As Street Rod 1's (src/enhanced/enhanced.c): out-of-range values are clamped, not refused. */
void enh_init(bool enabled, int res_scale, int aa, int motion_delay, bool town)
{
    enh_opt.enabled = enabled;
    enh_opt.res_scale = SDL_clamp(res_scale, 1, ENH_MAX_RES_SCALE);
    enh_opt.aa = SDL_clamp(aa, 1, ENH_MAX_AA);
    while (enh_opt.res_scale * enh_opt.aa > ENH_MAX_SAMPLES) enh_opt.aa--;
    enh_opt.motion_delay = SDL_clamp(motion_delay, 0, 100);
    enh_opt.town = town;
}

bool enh_enabled(void) { return enh_opt.enabled; }
int  enh_res_scale(void) { return enh_opt.res_scale; }

/* The race hooks; the road is drawn from part A's Task 4 on. */
void enh_race_begin(void) {}
void enh_race_end(void) {}
void enh_frame_built(void) {}
void enh_phys_step(void) {}
void enh_draw_begin(void) {}
void enh_draw_fg(bool fg) { (void)fg; }
void enh_frame_presented(void) {}

bool enh_compose(u32 *xrgb, const u32 rgb[16])
{
    (void)xrgb; (void)rgb;
    return false;
}
