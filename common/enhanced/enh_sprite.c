/* Pictures of the road view for the enhanced renderer: the game's planar bitmaps (the opponent's and the
 * police car's views) and rectangles of the off-screen sheets in video memory (signs, billboards), decoded
 * to palette-index images, and drawn scaled into the sample buffer. */
#include "enh_draw.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "mem.h"
#include "platform/ega.h"
#include "platform/video.h"
#include "remaster/rm_pack.h"
#include "remaster/rm_tags.h"

#define CACHE 48

const u32 *enh_cur_rgb;

static struct { u32 key; EnhImage img; } cache[CACHE];
static int ncache;

void enh_images_reset(void)
{
    for (int i = 0; i < ncache; i++) { SDL_free(cache[i].img.px); SDL_free(cache[i].img.rm); }
    ncache = 0;
}

static EnhImage *cache_slot(u32 key, int w, int h, bool *fresh)
{
    for (int i = 0; i < ncache; i++)
        if (cache[i].key == key && cache[i].img.w == w && cache[i].img.h == h) { *fresh = false; return &cache[i].img; }
    if (ncache == CACHE) enh_images_reset();
    EnhImage *img = &cache[ncache].img;
    img->rm = NULL;
    cache[ncache++].key = key;
    img->w = w;
    img->h = h;
    img->px = SDL_malloc((size_t)w * h);
    *fresh = true;
    return img;
}

/* ENH: the remaster tags of img's pixels, each kept only where its picture has art and the colour matches
 * the original pixel the tag names. */
static void attach_art(EnhImage *img, RmTag (*tag)(const void *ctx, int i, int j), const void *ctx)
{
    SDL_free(img->rm);
    img->rm = NULL;
    if (!rm_pack_active()) return;
    int w = img->w, h = img->h, n = 0;
    u32 *t = SDL_calloc((size_t)w * h, sizeof *t);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            u8 c = img->px[j * w + i];
            RmTag g = c == ENH_CLEAR ? 0 : tag(ctx, i, j);
            if (!g) continue;
            RmPic *p = rm_pic(rm_tag_slot(g));
            int u = rm_tag_u(g), v = rm_tag_v(g);
            if (!p || u >= p->w || v >= p->h || p->orig[v * p->w + u] != c) continue;
            t[j * w + i] = g;
            n++;
        }
    if (n) img->rm = t;
    else SDL_free(t);
}

typedef struct { FarPtr pl; int stride, h; } BitmapCtx;
static RmTag bitmap_tag(const void *ctx, int i, int j)
{
    const BitmapCtx *b = ctx;
    return rm_mtag(far_lin(b->pl) + (u32)(j * b->stride + (i >> 3)), 7 - (i & 7));
}

typedef struct { u16 page; int x, y; } VramCtx;
static RmTag vram_tag(const void *ctx, int i, int j)
{
    const VramCtx *v = ctx;
    return rm_vtag((u16)(v->page + (v->y + j) * EGA_BYTES_PER_LINE + ((v->x + i) >> 3)), 7 - ((v->x + i) & 7));
}

/* A RAM bitmap (desc: DESC_* layout, video.h): 4 planes of h rows of `stride` bytes, the mask plane's set
 * bits transparent. */
const EnhImage *enh_image_bitmap(u16 seg, u16 off)
{
    FarPtr d = { off, seg };
    if (far_is_null(d)) return NULL;
    int w = desc_w(d), h = desc_h(d), stride = desc_stride(d);
    if (w <= 0 || h <= 0 || w > 640 || h > 400) return NULL;
    bool fresh;
    EnhImage *img = cache_slot((u32)seg << 16 | off, w, h, &fresh);
    if (!fresh) return img;
    FarPtr pl = desc_planes(d), mk = desc_mask(d);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            u16 o = (u16)(y * stride + (x >> 3));
            int bit = 7 - (x & 7), c = 0;
            if (!far_is_null(mk) && (rd8(mk.seg, (u16)(mk.off + o)) >> bit & 1)) { img->px[y * w + x] = ENH_CLEAR; continue; }
            for (int k = 0; k < 4; k++)
                c |= (rd8(pl.seg, (u16)(pl.off + k * stride * h + o)) >> bit & 1) << k;
            img->px[y * w + x] = (u8)c;
        }
    BitmapCtx bc = { pl, stride, h };
    attach_art(img, bitmap_tag, &bc);
    return img;
}

/* A w x h rectangle at (x, y) of the page at plane offset `page` (opaque). */
const EnhImage *enh_image_vram(u16 page, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return NULL;
    bool fresh;
    EnhImage *img = cache_slot(0xFFFF0000u ^ (u32)(page + y * 320 + x), w, h, &fresh);
    if (!fresh) return img;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            u16 o = (u16)(page + (y + j) * EGA_BYTES_PER_LINE + ((x + i) >> 3));
            int bit = 7 - ((x + i) & 7), c = 0;
            for (int k = 0; k < 4; k++) c |= (ega_plane(k)[o] >> bit & 1) << k;
            img->px[j * w + i] = (u8)c;
        }
    VramCtx vc = { page, x, y };
    attach_art(img, vram_tag, &vc);
    return img;
}

/* The image stretched over [x0, x1) x [y0, y1) (original screen coordinates), nearest texel per sample;
 * mirrored when x1 < x0. */
void enh_draw_image(const EnhImage *img, double x0, double y0, double x1, double y1)
{
    if (!img || y1 <= y0 || x1 == x0) return;
    EnhBuf *b = &enh_buf;
    bool flip = x1 < x0;
    double l = flip ? x1 : x0, r = flip ? x0 : x1;
    double sl = l * b->sa, sr = r * b->sa, st = (y0 - ENH_VIEW_Y0) * b->sa, sb = (y1 - ENH_VIEW_Y0) * b->sa;
    int i0 = SDL_max(b->cx0, (int)ceil(sl - 0.5)), i1 = SDL_min(b->cx1, (int)ceil(sr - 0.5));
    int j0 = SDL_max(b->cy0, (int)ceil(st - 0.5)), j1 = SDL_min(b->cy1, (int)ceil(sb - 0.5));
    double du = img->w / (sr - sl), dv = img->h / (sb - st);
    if (img->rm && b->rgb && enh_cur_rgb) {             /* ENH: remastered art, 4x the texels */
        int st_mode = rm_selftest();
        for (int j = j0; j < j1; j++) {
            double vf = (j + 0.5 - st) * dv;
            int v = (int)vf;
            if (v < 0 || v >= img->h) continue;
            int fv = (int)((vf - v) * RM_ART_SCALE);
            u8 *out = b->px + (size_t)j * b->w;
            u32 *orgb = b->rgb + (size_t)j * b->w;
            for (int i = i0; i < i1; i++) {
                double uf = (i + 0.5 - sl) * du;
                int u = (int)uf;
                if (u < 0 || u >= img->w) continue;
                int fu = (int)((uf - u) * RM_ART_SCALE), su = flip ? img->w - 1 - u : u;
                if (flip) fu = RM_ART_SCALE - 1 - fu;
                u8 c = img->px[(size_t)v * img->w + su];
                RmTag t = img->rm[(size_t)v * img->w + su];
                RmPic *art = t ? rm_pic(rm_tag_slot(t)) : NULL;
                if (!art) {
                    if (c != ENH_CLEAR) out[i] = c;
                    continue;
                }
                int a;
                u32 col = rm_texel(art, rm_tag_u(t) * RM_ART_SCALE + fu, rm_tag_v(t) * RM_ART_SCALE + fv, enh_cur_rgb, &a);
                if (st_mode) rm_selftest_race(a < 128 || col != enh_cur_rgb[c]);
                if (a < 128) continue;
                if (st_mode == 2) col = (col >> 1 & 0x7F7F7F) + 0x400060;
                out[i] = ENH_RGB;
                orgb[i] = col;
            }
        }
        return;
    }
    for (int j = j0; j < j1; j++) {
        int v = (int)((j + 0.5 - st) * dv);
        if (v < 0 || v >= img->h) continue;
        const u8 *src = img->px + (size_t)v * img->w;
        u8 *out = b->px + (size_t)j * b->w;
        for (int i = i0; i < i1; i++) {
            int u = (int)((i + 0.5 - sl) * du);
            if (u < 0 || u >= img->w) continue;
            u8 c = src[flip ? img->w - 1 - u : u];
            if (c != ENH_CLEAR) out[i] = c;
        }
    }
}

void enh_fill_rect(double x0, double y0, double x1, double y1, u8 c)
{
    double xs[4] = { x0, x1, x1, x0 }, ys[4] = { y0, y0, y1, y1 };
    enh_fill_poly(xs, ys, 4, c);
}
