#pragma once
/* The enhanced view's polygons as Street Rod 2's road_poly_fill draws them (clip_polygon, then poly_fill with its
 * edge pixels): the barrier walls, the town's faces, and later the hillsides and the rails. */
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
