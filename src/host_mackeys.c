/* host_mackeys.c -- ENH: the macOS key filter (host_mackeys.h). */
#include "host_mackeys.h"

#include <string.h>

static bool is_ctrl(int sc) { return sc == SDL_SCANCODE_LCTRL || sc == SDL_SCANCODE_RCTRL; }
static bool is_gui(int sc) { return sc == SDL_SCANCODE_LGUI || sc == SDL_SCANCODE_RGUI; }

void mac_keys_reset(MacKeyFilter *f) { memset(f, 0, sizeof *f); }

int mac_keys_filter(MacKeyFilter *f, int scancode, bool down, bool gui_held, MacKey out[2])
{
    if (scancode <= 0 || scancode >= SDL_SCANCODE_COUNT) return 0;
    int n = 0;
    if (down) {
        if (is_gui(scancode) || gui_held) {
            /* A Cmd shortcut: the Mac's. A Ctrl held back for it is dropped with it. */
            f->suppressed[scancode] = true;
            if (f->pending_ctrl) {
                f->dropped_ctrl = f->pending_ctrl;
                f->pending_ctrl = 0;
            }
            return 0;
        }
        if (is_ctrl(scancode)) {
            if (f->pending_ctrl == scancode || f->dropped_ctrl == scancode) return 0;  /* key repeat */
            f->pending_ctrl = scancode;
            return 0;
        }
        if (f->pending_ctrl) {          /* Ctrl with another key: not a shortcut, send it first */
            out[n++] = (MacKey){ f->pending_ctrl, true };
            f->sent_ctrl = f->pending_ctrl;
            f->pending_ctrl = 0;
        }
        out[n++] = (MacKey){ scancode, true };
        return n;
    }
    if (f->suppressed[scancode]) {
        f->suppressed[scancode] = false;
        return 0;
    }
    if (is_ctrl(scancode)) {
        if (f->dropped_ctrl == scancode) {
            f->dropped_ctrl = 0;
            return 0;
        }
        if (f->pending_ctrl == scancode) {  /* a Ctrl tap: make and break now */
            f->pending_ctrl = 0;
            out[n++] = (MacKey){ scancode, true };
        } else if (f->sent_ctrl == scancode) {
            f->sent_ctrl = 0;
        } else {
            return 0;                       /* its make code was never sent (reset meanwhile) */
        }
    }
    out[n++] = (MacKey){ scancode, false };
    return n;
}
