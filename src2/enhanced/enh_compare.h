#pragma once
/* The enhanced view's check against the original frame (spec: Enhanced race view, acceptance). With
 * SR_ENH_COMPARE=<file>, every road frame the enhanced view draws is drawn again at scale 1 and aa 1 from
 * that frame's state, without interpolation, and compared with the shown page: one line per frame and a
 * summary at the end. */
#include "types.h"

typedef struct {
    int rows;           /* rows compared */
    int edge_rows;      /* of them, rows where both have a road among their covered pixels */
    int edge_err_max;   /* the largest difference of the road's left or right edge, px */
    int pixels;         /* covered pixels */
    int mismatched;     /* covered pixels whose colour differs */
} EnhCompare;

/* orig, enh: 320-wide rows of palette indices, rows y0..y1; covered: nonzero where the enhanced view replaces
 * the pixel. A row's road edges are its first and last covered pixel whose colour is one of road_colours. */
void enh_compare_rows(const u8 *orig, const u8 *enh, const u8 *covered, int y0, int y1, const u8 *road_colours,
                      int n_road, EnhCompare *out);

/* The log: open reads SR_ENH_COMPARE (false: no comparing); frame appends a line; close writes the summary
 * (frames, the worst edge error, the median and largest mismatch in permille). */
bool enh_compare_open(void);
bool enh_compare_on(void);
void enh_compare_frame(int mode, const EnhCompare *c);
void enh_compare_close(void);
