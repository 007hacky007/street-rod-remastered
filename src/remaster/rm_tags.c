/* rm_tags.c -- see rm_tags.h. */
#include "remaster/rm_tags.h"

#include <stdlib.h>
#include <string.h>

#include "platform/vga.h"

#define VPIX ((size_t)0x10000 * 8)
#define MPIX ((size_t)MEM_SIZE * 8)

bool rm_tags_active;
static bool on;
static int suspended, armed;
static RmTag *vtop, *vund, *mtop, *mund;  /* index = byte * 8 + (7 - bit): pixel order */
static RmTag latch_top[8], latch_und[8];  /* of the last video memory read, by pixel 0..7 */
static RmTag cpu_top[8], cpu_und[8];      /* of the last byte read (video memory or RAM) */

static void update_active(void) { rm_tags_active = on && suspended == 0; }

bool rm_tags_on(void) { return on; }

void rm_tags_enable(void)
{
    if (on) return;
    vtop = calloc(VPIX, sizeof *vtop);
    vund = calloc(VPIX, sizeof *vund);
    mtop = calloc(MPIX, sizeof *mtop);
    mund = calloc(MPIX, sizeof *mund);
    if (!vtop || !vund || !mtop || !mund) return;
    on = true;
    update_active();
}

void rm_tags_reset(void)
{
    if (!on) return;
    memset(vtop, 0, VPIX * sizeof *vtop);
    memset(vund, 0, VPIX * sizeof *vund);
    memset(mtop, 0, MPIX * sizeof *mtop);
    memset(mund, 0, MPIX * sizeof *mund);
    memset(latch_top, 0, sizeof latch_top);
    memset(latch_und, 0, sizeof latch_und);
    memset(cpu_top, 0, sizeof cpu_top);
    memset(cpu_und, 0, sizeof cpu_und);
    suspended = armed = 0;
    update_active();
}

static inline size_t px(u32 byte, int bit) { return (size_t)byte * 8 + (size_t)(7 - bit); }

RmTag rm_vtag(u16 off, int bit)   { return on ? vtop[px(off, bit)] : 0; }
RmTag rm_vunder(u16 off, int bit) { return on ? vund[px(off, bit)] : 0; }
RmTag rm_mtag(u32 l, int bit)     { return on && l < MEM_SIZE ? mtop[px(l, bit)] : 0; }
RmTag rm_munder(u32 l, int bit)   { return on && l < MEM_SIZE ? mund[px(l, bit)] : 0; }

/* The top and under arrays and index of pixel `bit` of seg:off. */
static bool slot_at(u16 seg, u16 off, int bit, RmTag **top, RmTag **und, size_t *i)
{
    if (!on) return false;
    if (is_vram(seg)) {
        *top = vtop; *und = vund; *i = px(vram_off(seg, off), bit);
    } else {
        u32 l = lin(seg, off);
        if (l >= MEM_SIZE) return false;
        *top = mtop; *und = mund; *i = px(l, bit);
    }
    return true;
}

RmTag rm_tag_at(u16 seg, u16 off, int bit)
{
    RmTag *t, *u; size_t i;
    return slot_at(seg, off, bit, &t, &u, &i) ? t[i] : 0;
}

RmTag rm_under_at(u16 seg, u16 off, int bit)
{
    RmTag *t, *u; size_t i;
    return slot_at(seg, off, bit, &t, &u, &i) ? u[i] : 0;
}

void rm_set_at(u16 seg, u16 off, int bit, RmTag top, RmTag under)
{
    RmTag *t, *u; size_t i;
    if (slot_at(seg, off, bit, &t, &u, &i)) { t[i] = top; u[i] = under; }
}

void rm_on_vram_read(u16 off)
{
    if (!on) return;
    size_t b = (size_t)off * 8;
    memcpy(latch_top, vtop + b, sizeof latch_top);
    memcpy(latch_und, vund + b, sizeof latch_und);
    memcpy(cpu_top, latch_top, sizeof cpu_top);
    memcpy(cpu_und, latch_und, sizeof cpu_und);
}

void rm_on_vram_write(u16 off, u8 cpu_bits, bool data)
{
    if (!rm_tags_active) return;
    size_t b = (size_t)off * 8;
    for (int p = 0; p < 8; p++) {
        int bit = 7 - p;
        if (!(cpu_bits >> bit & 1)) {            /* the pixel keeps the latched value */
            vtop[b + p] = latch_top[p];
            vund[b + p] = latch_und[p];
        } else if (armed && data) {
            vtop[b + p] = cpu_top[p];
            vund[b + p] = cpu_und[p];
        } else {
            vtop[b + p] = 0;
            vund[b + p] = 0;
        }
    }
}

void rm_on_ram_read(u32 l)
{
    if (!on || l >= MEM_SIZE) return;
    memcpy(cpu_top, mtop + (size_t)l * 8, sizeof cpu_top);
    memcpy(cpu_und, mund + (size_t)l * 8, sizeof cpu_und);
}

void rm_on_ram_write(u32 l)
{
    if (!rm_tags_active || l >= MEM_SIZE) return;
    size_t b = (size_t)l * 8;
    if (armed) {
        memcpy(mtop + b, cpu_top, sizeof cpu_top);
        memcpy(mund + b, cpu_und, sizeof cpu_und);
    } else {
        memset(mtop + b, 0, sizeof cpu_top);
        memset(mund + b, 0, sizeof cpu_und);
    }
}

void rm_copy_begin(void) { armed++; }
void rm_copy_end(void)   { if (armed > 0) armed--; }
void rm_suspend(void)    { suspended++; update_active(); }
void rm_resume(void)     { if (suspended > 0) suspended--; update_active(); }

void rm_seed_bitmap(FarPtr planes, int w, int h, int stride, int slot)
{
    if (!on) return;
    u32 base = far_lin(planes);
    for (int k = 0; k < 4; k++)
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                u32 l = base + (u32)(k * stride * h + y * stride + (x >> 3));
                if (l >= MEM_SIZE) continue;
                mtop[px(l, 7 - (x & 7))] = rm_tag(slot, x, y);
                mund[px(l, 7 - (x & 7))] = 0;
            }
}

void rm_over(u16 dseg, u16 doff, int dbit, u16 sseg, u16 soff, int sbit)
{
    RmTag *dt, *du, *st, *su; size_t di, si;
    if (!slot_at(dseg, doff, dbit, &dt, &du, &di) || !slot_at(sseg, soff, sbit, &st, &su, &si)) return;
    if (dt[di] == st[si]) return;                /* idempotent over the four plane passes */
    du[di] = dt[di];
    dt[di] = st[si];
}

void rm_copy_px(u16 dseg, u16 doff, int dbit, u16 sseg, u16 soff, int sbit)
{
    RmTag *dt, *du, *st, *su; size_t di, si;
    if (!slot_at(dseg, doff, dbit, &dt, &du, &di) || !slot_at(sseg, soff, sbit, &st, &su, &si)) return;
    dt[di] = st[si];
    du[di] = su[si];
}

void rm_clear_px(u16 seg, u16 off, int bit)
{
    RmTag *t, *u; size_t i;
    if (slot_at(seg, off, bit, &t, &u, &i)) { t[i] = 0; u[i] = 0; }
}

void rm_masked_byte(u16 dseg, u16 doff, u16 sseg, u16 soff, u8 mask)
{
    if (!on) return;
    for (int bit = 0; bit < 8; bit++)
        if (!(mask >> bit & 1)) rm_over(dseg, doff, bit, sseg, soff, bit);
}

void rm_composite_byte(u16 dseg, u16 doff, u16 sseg, u16 soff, u16 bseg, u16 boff, u8 mask)
{
    if (!on) return;
    for (int bit = 0; bit < 8; bit++) {
        if (mask >> bit & 1) rm_copy_px(dseg, doff, bit, bseg, boff, bit);
        else rm_set_at(dseg, doff, bit, rm_tag_at(sseg, soff, bit), rm_tag_at(bseg, boff, bit));
    }
}

void rm_copy_bits(u16 dseg, u16 doff, u16 sseg, u16 soff, u8 bits)
{
    if (!on) return;
    for (int bit = 0; bit < 8; bit++)
        if (bits >> bit & 1) rm_copy_px(dseg, doff, bit, sseg, soff, bit);
}
