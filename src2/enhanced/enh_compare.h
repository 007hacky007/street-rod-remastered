#pragma once
/* The enhanced view's check against the original frame (spec: Enhanced race view, acceptance). With
 * SR_ENH_COMPARE=<file>, every road frame the enhanced view draws is drawn again at scale 1 and aa 1 from
 * that frame's state, without interpolation, and compared with the shown page: one line per frame and a
 * summary at the end. */
#include "types.h"

typedef struct {
    int rows, edge_rows, edge_err_max, pixels, mismatched;            /* front view, outside the mirror rectangle */
    int m_rows, m_edge_rows, m_edge_err_max, m_pixels, m_mismatched;  /* the mirror's glass */
    int mirror_cov;         /* covered glass pixels: 1596 when the mirror is enhanced, 0 when it is the game's */
    int geo_err;            /* largest |enhanced exact row value - game's row table value| */
    int obj_err;            /* largest re-projection error of a cut's bottom (opponent: and centre x), px; -1 none */
    int police, opp_sprite; /* DS_police_state, DS_opp_sprite */
    int line_px, line_err_max;       /* front: the line metric's pixels and its largest distance (0 outside mode 8) */
    int m_line_px, m_line_err_max;   /* the same over the mirror's glass */
    int roll, bounce;                /* DS_rot_enable of the frame (mode 8; else 0), DS_view_bounce_y */
} EnhCompare;
#define ENH_EDGE_MIN_RUN 3   /* road-colour runs shorter than this are not the road's edge */
#define ENH_LINE_CAP 8        /* the line metric's distance when nothing of the colour is nearer */
#define ENH_MISMATCH_BAD 20  /* SR_ENH_COMPARE_DUMP also dumps a frame with more mismatched pixels (permille) */

/* orig, enh: 320-wide rows of palette indices, rows y0..y1; covered: nonzero where the enhanced view replaces
 * the pixel. A row's road edges are the first and the last pixel of its covered runs of road_colours at least
 * ENH_EDGE_MIN_RUN long, or overlapping such a run in the other picture's row. Fills rows, edge_rows,
 * edge_err_max, pixels and mismatched (the rest 0). */
void enh_compare_rows(const u8 *orig, const u8 *enh, const u8 *covered, int y0, int y1, const u8 *road_colours,
                      int n_road, EnhCompare *out);

/* The line metric (mode 8, where per-row edges cannot measure shallow, rotated lines): every covered pixel of
 * either picture whose colour is one of cols, against the nearest covered pixel of the same colour in the other
 * picture (Chebyshev distance, ENH_LINE_CAP when none is that near); and, when border >= 0, every covered pixel of
 * colour `border` with a covered 4-neighbour (rows y0..y1 - 1) of another colour, against the nearest such border
 * pixel of the other picture. *px: such pixels in orig; *err: the largest distance. Rows y0..y1 - 1 of 320-wide
 * pictures; `covered` is the one mask of both pictures. */
void enh_compare_lines(const u8 *orig, const u8 *enh, const u8 *covered, int y0, int y1, const u8 *cols, int n,
                       int border, int *px, int *err);

/* The log: open reads SR_ENH_COMPARE (false: no comparing); frame appends a line; close writes one summary line
 * per mode seen, modes ascending (frames, skipped, the worst edge error, the median and largest mismatch in
 * permille, the same for the mirror's glass, its least coverage, the largest geometry and re-projection errors,
 * the compositions, the largest line metric distances, front and glass), then the totals. */
bool enh_compare_open(void);
bool enh_compare_on(void);
void enh_compare_frame(int mode, const EnhCompare *c);
void enh_compare_skipped(int mode);    /* a frame the enhanced view did not show (fallen) */
void enh_compare_race(int mode);       /* a race of road mode `mode` starts, enhanced or not ("race <mode>") */
void enh_compare_log_rows(bool on);    /* SR_ENH_COMPARE_ROWS logs rows only while on (the edge pass) */
void enh_compare_shown(void);  /* a composed picture showed the enhanced view (counted under the last frame's mode) */
void enh_compare_close(void);
