#pragma once
/* enh_raster.c <-> enh_bands.c: a frame's resolve, run band by band. */
#include "enh_draw.h"

typedef struct {
    u32 *xrgb;
    u16 page;
    int y_end;
    float lr[256], lg[256], lb[256];   /* the palette in linear light */
} EnhResolveJob;
/* Output rows r0..r1 (counted from the view's top, res scale per original row) of the frame q describes. */
void enh_resolve_rows(const EnhResolveJob *q, int r0, int r1);
/* Runs the recorded commands and, with q, the resolve, in bands. */
void enh_bands_run(const EnhResolveJob *q);
