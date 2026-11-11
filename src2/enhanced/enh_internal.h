#pragma once
/* Shared declarations of src2/enhanced (Street Rod 2's enhanced view). */
#include <SDL3/SDL_stdinc.h>

#include "types.h"

#include "enh_draw.h"

/* The newest frame's state that is not interpolated. */
typedef struct {
    s16 height;                /* DS_cur_height */
    s16 cam_y;                 /* DS_cam_y (the eye height; it moves in Street Rod 2) */
    s16 bounce;                /* DS_view_bounce_y */
    s16 mode;                  /* DS_road_mode */
    s16 opp_sprite;            /* DS_opp_sprite after the frame was drawn: the opponent's view */
    bool town;                 /* the town cruise (DS_racing = 0) */
} EnhSnap;

/* The smooth view at a moment: the road-curve part of the heading comes from YAW at pos. Positions in
 * segments (substep and the opponent's fraction are in 1/64 segment). */
typedef struct {
    double pos, lat, heading, opp_pos, opp_lat;   /* heading: DS_heading, unwrapped (not relative to the road) */
    s16 height, cam_y, bounce, mode, opp_sprite;
    bool town;
    bool exact;                /* the newest frame itself (the compare tool): the game's own height, not the track's at pos */
} EnhView;

bool enh_view_at(Uint64 now_ns, EnhView *v);
/* The newest frame's view, not interpolated (the compare tool). */
bool enh_view_newest(EnhView *v);

/* enh_road2.c: modes 0 (town) and 1 (street drag). */
void enh_draw_road(const EnhView *v);
/* The road colours of the compare tool's edges. */
extern const u8 enh_road_colours[2];
extern double enh_ground_y;    /* the first row below the horizon strip of the last frame drawn */
