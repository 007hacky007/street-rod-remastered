/* test_mackeys.c -- the macOS key filter (src/host_mackeys.c). */
#include <stdio.h>

#include "host_mackeys.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

enum { LCTRL = SDL_SCANCODE_LCTRL, LGUI = SDL_SCANCODE_LGUI, F = SDL_SCANCODE_F, Q = SDL_SCANCODE_Q,
       UP = SDL_SCANCODE_UP, A = SDL_SCANCODE_A };

static MacKeyFilter f;
static MacKey out[2];

static int ev(int sc, bool down, bool gui) { return mac_keys_filter(&f, sc, down, gui, out); }

static void ctrl_tap_toggles_on_release(void)
{
    CHECK(ev(LCTRL, true, false) == 0);
    CHECK(ev(LCTRL, false, false) == 2);
    CHECK(out[0].scancode == LCTRL && out[0].down);
    CHECK(out[1].scancode == LCTRL && !out[1].down);
}

static void ctrl_then_cmd_f_sends_nothing(void)
{
    CHECK(ev(LCTRL, true, false) == 0);
    CHECK(ev(LGUI, true, true) == 0);
    CHECK(ev(F, true, true) == 0);
    CHECK(ev(F, false, true) == 0);
    CHECK(ev(LGUI, false, false) == 0);
    CHECK(ev(LCTRL, false, false) == 0);
}

static void cmd_q_not_sent(void)
{
    CHECK(ev(LGUI, true, true) == 0);
    CHECK(ev(Q, true, true) == 0);
    CHECK(ev(Q, false, true) == 0);
    CHECK(ev(LGUI, false, false) == 0);
}

static void ctrl_held_with_other_key_flushes(void)
{
    CHECK(ev(LCTRL, true, false) == 0);
    CHECK(ev(UP, true, false) == 2);
    CHECK(out[0].scancode == LCTRL && out[0].down);
    CHECK(out[1].scancode == UP && out[1].down);
    CHECK(ev(UP, false, false) == 1);
    CHECK(out[0].scancode == UP && !out[0].down);
    CHECK(ev(LCTRL, false, false) == 1);
    CHECK(out[0].scancode == LCTRL && !out[0].down);
}

static void suppressed_release_after_gui_up(void)
{
    CHECK(ev(LGUI, true, true) == 0);
    CHECK(ev(F, true, true) == 0);
    CHECK(ev(LGUI, false, false) == 0);
    CHECK(ev(F, false, false) == 0);
}

static void plain_keys_pass(void)
{
    CHECK(ev(A, true, false) == 1);
    CHECK(out[0].scancode == A && out[0].down);
    CHECK(ev(A, false, false) == 1);
    CHECK(out[0].scancode == A && !out[0].down);
}

static void reset_drops_pending_ctrl(void)
{
    CHECK(ev(LCTRL, true, false) == 0);
    mac_keys_reset(&f);
    CHECK(ev(LCTRL, false, false) == 0);
}

#define RUN(t) do { mac_keys_reset(&f); int before = failures; t(); printf("%s %s\n", failures == before ? "ok  " : "FAIL", #t); } while (0)

int main(void)
{
    RUN(ctrl_tap_toggles_on_release);
    RUN(ctrl_then_cmd_f_sends_nothing);
    RUN(cmd_q_not_sent);
    RUN(ctrl_held_with_other_key_flushes);
    RUN(suppressed_release_after_gui_up);
    RUN(plain_keys_pass);
    RUN(reset_drops_pending_ctrl);
    return failures != 0;
}
