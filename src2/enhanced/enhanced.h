#pragma once
/* Enhanced road view for Street Rod 2 (ENHANCED.md, its Street Rod 2 section); the same API as Street Rod 1's
 * src/enhanced/enhanced.h. Not bound by the faithful-engine rules in ENGINE2.md.
 *
 * The faithful engine keeps running every race frame at its own pace (road_frame: about 8.1 frames a second,
 * the town cruise too). This module records the car, opponent and view state of each of those frames, draws
 * the road view and the rear-view mirror again on the host at a higher resolution for every displayed frame,
 * with the motion carried smoothly between the game's frames, and lays the result over the EGA picture. The
 * hooks below are called from the engine at the places marked `ENH:`. */
#include "types.h"

#define ENH_DEFAULT_RES_SCALE 4        /* output = 320x200 times this */
#define ENH_MAX_RES_SCALE     8
#define ENH_DEFAULT_AA        2        /* samples per output pixel along each axis */
#define ENH_MAX_AA            4
#define ENH_MAX_SAMPLES       16       /* res scale x aa at most (samples per original pixel, each axis) */
#define ENH_DEFAULT_MOTION_DELAY 100   /* percent of a game frame the smooth view runs behind the game: 100 = it
                                          only interpolates between frames it has */

/* main.c: enabled = false is --classic (the original's picture, scaled by res_scale). res_scale 1..8,
 * aa 1..4, motion_delay 0..100, town = the town cruise is enhanced too. */
void enh_init(bool enabled, int res_scale, int aa, int motion_delay, bool town);
bool enh_enabled(void);
int  enh_res_scale(void);

/* race_run (race_drive.c): the road view starts (before road_race_init) and ends (the frame loop left). */
void enh_race_begin(void);
void enh_race_end(void);

/* road_frame (race_road.c), after the projection: the frame's state becomes the newest snapshot. */
void enh_frame_built(void);
/* race_phys_step (race_phys.c), after player_advance / opp_advance_steer: the cars' positions along the
 * road moved (every 12th tick in a race, not in step with the frames). */
void enh_phys_step(void);

/* road_draw_frame (race_road.c): what the engine draws next into the back page is replaced by the enhanced
 * view (bg: sky, ground, road bands) or stays the original's picture (fg: road-side objects, the opponent,
 * the town walls, the mirror, the cockpit), until the enhanced renderer draws those too. begin = a new
 * frame into the back page (all its marks cleared, background). */
void enh_draw_begin(void);
void enh_draw_fg(bool fg);
/* road_draw_front, road_draw_sky: on = the sky's blits follow (the SKY layer: background, every pixel tagged as
 * the sky's for the compare tool); off = back to the base layer. */
void enh_draw_sky(bool on);
/* road_draw_front: what the engine draws next is an object of row `id` (1..14), or the opponent
 * (ENH_OBJ_OPP); 0 = back to the base layer (the background after enh_draw_begin, the foreground after
 * enh_draw_fg(true)). The enhanced view draws those pixels again at the smooth position of their row (or of the
 * opponent). One id table for the front view, the mirror and the Aqueduct: */
#define ENH_OBJ_OPP      15          /* the opponent, or the police in its slot */
#define ENH_OBJ_MIR(r)   (15 + (r))  /* mirror rows 1..7: 16..22 */
#define ENH_OBJ_STARTER  23          /* the starter figure in the mirror (its y pinned, starter_draw_mirror) */
#define ENH_OBJ_STARS    24          /* part C: the Aqueduct's stars */
#define ENH_OBJ_CLOUDS   25          /* part C: its clouds */
#define ENH_OBJ_IDS      26
void enh_draw_objects(int id);
/* road_frame: the rectangle x0..x1, y0..y1 (inclusive) of the back page stays the original's picture. */
void enh_mark_fg(int x0, int y0, int x1, int y1);

/* road_page_flip (race_road.c): the frame just drawn is now the shown page. */
void enh_frame_presented(void);

/* platform/ega.c compose: the EGA picture has been scaled into xrgb (320*S x 200*S); lay the enhanced view
 * over it. rgb = the 16 palette registers as XRGB. Returns true while the enhanced view is shown (the
 * picture changes with time even when the game draws nothing). */
bool enh_compose(u32 *xrgb, const u32 rgb[16]);
