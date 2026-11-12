#pragma once
/* The enhanced view's drawing core (common/enhanced: enh_raster.c, enh_sprite.c), shared by both games. Each
 * game builds it into its own core with its own mem.h, platform/ and remaster/ headers. */
#include <SDL3/SDL_stdinc.h>

#include "types.h"

/* The road view on screen (race_road.c clip DS:5E4E..: rows 12h..65h), the split below it. */
#define ENH_VIEW_Y0 0x12
#define ENH_VIEW_Y1 0x66

typedef struct {
    bool enabled;
    int res_scale, aa, motion_delay;
    bool town;
} EnhOptions;
extern EnhOptions enh_opt;

/* enh_raster.c: samples of the view rows ENH_VIEW_Y0..Y1, sa = res scale x aa per original pixel. */
typedef struct {
    int w, h, sa;
    u8 *px;                    /* palette indices; ENH_RGB = the colour in rgb[] (remastered art) */
    u32 *rgb;                  /* ENH: remaster: allocated only with an art pack */
    int cx0, cy0, cx1, cy1;    /* clip rectangle, samples */
} EnhBuf;
extern EnhBuf enh_buf;
void enh_buf_setup(void);
/* Drawing is clipped to [x0, x1) x [y0, y1) of the original screen; enh_clip_view() = the whole view. */
void enh_clip(double x0, double y0, double x1, double y1);
void enh_clip_view(void);
void enh_fill_rows(double y0, double y1, u8 c);                  /* original screen rows [y0, y1) */
void enh_fill_poly(const double *xs, const double *ys, int n, u8 c);
/* Output rows ENH_VIEW_Y0..y_end of xrgb (320*S wide): the samples resolved through pal, except where the
 * EGA page at plane offset `page` has written or foreground pixels. */
void enh_resolve(u32 *xrgb, const u32 *pal, int npal, u16 page, int y_end);

/* enh_sprite.c: palette-index images, ENH_CLEAR = transparent. */
#define ENH_CLEAR 0xFF
#define ENH_RGB   0xFE
/* rm: NULL or w * h remaster tags (rm_tags.h), one per pixel: which art pixel it shows, 0 = none. */
typedef struct { int w, h; u8 *px; u32 *rm; } EnhImage;
extern const u32 *enh_cur_rgb;     /* the palette of the frame being drawn (enh_compose) */
void enh_images_reset(void);
const EnhImage *enh_image_bitmap(u16 seg, u16 off);
const EnhImage *enh_image_vram(u16 page, int x, int y, int w, int h);
void enh_draw_image(const EnhImage *img, double x0, double y0, double x1, double y1);
void enh_fill_rect(double x0, double y0, double x1, double y1, u8 c);

/* enh_bands.c: the drawing calls above (and each game's own: its sky rows, polygons, lines, turned images) record
 * a command list for the frame instead of writing the sample buffer; enh_resolve runs it in horizontal bands, one
 * per logical core (worker threads, the calling thread takes the first band), each band its commands clipped to its
 * rows in the order recorded and then its rows resolved. Bands never share a row, so the frame is the one a single
 * thread draws. One band (SR_ENH_THREADS=1, no worker, the remaster's self-test or palette log on) draws directly.
 * A drawing call packs its arguments and hands them with its body to enh_submit; the body draws under k's clip
 * (the clip at the call, within the band's rows; by0..by1 the band's sample rows for a body that ignores the clip)
 * and k->rgb in place of enh_cur_rgb. Arguments are copied; pointers in them (image pixels and tags) must stay valid
 * until the commands ran: enh_flush first before freeing or changing what a recorded command may read. */
typedef struct { int cx0, cy0, cx1, cy1, by0, by1; const u32 *rgb; } EnhCtx;
typedef void (*EnhCmdFn)(const EnhCtx *k, const void *arg);
void enh_submit(EnhCmdFn fn, const void *arg, size_t size);
void enh_flush(void);                  /* runs the recorded commands now (in bands) */
int  enh_bands(void);                  /* bands a frame is drawn in, 1 = directly */
void enh_bands_shutdown(void);         /* runs anything recorded, then stops the workers (before SDL_Quit) */
typedef struct {
    int bands;          /* bands the last frame's commands ran in (1: every command drawn directly) */
    int cmds;           /* commands recorded in the last frame */
    int flushes;        /* runs before the last frame's resolve (full list, cache reset, buffer change) */
    long full;          /* times the list was full since the start */
    double band_ms_max, band_ms_mean;   /* last frame: slowest band and mean band (run + resolve), ms */
} EnhBandStats;
void enh_bands_stats(EnhBandStats *out);
/* Test and developer hook: stops the workers (running anything recorded) and makes the next submit start
 * `bands` bands (0: SR_ENH_THREADS or one per core) with a list of cmd_max commands and arena bytes (0: the
 * defaults 32768 and 4 MiB); fail_after >= 0 makes thread creation fail after that many workers (-1: never). */
void enh_bands_config(int bands, int cmd_max, size_t arena, int fail_after);
/* SR_ENH_THREADS_CHECK: draw(v) once directly and once in bands (odd frames in bands first), both over a copy of
 * xrgb's picture and over samples reset first (px and rgb), and the sample buffers and resolved frames compared byte
 * for byte; xrgb ends as the banded frame. xrgb NULL with y_end <= ENH_VIEW_Y0: the samples only, no resolve. The
 * line on stderr: frames compared, the fewest bands a banded pass ran in, frames differing, frames whose banded pass
 * drew art (ENH_RGB), frames compared without xrgb (SR2's compare tool). */
void enh_bands_check(void (*draw)(const void *v), const void *v, u32 *xrgb, const u32 *pal, int npal, u16 page,
                     int y_end);
bool enh_bands_check_on(void);
