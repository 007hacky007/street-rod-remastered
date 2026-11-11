/* test_sr2_enh.c -- Street Rod 2's enhanced race view (src2/enhanced, src2/platform ENH parts): the output
 * scale, the coverage layers, the options.
 *   test_sr2_enh <SR2 game dir>
 * Headless with the SDL dummy drivers. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "enhanced/enhanced.h"
#include "enhanced/enh_internal.h"
#include "host.h"
#include "mem.h"
#include "platform/ega.h"
#include "platform/vga.h"

static int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
            failures++;                                                                \
        }                                                                              \
    } while (0)
#define RUN(t) do { int b = failures; t(); printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)

static u32 small[320 * 200], big[1280 * 800];

/* A pattern in all four planes: every pixel's colour depends on its place. */
static void pattern(void)
{
    for (int k = 0; k < 4; k++)
        for (int o = 0; o < 200 * EGA_BYTES_PER_LINE; o++) ega_plane(k)[o] = (u8)(o * (k + 3) + k * 0x55);
}

static void sr2_enh_scale(void)
{
    enh_init(true, 1, 2, 100, true);
    ega_init();
    pattern();
    ega_show();
    CHECK(ega_compose(small));
    enh_init(true, 4, 2, 100, true);
    ega_show();                                     /* the output scale is taken here */
    CHECK(ega_compose(big));
    int bad = 0;
    for (int y = 0; y < 800 && !bad; y++)
        for (int x = 0; x < 1280; x++)
            if (big[y * 1280 + x] != small[(y / 4) * 320 + x / 4]) { bad = 1; printf("  differs at %d,%d\n", x, y); break; }
    CHECK(!bad);
}

static void sr2_enh_layers(void)
{
    enh_init(true, 1, 2, 100, true);
    ega_init();
    vga_mode_0d();
    vga_out(0x3C4, 2); vga_out(0x3C5, 0x0F);       /* map mask: all planes */
    vga_out(0x3CE, 8); vga_out(0x3CF, 0xFF);       /* bit mask: all pixels */
    ega_clear_written(0, 0x2000);
    ega_clear_fg(0, 0x2000);
    ega_set_layer(EGA_LAYER_BG);
    vga_wr(10, 0xAA);
    CHECK(ega_written()[10] == 0xFF);
    CHECK(ega_fg()[10] == 0);
    ega_set_layer(EGA_LAYER_FG);
    vga_wr(11, 0x0F);
    CHECK(ega_written()[11] == 0xFF);
    CHECK(ega_fg()[11] == 0x0F);                   /* only the pixels whose colour changed */
    ega_set_layer(EGA_LAYER_BG);
    vga_wr(11, 0x00);                               /* the background covers the foreground again */
    CHECK(ega_fg()[11] == 0);
    ega_set_layer(EGA_LAYER_OFF);
    ega_clear_written(0, 0x2000);
    CHECK(ega_written()[10] == 0 && ega_written()[11] == 0);
}

/* As Street Rod 1: out-of-range values are clamped; res scale x aa stays within 16 samples. */
static void sr2_enh_options(void)
{
    enh_init(true, 9, 0, 150, true);
    CHECK(enh_opt.res_scale == 8 && enh_opt.aa == 1 && enh_opt.motion_delay == 100);
    enh_init(true, 8, 4, -5, false);
    CHECK(enh_opt.res_scale == 8 && enh_opt.aa == 2 && enh_opt.motion_delay == 0 && !enh_opt.town);
    enh_init(false, 1, 2, 100, true);
    CHECK(!enh_enabled() && enh_res_scale() == 1);
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <SR2 game dir>\n", argv[0]); return 2; }
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    char exe[1024], err[256];
    SDL_snprintf(exe, sizeof exe, "%s/%s", argv[1], SR_EXE_NAME);
    if (!mem_load_exe(exe, err, sizeof err)) { printf("FAIL %s\n", err); return 1; }
    host_configure(&(HostConfig){ "test", 49716, true });
    if (!host_init(argv[1], 1, false)) { printf("FAIL host_init\n"); return 1; }
    RUN(sr2_enh_scale);
    RUN(sr2_enh_layers);
    RUN(sr2_enh_options);
    host_shutdown();
    return failures ? 1 : 0;
}
