#pragma once
/* The enhanced view's polygons as Street Rod 2's road_poly_fill draws them (clip_polygon, then poly_fill with its
 * edge pixels): the barrier walls, the town's faces, Mulholland's hillsides, rails and posts; and its 1-px lines. */
#include "types.h"

/* clip_polygon's Sutherland-Hodgman (race_gfx.c) on the clip xmin..xmax, ymin..ymax (inclusive), in the order xmin,
 * xmax, ymin, ymax, the new vertices by the game's truncating line_y_at_x / line_x_at_y from the previous vertex;
 * 64-point buffers. Returns the count of points in out (0: nothing left). */
int enh_clip_poly_i(const s16 *in, int n, s16 *out, int xmin, int xmax, int ymin, int ymax);
/* poly_fill with its edge pixels: each pixel row is filled from the polygon's leftmost to its rightmost point
 * within the row (its line y - 0.5 .. y + 0.5), plus one pixel: the hull of the vertices' pixel squares when the
 * polygon is convex, and a y-monotone concave one as poly_fill's two vertex chains fill it. x: the pixel's left
 * edge, y: its centre (the original's line y + ROW_TOP). Under the current clip; at a higher resolution each
 * sample row takes the original row around its own centre. */
void enh_fill_poly_incl(const double *x, const double *y, int n, u8 col);
/* clip_line's clip (race_gfx.c) of the line in[0..3] = x1, y1, x2, y2 on xmin..xmax, ymin..ymax (inclusive), the
 * new ends by the game's truncating line_y_at_x / line_x_at_y: false when the game draws nothing. */
bool enh_clip_line_i(const s16 in[4], s16 out[4], int xmin, int xmax, int ymin, int ymax);
/* A line one original pixel thick (sa samples across the minor axis) through the centres of the pixels (x0, y0) and
 * (x1, y1), both ends included: at res 1 the pixels of the game's Bresenham line (ega_line, a tie stepped towards
 * the far end from the left end). x, y: the original's pixel coordinates (the line y, without ROW_TOP). Under the
 * current clip. */
void enh_line1(double x0, double y0, double x1, double y1, u8 col);
/* poly_fill_pts with an edge colour, on a polygon already clipped (game coordinates: x the pixel, y the line y):
 * filled as enh_fill_poly_incl fills it (y + ROW_TOP), then each edge as enh_line1 in `edge`; edge == fill fills
 * only. Under the current clip. */
void enh_fill_poly_edge(const double *x, const double *y, int n, u8 fill, u8 edge);
/* clip_polygon's Sutherland-Hodgman in doubles (no truncation), on xmin..xmax, ymin..ymax in the same order;
 * 64-point buffers. Returns the count of points in ox, oy (0: nothing left). */
int enh_clip_poly_f(const double *x, const double *y, int n, double *ox, double *oy,
                    double xmin, double xmax, double ymin, double ymax);
/* poly_fill itself (race_gfx.c) on a polygon already clipped, integer points {x, y} (y the line y): its two edge
 * walkers' pixels from the top vertex in `edge`, the spans strictly between them in `fill`, as whole original pixels
 * (exact mode: the game's picture pixel for pixel). Under the current clip. */
void enh_poly_fill_i(const s16 *pts, int n, u8 fill, u8 edge);
