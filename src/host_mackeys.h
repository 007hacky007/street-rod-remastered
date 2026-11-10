/* host_mackeys.h -- ENH: the macOS key filter. Cmd shortcuts (Cmd+Q, Ctrl+Cmd+F, Cmd+Tab ...) belong to the
 * Mac and must not reach the game; and the game toggles the sound on a Ctrl make code, so a Ctrl press is
 * held back until it is clear it isn't part of a Cmd shortcut. Uses SDL's scancodes only, no SDL calls. */
#pragma once

#include <stdbool.h>
#include <SDL3/SDL_scancode.h>

typedef struct { int scancode; bool down; } MacKey;

typedef struct {
    int pending_ctrl;   /* SDL_SCANCODE_LCTRL/RCTRL held but not sent yet, 0 if none */
    int dropped_ctrl;   /* a Ctrl whose make code was dropped (used in a Cmd shortcut), 0 if none */
    int sent_ctrl;      /* a Ctrl whose make code was sent and whose break is still due, 0 if none */
    bool suppressed[SDL_SCANCODE_COUNT];   /* keys whose make code was not sent, so neither is the break */
} MacKeyFilter;

void mac_keys_reset(MacKeyFilter *f);

/* One key event in; returns how many events (0..2) to feed to the game, in order, in out[]. */
int mac_keys_filter(MacKeyFilter *f, int scancode, bool down, bool gui_held, MacKey out[2]);
