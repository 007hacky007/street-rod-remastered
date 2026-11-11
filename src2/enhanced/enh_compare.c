/* The enhanced view's check against the original frame (enh_compare.h). */
#include "enhanced/enh_compare.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *log_file;
static bool log_rows;              /* SR_ENH_COMPARE_ROWS: the rows whose edges differ by more than 1 px */

static bool is_road(u8 c, const u8 *road, int n)
{
    for (int i = 0; i < n; i++)
        if (road[i] == c) return true;
    return false;
}

/* The first and last covered road pixel of a row; false if none. */
static bool edges(const u8 *row, const u8 *cov, const u8 *road, int n, int *l, int *r)
{
    *l = -1;
    for (int x = 0; x < 320; x++)
        if (cov[x] && is_road(row[x], road, n)) { if (*l < 0) *l = x; *r = x; }
    return *l >= 0;
}

void enh_compare_rows(const u8 *orig, const u8 *enh, const u8 *covered, int y0, int y1, const u8 *road_colours,
                      int n_road, EnhCompare *out)
{
    EnhCompare c = { 0, 0, 0, 0, 0 };
    for (int y = y0; y < y1; y++) {
        const u8 *o = orig + (size_t)y * 320, *e = enh + (size_t)y * 320, *k = covered + (size_t)y * 320;
        c.rows++;
        for (int x = 0; x < 320; x++)
            if (k[x]) { c.pixels++; if (o[x] != e[x]) c.mismatched++; }
        int ol, or_, el, er;
        if (edges(o, k, road_colours, n_road, &ol, &or_) && edges(e, k, road_colours, n_road, &el, &er)) {
            /* An edge is measured only where it is seen: the pixel beyond it covered in both (an edge that runs
             * under the original's foreground, an object or the cockpit, cannot be compared). */
            int l = SDL_min(ol, el), r = SDL_max(or_, er);
            bool lv = l == 0 || k[l - 1], rv = r == 319 || k[r + 1];
            for (int x = l; lv && x <= SDL_max(ol, el); x++) lv = k[x] != 0;   /* and all between the two edges */
            for (int x = SDL_min(or_, er); rv && x <= r; x++) rv = k[x] != 0;
            if (!lv && !rv) continue;
            c.edge_rows++;
            int d = SDL_max(lv ? abs(ol - el) : 0, rv ? abs(or_ - er) : 0);
            if (d > c.edge_err_max) c.edge_err_max = d;
            if (log_rows && log_file && d > 1) fprintf(log_file, "  row %02X orig %d..%d enh %d..%d\n", y, ol, or_, el, er);
        }
    }
    *out = c;
}

static int frames, worst_edge;
static int *permille;              /* every frame's mismatch, for the median */
static int permille_cap;

bool enh_compare_open(void)
{
    const char *p = SDL_getenv("SR_ENH_COMPARE");
    if (!p || !*p) return false;
    log_file = fopen(p, "w");
    log_rows = SDL_getenv("SR_ENH_COMPARE_ROWS") != NULL;
    frames = worst_edge = 0;
    return log_file != NULL;
}

bool enh_compare_on(void) { return log_file != NULL; }

void enh_compare_frame(int mode, const EnhCompare *c)
{
    if (!log_file) return;
    int pm = c->pixels ? (int)((1000LL * c->mismatched + c->pixels / 2) / c->pixels) : 0;
    if (frames == permille_cap) {
        permille_cap = permille_cap ? permille_cap * 2 : 256;
        permille = realloc(permille, sizeof *permille * (size_t)permille_cap);
    }
    permille[frames++] = pm;
    if (c->edge_err_max > worst_edge) worst_edge = c->edge_err_max;
    fprintf(log_file, "frame %d mode %d rows %d edge_rows %d edge_err_max %d mismatch %d\n", frames, mode, c->rows,
            c->edge_rows, c->edge_err_max, pm);
    fflush(log_file);
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

void enh_compare_close(void)
{
    if (!log_file) return;
    int p50 = 0, max = 0;
    if (frames) {
        qsort(permille, (size_t)frames, sizeof *permille, cmp_int);
        p50 = permille[(frames - 1) / 2];
        max = permille[frames - 1];
    }
    fprintf(log_file, "frames %d worst_edge %d mismatch_p50 %d mismatch_max %d\n", frames, worst_edge, p50, max);
    fclose(log_file);
    log_file = NULL;
}
