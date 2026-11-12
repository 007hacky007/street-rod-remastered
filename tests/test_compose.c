/* test_compose.c -- rm_compose: a sprite pixel whose art is transparent shows the background picture's art,
 * found from a neighbour when the tags have no usable "under" (a car driving off at Gus'). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "platform/ega.h"
#include "platform/vga.h"
#include "remaster/rm_pack.h"
#include "remaster/rm_tags.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define GROUND "1111111111111111111111111111111111111111"
#define SPRITE "2222222222222222222222222222222222222222"
#define PART   "3333333333333333333333333333333333333333"
#define WHEEL  "4444444444444444444444444444444444444444"
#define DARK   "5555555555555555555555555555555555555555"
#define W 320

static void save_png(const char *dir, const char *sha1, int w, int h, u32 abgr, bool idx)
{
    SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) ((u32 *)((u8 *)s->pixels + y * s->pitch))[x] = abgr;
    char path[1024];
    snprintf(path, sizeof path, "%s/pics/%s%s", dir, sha1, idx ? ".idx.png" : ".png");
    SDL_SavePNG(s, path);
    SDL_DestroySurface(s);
}

int main(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "sre-compose-%d", (int)SDL_GetTicks());      /* in the working directory */
    SDL_CreateDirectory(dir);
    char path[1024];
    snprintf(path, sizeof path, "%s/pics", dir);
    SDL_CreateDirectory(path);
    snprintf(path, sizeof path, "%s/pack.json", dir);
    FILE *f = fopen(path, "w");
    fprintf(f, "{\"format\": 1, \"pictures\": {");
    const char *pal = "[\"000000\",\"0000AA\",\"00AA00\",\"00AAAA\",\"AA0000\",\"AA00AA\",\"AA5500\",\"AAAAAA\","
                      "\"555555\",\"5555FF\",\"55FF55\",\"55FFFF\",\"FF5555\",\"FF55FF\",\"FFFF55\",\"FFFFFF\"]";
    fprintf(f, "\"%s\": {\"ref\": %s}, \"%s\": {\"ref\": %s}, \"%s\": {\"ref\": %s}}, \"shapes\": {}}", GROUND, pal,
            SPRITE, pal, PART, pal);
    fseek(f, -2 - (long)strlen(", \"shapes\": {}}"), SEEK_END);
    fprintf(f, ", \"%s\": {\"ref\": %s}, \"%s\": {\"ref\": %s}}, \"shapes\": {}}", WHEEL, pal, DARK, pal);
    fclose(f);
    /* the ground: index 7 (grey) drawn as a textured dark grey 0x404040 */
    save_png(dir, GROUND, W * 4, 4, 0xFF404040u, false);
    save_png(dir, GROUND, W * 4, 4, 0xFF070707u, true);
    /* the sprite: its pixels fully transparent in the art */
    save_png(dir, SPRITE, 8 * 4, 4, 0x00000000u, false);
    save_png(dir, SPRITE, 8 * 4, 4, 0xFF070707u, true);
    save_png(dir, PART, 8 * 4, 4, 0x00000000u, false);      /* another part, transparent there too */
    save_png(dir, PART, 8 * 4, 4, 0xFF070707u, true);
    save_png(dir, WHEEL, 4 * 4, 4, 0xFFFFFFFFu, false);      /* an opaque sprite next to it */
    save_png(dir, WHEEL, 4 * 4, 4, 0xFF070707u, true);
    save_png(dir, DARK, 4 * 4, 4, 0xFF303030u, false);       /* a dark grey drawn on the black index */
    save_png(dir, DARK, 4 * 4, 4, 0xFF000000u, true);

    memset(mem, 0, MEM_SIZE);
    vga_mode_0d();
    if (!rm_pack_open(dir)) { printf("FAIL pack\n"); return 1; }
    u8 orig[W];
    memset(orig, 7, sizeof orig);
    rm_slot_register_raw(1, GROUND, orig, W, 1);
    rm_slot_register_raw(2, SPRITE, orig, 8, 1);
    rm_slot_register_raw(3, PART, orig, 8, 1);
    u8 white[4] = { 15, 15, 15, 15 };
    rm_slot_register_raw(4, WHEEL, white, 4, 1);
    /* screen row 0: all grey (index 7); pixels 0-15 the ground, 16-23 the sprite with no under */
    for (int k = 0; k < 3; k++) memset(ega_plane(k), 0xFF, 40);
    ega_plane(3)[3] = 0xF0;                 /* pixels 24-27: index 15 (the wheel) */
    for (int x = 0; x < W; x++) {
        RmTag top = x >= 16 && x < 24 ? rm_tag(2, x - 16, 0) : x >= 24 && x < 28 ? rm_tag(4, x - 24, 0) : rm_tag(1, x, 0);
        RmTag under = x >= 16 && x < 20 ? top : x >= 20 && x < 24 ? rm_tag(3, x - 20, 0) : 0;
        rm_set_at(0xA000, (u16)(x >> 3), 7 - (x & 7), top, under);
    }
    /* pixel 30: a stale sprite tag (the screen shows the ground's index 7, the sprite's original there is 3) */
    u8 stale[8] = { 3, 3, 3, 3, 3, 3, 3, 3 };
    rm_slot_register_raw(5, PART, stale, 8, 1);   /* a picture with art, its original 3 there */
    rm_set_at(0xA000, 3, 7 - (30 & 7), rm_tag(5, 2, 0), rm_tag(5, 2, 0));
    u32 rgb[16];
    for (int i = 0; i < 16; i++) rgb[i] = 0xAAAAAA;
    static u32 frame[320 * 200];
    u8 *planes[4] = { ega_plane(0), ega_plane(1), ega_plane(2), ega_plane(3) };
    rm_compose(frame, 1, rgb, 0, 1000, planes);
    CHECK(frame[2] == 0x404040);
    CHECK(frame[25] == 0xFFFFFF);           /* the wheel */            /* the ground's art */
    CHECK(frame[18] == 0x404040);           /* the sprite's transparent pixel: the ground's art, not 0xAAAAAA */
    CHECK(frame[23] == 0x404040);           /* under is a part transparent there too */
    CHECK(frame[30] == 0x404040);           /* a stale tag: the ground's art */
    printf("%s transparent_sprite_shows_ground\n", failures ? "FAIL" : "ok  ");

    /* the game's fade to black (every register black): nothing of the art shows, not even texels drawn
     * lighter than black on the black index */
    int before = failures;
    u8 black[4] = { 0, 0, 0, 0 };
    rm_slot_register_raw(6, DARK, black, 4, 1);
    for (int x = 40; x < 44; x++) {
        int bit = 7 - (x & 7);
        for (int k = 0; k < 4; k++) ega_plane(k)[x >> 3] &= (u8)~(1 << bit);
        rm_set_at(0xA000, (u16)(x >> 3), bit, rm_tag(6, x - 40, 0), 0);
    }
    rgb[0] = 0;                             /* the normal palette: black is black */
    memset(frame, 0, sizeof frame);
    rm_compose(frame, 1, rgb, 0, 1000, planes);
    CHECK(frame[41] == 0x303030);           /* shown as drawn in the normal palette */
    u32 dark[16] = { 0 };
    memset(frame, 0, sizeof frame);
    rm_compose(frame, 1, dark, 0, 1000, planes);
    CHECK(frame[41] == 0);
    CHECK(frame[2] == 0);
    int a;
    CHECK(rm_texel(rm_pic(6), 4, 0, dark, &a) == 0);
    printf("%s black_palette_shows_black\n", failures != before ? "FAIL" : "ok  ");

    /* the enhanced view's command bodies (any thread): a picture never looked up is not loaded, only counted */
    before = failures;
    rm_slot_register_raw(7, WHEEL, white, 4, 1);
    CHECK(rm_pic_ready(7) == NULL);
    CHECK(rm_unready() == 1);
    CHECK(rm_pic(7) && rm_pic(7)->art == 1);    /* still untried: rm_pic loads it now */
    CHECK(rm_pic_ready(7) == rm_pic(7));
    CHECK(rm_unready() == 1);
    printf("%s pic_ready_never_loads\n", failures != before ? "FAIL" : "ok  ");
    return failures != 0;
}
