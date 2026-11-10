/* rm_dump.c -- the remaster's picture dump: the game's own unpacker (pic_unpack) on the library files,
 * read directly, so no window or platform start-up is needed. See rm_dump.h. */
#include "remaster/rm_dump.h"

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "platform/ega.h"
#include "platform/video.h"

/* Scratch segments for one picture: above the program image and DGROUP, unused before the game starts. */
#define PACK_SEG 0x8000
#define DATA_SEG 0x9000

/* ------------------------------------------------------------------ SHA-1 (FIPS 180-1) */

typedef struct { u32 h[5]; uint64_t len; u8 buf[64]; u32 used; } Sha1;

static u32 rol(u32 x, int n) { return x << n | x >> (32 - n); }

static void sha1_block(Sha1 *s, const u8 *p)
{
    u32 w[80];
    for (int i = 0; i < 16; i++) w[i] = (u32)p[4 * i] << 24 | (u32)p[4 * i + 1] << 16 | (u32)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    u32 a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
    for (int i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
        u32 t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}

void rm_sha1_hex(const unsigned char *p, unsigned long n, char hex[41])
{
    Sha1 s = { { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 }, 0, { 0 }, 0 };
    s.len = (uint64_t)n * 8;
    while (n >= 64) { sha1_block(&s, p); p += 64; n -= 64; }
    u8 last[128] = { 0 };
    memcpy(last, p, n);
    last[n] = 0x80;
    int blocks = n + 9 > 64 ? 2 : 1;
    for (int i = 0; i < 8; i++) last[blocks * 64 - 1 - i] = (u8)(s.len >> (8 * i));
    for (int i = 0; i < blocks; i++) sha1_block(&s, last + 64 * i);
    for (int i = 0; i < 5; i++) snprintf(hex + 8 * i, 9, "%08x", (unsigned)s.h[i]);
}

/* ------------------------------------------------------------------ the dump */

static bool find_file(const char *dir, const char *name, char *path, size_t n)
{
    int count = 0;
    char **entries = SDL_GlobDirectory(dir, NULL, 0, &count);
    bool found = false;
    for (int i = 0; entries && i < count && !found; i++)
        if (SDL_strcasecmp(entries[i], name) == 0) {
            snprintf(path, n, "%s/%s", dir, entries[i]);
            found = true;
        }
    SDL_free(entries);
    return found;
}

static bool save_indexed(const char *path, const u8 *px, int w, int h, bool mask)
{
    SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_INDEX8);
    if (!s) return false;
    SDL_Palette *pal = SDL_CreateSurfacePalette(s);
    SDL_Color c[256];
    for (int i = 0; i < 256; i++) {
        u32 v = mask ? (u32)(i * 0x010101) : i < 16 ? ega_palette[i] : 0;
        c[i] = (SDL_Color){ (u8)(v >> 16), (u8)(v >> 8), (u8)v, 255 };
    }
    SDL_SetPaletteColors(pal, c, 0, 256);
    for (int y = 0; y < h; y++) memcpy((u8 *)s->pixels + (size_t)y * s->pitch, px + (size_t)y * w, (size_t)w);
    bool ok = SDL_SaveBMP(s, path);
    SDL_DestroySurface(s);
    return ok;
}

/* One library: the 30-byte directory records behind a u16 count, packed data from 2 + count * 30. */
static int dump_lib(const char *path, int lib, const char *out, FILE *jsonl)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = malloc((size_t)size);
    if (!d || fread(d, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(d); return -1; }
    fclose(f);
    int count = d[0] | d[1] << 8, done = 0;
    long base = 2 + (long)count * 30;
    u8 *px = malloc(640 * 400), *mk = malloc(640 * 400);
    for (int i = 0; i < count; i++) {
        const u8 *r = d + 2 + i * 30;
        int w = r[0] | r[1] << 8, h = r[2] | r[3] << 8, masked = r[6];
        long off = base + (long)(r[0x18] | r[0x19] << 8 | r[0x1A] << 16 | (u32)r[0x1B] << 24);
        int psize = r[0x1C] | r[0x1D] << 8;
        if (w <= 0 || h <= 0 || w > 640 || h > 400 || off < base || off + psize + 1 > size || d[off] != 0xBC) {
            fprintf(stderr, "%s: bad record %d\n", path, i);
            continue;
        }
        char sha[41];
        rm_sha1_hex(d + off, (unsigned long)psize, sha);
        /* The game's unpacker: the packed bytes after the leading BCh into DATA_SEG. */
        memcpy(mp(PACK_SEG, 0), d + off + 1, (size_t)(psize - 1));
        int stride = ((w - 1) >> 3) + 1, plane = stride * h;
        bool mask_only = (masked & 2) != 0;
        memset(mp(DATA_SEG, 0), 0, 0x10000);
        pic_unpack(far_make(PACK_SEG, 0), far_make(DATA_SEG, 0), (u16)(psize - 1), r + 7, 0x10);
        const u8 *pl = mp(DATA_SEG, 0);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int o = y * stride + (x >> 3), bit = 7 - (x & 7), c = 0;
                if (mask_only) c = (pl[o] >> bit & 1) ? 0 : 15;
                else
                    for (int k = 0; k < 4; k++) c |= (pl[k * plane + o] >> bit & 1) << k;
                px[y * w + x] = (u8)c;
                /* make_mask_plane: transparent where all four planes are 0 (colour 0) */
                mk[y * w + x] = (u8)(mask_only ? (c ? 0 : 255) : (masked && c == 0) ? 255 : 0);
            }
        char p[1200];
        snprintf(p, sizeof p, "%s/%s.bmp", out, sha);
        if (!save_indexed(p, px, w, h, false)) { fprintf(stderr, "cannot write %s\n", p); free(d); return -1; }
        if (masked) {
            snprintf(p, sizeof p, "%s/%s.mask.bmp", out, sha);
            if (!save_indexed(p, mk, w, h, true)) { free(d); return -1; }
        }
        int id = lib == 1 ? i + 1 : 1000 + i;
        fprintf(jsonl, "{\"sha1\":\"%s\",\"lib\":%d,\"index\":%d,\"id\":%d,\"w\":%d,\"h\":%d,\"masked\":%d}\n",
                sha, lib, i, id, w, h, masked);
        done++;
    }
    free(px);
    free(mk);
    free(d);
    return done;
}

int rm_dump_pictures(const char *game_dir, const char *out)
{
    char path[1024], jp[1100];
    snprintf(jp, sizeof jp, "%s/pictures.jsonl", out);
    FILE *jsonl = fopen(jp, "w");
    if (!jsonl) { fprintf(stderr, "cannot write %s\n", jp); return -1; }
    int total = 0;
    static const char *const names[2] = { "LIB1", "LIB2" };
    for (int lib = 1; lib <= 2; lib++) {
        if (!find_file(game_dir, names[lib - 1], path, sizeof path)) {
            fprintf(stderr, "%s not found in %s\n", names[lib - 1], game_dir);
            fclose(jsonl);
            return -1;
        }
        int n = dump_lib(path, lib, out, jsonl);
        if (n < 0) { fclose(jsonl); return -1; }
        total += n;
    }
    fclose(jsonl);
    return total;
}
