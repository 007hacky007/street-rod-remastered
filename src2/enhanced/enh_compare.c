/* The enhanced view's check against the original frame (enh_compare.h). */
#include "enhanced/enh_compare.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *log_file;
static bool log_rows;              /* SR_ENH_COMPARE_ROWS: the rows whose edges differ by more than 1 px */
static bool rows_on;               /* enh_compare_log_rows: this pass's rows are logged */

static bool is_road(u8 c, const u8 *road, int n)
{
    for (int i = 0; i < n; i++)
        if (road[i] == c) return true;
    return false;
}

static bool road_at(const u8 *row, const u8 *cov, const u8 *road, int n, int x)
{
    return cov[x] && is_road(row[x], road, n);
}

/* Whether the covered road run of row through x (a road pixel) is at least ENH_EDGE_MIN_RUN long. */
static bool long_run_at(const u8 *row, const u8 *cov, const u8 *road, int n, int x)
{
    int a = x, e = x;
    while (a > 0 && road_at(row, cov, road, n, a - 1)) a--;
    while (e + 1 < 320 && road_at(row, cov, road, n, e + 1)) e++;
    return e - a + 1 >= ENH_EDGE_MIN_RUN;
}

/* The first and last pixel of a row's covered road runs that are at least ENH_EDGE_MIN_RUN long, or that overlap
 * such a run of the other picture's row (beside Mulholland's truss bars a 3-px run in one picture is 2 px in the
 * other); false if none. */
static bool edges(const u8 *row, const u8 *other, const u8 *cov, const u8 *road, int n, int *l, int *r)
{
    *l = -1;
    for (int x = 0; x < 320;) {
        if (!road_at(row, cov, road, n, x)) { x++; continue; }
        int e = x;
        while (e + 1 < 320 && road_at(row, cov, road, n, e + 1)) e++;
        bool keep = e - x + 1 >= ENH_EDGE_MIN_RUN;
        for (int i = x; !keep && i <= e; i++)
            keep = road_at(other, cov, road, n, i) && long_run_at(other, cov, road, n, i);
        if (keep) { if (*l < 0) *l = x; *r = e; }
        x = e + 1;
    }
    return *l >= 0;
}

void enh_compare_rows(const u8 *orig, const u8 *enh, const u8 *covered, int y0, int y1, const u8 *road_colours,
                      int n_road, EnhCompare *out)
{
    EnhCompare c = { 0 };
    for (int y = y0; y < y1; y++) {
        const u8 *o = orig + (size_t)y * 320, *e = enh + (size_t)y * 320, *k = covered + (size_t)y * 320;
        c.rows++;
        for (int x = 0; x < 320; x++)
            if (k[x]) { c.pixels++; if (o[x] != e[x]) c.mismatched++; }
        int ol, or_, el, er;
        if (edges(o, e, k, road_colours, n_road, &ol, &or_) && edges(e, o, k, road_colours, n_road, &el, &er)) {
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
            if (log_rows && rows_on && log_file && d > 1) {
                fprintf(log_file, "  row %02X orig %d..%d enh %d..%d\n", y, ol, or_, el, er);
                if (d > 5) {                                /* the colours around the right edges (hex; . = not covered) */
                    int a = SDL_max(0, SDL_min(or_, er) - 3), b = SDL_min(319, SDL_max(or_, er) + 3);
                    fprintf(log_file, "    orig ");
                    for (int x = a; x <= b; x++) fputc(k[x] ? "0123456789abcdef"[o[x] & 15] : '.', log_file);
                    fprintf(log_file, "\n    enh  ");
                    for (int x = a; x <= b; x++) fputc(k[x] ? "0123456789abcdef"[e[x] & 15] : '.', log_file);
                    fprintf(log_file, "\n");
                }
            }
        }
    }
    *out = c;
}

/* Per road mode: the frames and their statistics. */
typedef struct {
    bool seen;
    int frames, skipped, shown, worst_edge, m_worst_edge, cov_min, geo_max, obj_max;
    int *pm, *m_pm, cap;           /* every frame's mismatch (front, glass), permille, for the median */
} ModeStats;
static ModeStats modes[256];
static int frames, skipped, shown, last_mode;

static ModeStats *mode_of(int mode) { return &modes[mode & 0xFF]; }

static int permille(int bad, int all) { return all ? (int)((1000LL * bad + all / 2) / all) : 0; }

bool enh_compare_open(void)
{
    const char *p = SDL_getenv("SR_ENH_COMPARE");
    if (!p || !*p) return false;
    log_file = fopen(p, "w");
    log_rows = SDL_getenv("SR_ENH_COMPARE_ROWS") != NULL;
    rows_on = false;
    for (int m = 0; m < 256; m++) { free(modes[m].pm); free(modes[m].m_pm); modes[m] = (ModeStats){ 0 }; }
    frames = skipped = shown = last_mode = 0;
    return log_file != NULL;
}

bool enh_compare_on(void) { return log_file != NULL; }

void enh_compare_log_rows(bool on) { rows_on = on; }

void enh_compare_frame(int mode, const EnhCompare *c)
{
    if (!log_file) return;
    ModeStats *m = mode_of(mode);
    int pm = permille(c->mismatched, c->pixels), mpm = permille(c->m_mismatched, c->m_pixels);
    if (m->frames == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 256;
        m->pm = realloc(m->pm, sizeof *m->pm * (size_t)m->cap);
        m->m_pm = realloc(m->m_pm, sizeof *m->m_pm * (size_t)m->cap);
    }
    m->pm[m->frames] = pm;
    m->m_pm[m->frames] = mpm;
    if (!m->frames || c->mirror_cov < m->cov_min) m->cov_min = c->mirror_cov;
    m->frames++;
    m->seen = true;
    if (c->edge_err_max > m->worst_edge) m->worst_edge = c->edge_err_max;
    if (c->m_edge_err_max > m->m_worst_edge) m->m_worst_edge = c->m_edge_err_max;
    if (c->geo_err > m->geo_max) m->geo_max = c->geo_err;
    if (c->obj_err > m->obj_max) m->obj_max = c->obj_err;
    frames++;
    last_mode = mode;
    fprintf(log_file,
            "frame %d mode %d rows %d edge_rows %d edge_err_max %d mismatch %d mirror_rows %d mirror_edge_rows %d "
            "mirror_edge_err_max %d mirror_mismatch %d mirror_cov %d geo_err %d obj_err %d police %d opp_sprite %d\n",
            frames, mode, c->rows, c->edge_rows, c->edge_err_max, pm, c->m_rows, c->m_edge_rows, c->m_edge_err_max,
            mpm, c->mirror_cov, c->geo_err, c->obj_err, c->police, c->opp_sprite);
    fflush(log_file);
}

void enh_compare_skipped(int mode)
{
    if (!log_file) return;
    ModeStats *m = mode_of(mode);
    m->seen = true;
    m->skipped++;
    skipped++;
    last_mode = mode;
}

void enh_compare_race(int mode)
{
    if (!log_file) return;
    fprintf(log_file, "race %d\n", mode);
    fflush(log_file);
}

void enh_compare_shown(void)
{
    if (!log_file) return;
    mode_of(last_mode)->shown++;
    shown++;
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

/* the median (sorted[(n - 1) / 2]) and the largest of n values, sorted in place */
static void p50_max(int *v, int n, int *p50, int *max)
{
    *p50 = *max = 0;
    if (!n) return;
    qsort(v, (size_t)n, sizeof *v, cmp_int);
    *p50 = v[(n - 1) / 2];
    *max = v[n - 1];
}

void enh_compare_close(void)
{
    if (!log_file) return;
    for (int i = 0; i < 256; i++) {
        ModeStats *m = &modes[i];
        if (!m->seen) continue;
        int p50, max, mp50, mmax;
        p50_max(m->pm, m->frames, &p50, &max);
        p50_max(m->m_pm, m->frames, &mp50, &mmax);
        fprintf(log_file,
                "mode %d frames %d skipped %d worst_edge %d mismatch_p50 %d mismatch_max %d mirror_worst_edge %d "
                "mirror_mismatch_p50 %d mirror_mismatch_max %d mirror_cov_min %d geo_max %d obj_max %d shown %d\n",
                i, m->frames, m->skipped, m->worst_edge, p50, max, m->m_worst_edge, mp50, mmax,
                m->frames ? m->cov_min : 0, m->geo_max, m->obj_max, m->shown);
    }
    fprintf(log_file, "frames %d skipped %d shown %d\n", frames, skipped, shown);
    fclose(log_file);
    log_file = NULL;
}
