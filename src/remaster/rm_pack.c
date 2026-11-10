/* rm_pack.c -- see rm_pack.h. */
#include "remaster/rm_pack.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/ega.h"
#include "platform/vga.h"
#include "remaster/rm_color.h"
#include "remaster/rm_dump.h"
#include "remaster/rm_tags.h"

typedef struct { char sha1[41]; u32 ref[16]; } Entry;

static bool active;
static char *pack_dir;
static Entry *entries;
static int nentries;
static RmPic pics[RM_MAX_SLOTS];
static int selftest;
static FILE *palette_log;
static unsigned long long st_frames, st_replaced, st_mismatched, st_race;

/* ------------------------------------------------------------------ pack.json */

static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

static int cmp_entry(const void *a, const void *b) { return strcmp(((const Entry *)a)->sha1, ((const Entry *)b)->sha1); }

/* Only what the engine needs: each "<40 hex>": {"ref": ["RRGGBB" x 16]} inside "pictures". */
static bool parse_meta(const char *s)
{
    const char *p = strstr(s, "\"pictures\"");
    if (!p) return false;
    const char *end = strstr(p, "\"shapes\"");
    int cap = 0;
    while ((p = strchr(p + 1, '"')) && (!end || p < end)) {
        const char *q = p + 1;
        int n = 0;
        while (hexval(q[n]) >= 0) n++;
        if (n != 40 || q[40] != '"') continue;
        const char *r = strstr(q, "\"ref\"");
        if (!r) break;
        r = strchr(r, '[');
        if (!r) break;
        Entry e;
        memcpy(e.sha1, q, 40);
        e.sha1[40] = 0;
        for (int i = 0; i < 40; i++) if (e.sha1[i] >= 'A' && e.sha1[i] <= 'F') e.sha1[i] = (char)(e.sha1[i] + 32);
        int k = 0;
        for (const char *c = r; *c && *c != ']' && k < 16; c++) {
            if (*c != '"') continue;
            u32 v = 0;
            int d = 0;
            for (c++; hexval(*c) >= 0; c++, d++) v = v << 4 | (u32)hexval(*c);
            if (!*c) break;                              /* the file ends inside the string */
            if (d == 6) e.ref[k++] = v;
        }
        if (k != 16) continue;
        if (nentries == cap) {
            cap = cap ? cap * 2 : 512;
            entries = realloc(entries, (size_t)cap * sizeof *entries);
        }
        entries[nentries++] = e;
        p = strchr(r, ']');
        if (!p) break;
    }
    qsort(entries, (size_t)nentries, sizeof *entries, cmp_entry);
    return true;
}

bool rm_pack_open(const char *dir)
{
    char path[1100];
    snprintf(path, sizeof path, "%s/pack.json", dir);
    size_t n = 0;
    char *s = SDL_LoadFile(path, &n);
    if (!s) { fprintf(stderr, "remaster: cannot read %s\n", path); return false; }
    bool ok = parse_meta(s);
    SDL_free(s);
    if (!ok) { fprintf(stderr, "remaster: bad %s\n", path); return false; }
    pack_dir = SDL_strdup(dir);
    rm_tags_enable();
    active = rm_tags_on();
    const char *t = SDL_getenv("SR_REMASTER_SELFTEST");
    selftest = t ? SDL_atoi(t) : 0;
    const char *log = SDL_getenv("SR_REMASTER_PALETTE_LOG");
    if (log && *log) palette_log = fopen(log, "a");
    return active;
}

bool rm_pack_active(void) { return active; }

/* ------------------------------------------------------------------ pictures */

void rm_slot_register(int slot, FarPtr packed, u16 psize, FarPtr planes, int w, int h, int stride)
{
    if (!active || slot <= 0 || slot >= RM_MAX_SLOTS || pics[slot].orig) return;
    if (w <= 0 || h <= 0 || w > 640 || h > 400) return;
    u8 *buf = malloc(psize ? psize : 1);
    rm_suspend();
    for (u16 i = 0; i < psize; i++) buf[i] = vrd(packed.seg, (u16)(packed.off + i));
    RmPic *p = &pics[slot];
    rm_sha1_hex(buf, psize, p->sha1);
    free(buf);
    p->w = w;
    p->h = h;
    p->orig = malloc((size_t)w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = 0;
            for (int k = 0; k < 4; k++)
                c |= (rd8(planes.seg, (u16)(planes.off + k * stride * h + y * stride + (x >> 3))) >> (7 - (x & 7)) & 1) << k;
            p->orig[y * w + x] = (u8)c;
        }
    rm_resume();
}

static u8 *load_rgba(const char *path, int w, int h)
{
    SDL_Surface *s = SDL_LoadPNG(path);
    if (!s) return NULL;
    SDL_Surface *c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (!c) return NULL;
    u8 *out = NULL;
    if (c->w == w && c->h == h) {
        out = malloc((size_t)w * h * 4);
        for (int y = 0; y < h; y++) memcpy(out + (size_t)y * w * 4, (u8 *)c->pixels + (size_t)y * c->pitch, (size_t)w * 4);
    }
    SDL_DestroySurface(c);
    return out;
}

RmPic *rm_pic(int slot)
{
    if (!active || slot <= 0 || slot >= RM_MAX_SLOTS) return NULL;
    RmPic *p = &pics[slot];
    if (!p->orig || p->art < 0) return NULL;
    if (p->art == 1) return p;
    p->art = -1;
    Entry key;
    memcpy(key.sha1, p->sha1, sizeof key.sha1);
    Entry *e = bsearch(&key, entries, (size_t)nentries, sizeof *entries, cmp_entry);
    if (!e) return NULL;
    int w4 = p->w * RM_ART_SCALE, h4 = p->h * RM_ART_SCALE;
    char path[1200];
    snprintf(path, sizeof path, "%s/pics/%s.png", pack_dir, p->sha1);
    u8 *rgba = load_rgba(path, w4, h4);
    snprintf(path, sizeof path, "%s/pics/%s.idx.png", pack_dir, p->sha1);
    u8 *ix = rgba ? load_rgba(path, w4, h4) : NULL;
    if (!rgba || !ix) {
        fprintf(stderr, "remaster: art for %s is missing or not %dx%d\n", p->sha1, w4, h4);
        free(rgba);
        free(ix);
        return NULL;
    }
    p->idx = malloc((size_t)w4 * h4);
    for (size_t i = 0; i < (size_t)w4 * h4; i++) p->idx[i] = (u8)(ix[i * 4] & 15);
    free(ix);
    p->rgba = rgba;
    memcpy(p->ref, e->ref, sizeof p->ref);
    p->art = 1;
    return p;
}

/* ------------------------------------------------------------------ compose */

static inline u32 texel(const RmPic *p, int tu, int tv, const u32 rgb[16], int *alpha)
{
    size_t i = (size_t)tv * p->w * RM_ART_SCALE + tu;
    const u8 *c = p->rgba + i * 4;
    *alpha = c[3];
    int ix = p->idx[i];
    return rm_shade((u32)c[0] << 16 | (u32)c[1] << 8 | c[2], p->ref[ix], rgb[ix]);
}

static inline u32 mix(u32 a, u32 b, int alpha)
{
    u32 out = 0;
    for (int sh = 16; sh >= 0; sh -= 8) {
        int x = (int)(a >> sh & 255), y = (int)(b >> sh & 255);
        out |= (u32)((x * alpha + y * (255 - alpha) + 127) / 255) << sh;
    }
    return out;
}

static void log_palette(RmPic *p, const u32 rgb[16])
{
    if (!palette_log || p->logged) return;
    p->logged = true;
    fprintf(palette_log, "{\"sha1\":\"%s\",\"pal\":[", p->sha1);
    for (int i = 0; i < 16; i++) fprintf(palette_log, "%s\"%06X\"", i ? "," : "", (unsigned)rgb[i]);
    fprintf(palette_log, "]}\n");
    fflush(palette_log);
}

void rm_compose(u32 *xrgb, int scale, const u32 rgb[16], u16 start, int line_compare, u8 *const planes[4])
{
    if (!active) return;
    int w = 320 * scale;
    for (int y = 0; y < 200; y++) {
        u16 row = y < line_compare ? (u16)(start + y * EGA_BYTES_PER_LINE) : (u16)((y - line_compare) * EGA_BYTES_PER_LINE);
        for (int x = 0; x < 320; x++) {
            u16 o = (u16)(row + (x >> 3));
            int bit = 7 - (x & 7);
            RmTag t = rm_vtag(o, bit);
            if (!t) continue;
            RmPic *p = rm_pic(rm_tag_slot(t));
            if (!p) continue;
            int u = rm_tag_u(t), v = rm_tag_v(t);
            if (u >= p->w || v >= p->h) continue;
            int c = (planes[0][o] >> bit & 1) | (planes[1][o] >> bit & 1) << 1 | (planes[2][o] >> bit & 1) << 2 |
                    (planes[3][o] >> bit & 1) << 3;
            if (p->orig[v * p->w + u] != c) continue;        /* verify: the screen still shows that pixel */
            log_palette(p, rgb);
            RmTag ut = rm_vunder(o, bit);
            RmPic *up = ut ? rm_pic(rm_tag_slot(ut)) : NULL;
            int uu = up ? rm_tag_u(ut) : 0, uv = up ? rm_tag_v(ut) : 0;
            if (up && (uu >= up->w || uv >= up->h)) up = NULL;
            bool differs = false;
            for (int j = 0; j < scale; j++) {
                u32 *out = xrgb + (size_t)(y * scale + j) * w + (size_t)x * scale;
                int tv = v * RM_ART_SCALE + j * RM_ART_SCALE / scale;
                for (int i = 0; i < scale; i++) {
                    int tu = u * RM_ART_SCALE + i * RM_ART_SCALE / scale, a;
                    u32 col = texel(p, tu, tv, rgb, &a);
                    if (a < 255) {
                        u32 under = rgb[c];
                        if (up) {
                            int ua;
                            under = texel(up, uu * RM_ART_SCALE + i * RM_ART_SCALE / scale,
                                          uv * RM_ART_SCALE + j * RM_ART_SCALE / scale, rgb, &ua);
                        }
                        col = mix(col, under, a);
                    }
                    if (col != rgb[c]) differs = true;
                    if (selftest == 2) col = mix(col, 0x400000u * (u32)(rm_tag_slot(t) % 4) + 0x4000u * (u32)(rm_tag_slot(t) / 4 % 4) + 0xC0u, 110);
                    out[i] = col;
                }
            }
            if (selftest) {
                st_replaced++;
                if (differs) st_mismatched++;
            }
        }
    }
    if (selftest && ++st_frames % 200 == 0) rm_pack_report();
}

void rm_pack_report(void)
{
    if (selftest)
        fprintf(stderr, "remaster selftest: frames %llu replaced %llu mismatched %llu race %llu\n", st_frames,
                st_replaced, st_mismatched, st_race);
}

int rm_selftest(void) { return selftest; }

void rm_selftest_race(bool differs)
{
    st_race++;
    if (differs) st_mismatched++;
}

u32 rm_texel(const RmPic *p, int tu, int tv, const u32 rgb[16], int *alpha) { return texel(p, tu, tv, rgb, alpha); }
