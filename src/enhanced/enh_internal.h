#pragma once
/* Shared declarations of src/enhanced. */
#include <SDL3/SDL_stdinc.h>

#include "types.h"

#include "enh_draw.h"

/* The newest frame's state that is not interpolated. */
typedef struct {
    s16 height;                /* DS:7D3E */
    s16 opp_sprite;            /* DS:7D7C after the frame was drawn: the opponent's view */
    bool town;                 /* the town cruise (DS:8ACA = 0) */
} EnhSnap;

/* The smooth view at a moment: the road-curve part of the heading comes from YAW at pos. */
typedef struct {
    double pos, lat, rel_heading, opp_pos, opp_lat;
    s16 height, opp_sprite;
    bool town;
} EnhView;

bool enh_view_at(Uint64 now_ns, EnhView *v);

/* enh_road.c */
void enh_draw_road(const EnhView *v);

/* enh_mirror.c: the dashboard's left side mirror (8, 103) 32 x 19, the original's copy of the rear-view mirror
 * (248, 22), drawn from the enhanced one over xrgb after the resolve; its last copy kept while a dialog covers the
 * rear-view mirror, nothing under a dialog over itself. reset: a new road view. */
void enh_side_mirror_reset(void);
void enh_side_mirror(u32 *xrgb, int res_scale);
