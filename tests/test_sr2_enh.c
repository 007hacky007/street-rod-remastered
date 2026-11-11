/* test_sr2_enh.c -- Street Rod 2's enhanced race view (src2/enhanced, src2/platform ENH parts): the output
 * scale, the coverage layers, the options.
 *   test_sr2_enh <SR2 game dir>
 * Headless with the SDL dummy drivers. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "enhanced/enhanced.h"
#include "enhanced/enh_compare.h"
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

/* Rows of a road: colour 8 between left and right (inclusive), 2 elsewhere. */
static void road_row(u8 *row, int left, int right)
{
    for (int x = 0; x < 320; x++) row[x] = x >= left && x <= right ? 8 : 2;
}

static void sr2_enh_compare_rows(void)
{
    static u8 orig[10 * 320], enh[10 * 320], cov[10 * 320];
    const u8 road[] = { 8 };
    EnhCompare c;
    for (int y = 0; y < 10; y++) { road_row(orig + y * 320, 100, 200); road_row(enh + y * 320, 100, 200); }
    memset(cov, 1, sizeof cov);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.rows == 10 && c.edge_rows == 10 && c.edge_err_max == 0 && c.mismatched == 0 && c.pixels == 3200);

    road_row(enh + 3 * 320, 101, 201);              /* one row 1 px off */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1 && c.mismatched == 2);

    road_row(enh + 5 * 320, 97, 200);               /* another 3 px off on the left */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 3 && c.mismatched == 5);

    memset(cov + 5 * 320, 0, 320);                  /* that row not covered: not counted */
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1 && c.mismatched == 2 && c.pixels == 2880);

    memset(cov + 6 * 320 + 95, 0, 5);               /* the left edge of row 6 runs under the foreground */
    road_row(enh + 6 * 320, 98, 200);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1);                     /* row 6's left edge is not measured; its right edge is */
    road_row(enh + 6 * 320, 100, 200);
    memset(cov + 6 * 320 + 95, 1, 5);
    memset(cov + 6 * 320 + 101, 0, 8);              /* hidden between the two edges: the enhanced edge is not seen */
    road_row(enh + 6 * 320, 110, 200);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_err_max == 1);
    road_row(enh + 6 * 320, 100, 200);
    memset(cov + 6 * 320 + 101, 1, 8);

    memset(orig + 7 * 320, 2, 320);                 /* no road in the original's row: no edge there */
    memset(enh + 7 * 320, 2, 320);
    enh_compare_rows(orig, enh, cov, 0, 10, road, 1, &c);
    CHECK(c.edge_rows == 8 && c.edge_err_max == 1);
}

static void sr2_enh_compare_log(void)
{
    char path[1024];
    SDL_snprintf(path, sizeof path, "%stest_sr2_enh_compare.log", SDL_GetBasePath());
    SDL_setenv_unsafe("SR_ENH_COMPARE", path, 1);
    CHECK(enh_compare_open());
    EnhCompare a = { 84, 80, 1, 26880, 27 }, b = { 84, 82, 0, 26880, 54 };
    enh_compare_frame(1, &a);
    enh_compare_frame(1, &b);
    enh_compare_close();
    SDL_unsetenv_unsafe("SR_ENH_COMPARE");
    size_t len = 0;
    char *text = SDL_LoadFile(path, &len);
    CHECK(text != NULL);
    if (!text) return;
    printf("%s", text);
    CHECK(strstr(text, "frame 1 mode 1 rows 84 edge_rows 80 edge_err_max 1 mismatch 1\n") != NULL);
    CHECK(strstr(text, "frame 2 mode 1 rows 84 edge_rows 82 edge_err_max 0 mismatch 2\n") != NULL);
    CHECK(strstr(text, "frames 2 worst_edge 1 mismatch_p50 1 mismatch_max 2\n") != NULL);
    SDL_free(text);
    SDL_RemovePath(path);
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
    RUN(sr2_enh_compare_rows);
    RUN(sr2_enh_compare_log);
    host_shutdown();
    return failures ? 1 : 0;
}
