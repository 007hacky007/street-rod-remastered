/* test_color.c -- the remaster's palette-relative colour (src/remaster/rm_color.c). */
#include <stdio.h>

#include "remaster/rm_color.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static int lum(u32 c) { return (int)(((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114); }

static void shade_same_is_identity(void)
{
    CHECK(rm_shade(0x123456, 0x0000AA, 0x0000AA) == 0x123456);
    CHECK(rm_shade(0x0000AA, 0x0000AA, 0xAA0000) == 0xAA0000);   /* the reference colour itself */
}

static void shade_black_is_black(void)
{
    CHECK(rm_shade(0x3344CC, 0x0000AA, 0x000000) == 0x000000);
}

static void shade_keeps_highlight_order(void)
{
    u32 dark = rm_shade(0x000066, 0x0000AA, 0xAA0000), light = rm_shade(0x6666FF, 0x0000AA, 0xAA0000);
    CHECK(lum(dark) < lum(0xAA0000) && lum(0xAA0000) < lum(light));
    CHECK((dark >> 16 & 255) > (dark & 255));                    /* red, not blue */
}

int main(void)
{
#define RUN(t) do { int b = failures; t(); printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)
    RUN(shade_same_is_identity);
    RUN(shade_black_is_black);
    RUN(shade_keeps_highlight_order);
    return failures != 0;
}
