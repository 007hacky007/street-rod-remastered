/* test_side_mirror.c -- Street Rod 1's enhanced view: the dashboard's left side mirror (8, 103) 32 x 19 shows the
 * enhanced rear-view mirror (248, 22) at the output resolution (enh_side_mirror), keeps its last copy while a
 * dialog covers the rear-view mirror, and is left alone under a dialog over itself. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "enhanced/enh_internal.h"
#include "enhanced/enhanced.h"
#include "game/ui.h"
#include "mem.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define S 2
#define W (320 * S)
static u32 xrgb[320 * S * 200 * S];

static void fill_mirror(u32 v)
{
    for (int y = 22 * S; y < 41 * S; y++)
        for (int x = 248 * S; x < 280 * S; x++) xrgb[y * W + x] = v + (u32)(y * W + x);
}

static bool side_is(u32 v)       /* the side mirror holds the rear-view mirror filled with v */
{
    for (int y = 0; y < 19 * S; y++)
        for (int x = 0; x < 32 * S; x++)
            if (xrgb[(103 * S + y) * W + 8 * S + x] != v + (u32)((22 * S + y) * W + 248 * S + x)) return false;
    return true;
}

static bool side_untouched(void)
{
    for (int y = 103 * S; y < 122 * S; y++)
        for (int x = 8 * S; x < 40 * S; x++)
            if (xrgb[y * W + x] != 0x123456) return false;
    return true;
}

static void clear_side(void)
{
    for (int y = 103 * S; y < 122 * S; y++)
        for (int x = 8 * S; x < 40 * S; x++) xrgb[y * W + x] = 0x123456;
}

/* one dialog on the UI stack (screen 0): a record [x0, x1) x [y0, y1) */
static void dialog(int x0, int y0, int x1, int y1)
{
    DSS(0x8B8E) = 1;                                     /* the stack's depth */
    DSS(0x8B86) = 0;                                     /* its screen 0 */
    DSS(UI_SCREENS) = 0;                                 /* screen 0's first record: 0, the head */
    DSW(UI_REC(0) + UI_R_NEXT) = 1;
    DSS(UI_REC(1) + UI_R_X0) = (s16)x0; DSS(UI_REC(1) + UI_R_Y0) = (s16)y0;
    DSS(UI_REC(1) + UI_R_X1) = (s16)x1; DSS(UI_REC(1) + UI_R_Y1) = (s16)y1;
    DSW(UI_REC(1) + UI_R_NEXT) = 0;
}

int main(void)
{
    DSS(0x8B8E) = 0;
    enh_side_mirror_reset();
    clear_side();
    fill_mirror(1000);
    enh_side_mirror(xrgb, S);
    CHECK(side_is(1000));                                /* no dialog: the copy */

    dialog(200, 10, 300, 60);                            /* over the rear-view mirror */
    fill_mirror(5000);
    clear_side();
    enh_side_mirror(xrgb, S);
    CHECK(side_is(1000));                                /* the last copy, not the dialog */

    dialog(0, 90, 60, 130);                              /* over the side mirror */
    clear_side();
    enh_side_mirror(xrgb, S);
    CHECK(side_untouched());

    dialog(200, 10, 300, 60);
    enh_side_mirror_reset();                             /* a new road view: no last copy */
    clear_side();
    enh_side_mirror(xrgb, S);
    CHECK(side_untouched());

    DSS(0x8B8E) = 0;
    enh_side_mirror(xrgb, S);
    CHECK(side_is(5000));
    printf("%s side_mirror\n", failures ? "FAIL" : "ok  ");
    return failures != 0;
}
