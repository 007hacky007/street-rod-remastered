# Enhanced version — design and status

The faithful engine (see `ENGINE.md`) keeps running unchanged: game flow, garage, the race logic and physics,
the original road renderer, the dashboard and all the other screens, at the game's own pace. `src/enhanced/`
records what each race frame is built from and draws the road view and the rear-view mirror again on the host,
for every displayed frame. The result is laid over the EGA picture. This is the approach of Test Drive II and
III Enhanced (TD2E, TD3E).

The goal of this version is **higher resolution and a longer draw distance** for the racing view. Smooth motion
comes along with it: at 8 frames a second, a high-resolution picture only makes the stepping more visible.

## Status (0.2.0)

Stage 1 is done; stage 2 (the longer draw distance) is not started.

* Done: the scaled output frame and the coverage (§2), smooth motion, the road, sky and horizon, the road-side
  objects, signs, banners, town walls, the opponent and the police car, the rear-view mirror and the dashboard's
  side mirror, the town cruise, the options and the launcher's Picture box.
* Still the original's picture, laid over the enhanced view: the cockpit (A-pillar, hood), the drag race's start
  light, the messages, and the dashboard below the split.
* **The side mirror** (`enh_mirror.c`, from bsvdoom's change to upstream streetrod-enhanced): the original copies
  the rear-view mirror's (248, 22) 32 x 19 to (8, 103) below the split while it draws the road; `enh_compose`
  copies the enhanced rear-view mirror there at the output resolution after the resolve. While a dialog of the
  UI stack covers the rear-view mirror, the side mirror keeps its last copy; a dialog over the side mirror
  itself is left alone. After the road view the original's own copy shows again (the crash, the ticket).
* Changes from the plan below, made while testing:
  * **Coverage by layer.** `road_draw_frame` marks what it draws: `enh_draw_begin` / `enh_draw_fg` switch the EGA
    model between a background layer (replaced by the enhanced view) and a foreground layer (kept). The
    foreground takes only the pixels a write changed, because masked blits rewrite whole bytes. The frame is
    marked presented *before* `ega_show_page`, since the host presents during its retrace wait.
  * **Motion from the physics.** In a race the cars move in `race_phys_step`, every 12th tick, not once per
    frame (9 ticks). Their road positions are interpolated between physics steps (`enh_phys_step`); the
    camera's lateral position and heading between frames.
  * **Sky scroll.** The sky and the horizon panorama scroll an eighth of the original's rate (`SKY_SCROLL`):
    tied to the heading as in the original, they raced across the screen when steering.
  * **Largest sizes only.** Road-side shapes always use the most detailed of the original's sizes, scaled by
    depth; signs use the largest pre-scaled picture; the cars their native pictures, scaled to fit the
    original's shrunk views (at most 1.3 times the native size).
  * **Mirror objects** stand at their true segment (the original draws them one segment off).
* **Worker threads** (`common/enhanced/enh_bands.c`, both games; after bsvdoom's change to upstream
  streetrod-enhanced): the drawing calls record a command list for the frame instead of writing the sample buffer;
  `enh_resolve` runs it in horizontal bands, one per logical core (SDL threads woken by semaphores, the calling
  thread takes the first band). Each band runs every command clipped to its own sample rows, in the order
  recorded, then resolves its output rows; bands never share a row, so nothing is locked and the frame is the one
  a single thread draws, byte for byte. A command keeps the clip and the palette of its call and a copy of its
  arguments (an image's header by value); the image cache and the sample buffer flush the list before they free
  anything a command may read, and Street Rod 2's check flushes before it reads the samples. A full list (warn
  once) is run and emptied mid-frame; a worker that cannot start leaves fewer bands, none leaves the direct
  drawing. The workers end before `SDL_Quit` (a host shutdown handler) or at exit. One band draws directly:
  `SR_ENH_THREADS=N` sets the bands (1: the single-threaded drawing), and the remaster's self-test and palette log
  keep it so (their counts and log in drawing order). `SR_ENH_THREADS_CHECK=1` draws each composed frame both ways
  and compares the sample buffers and resolved frames (a line on stderr; `tests/test_enh_threads.sh`). Median
  drawing and resolve time (`SR_ENH_TRACE`) at 1280 x 800, aa 2, on 10 cores, one band / all: Street Rod 1's demo
  race 4.2 / 0.9 ms, with an identity art pack 19.3 / 4.4 ms.
* Not done yet: mip levels for far sprites, the developer aids of §5 beyond `SR_ENH_TRACE`.

References:

* RR is `../StreetRod/port/spec/race_render.md`.
* Line numbers refer to the code as of commit 8cf0ee1.

## 1. What the original gives us

* **The whole road is in memory.** Segment 389b holds five u16 arrays of 1120 entries each: L, R, YAW, C and
  HGT (RR §3.1, flag bits in §5.2).
  * Segments 0–179 are the town and 180–1119 the race road. A race starts at 200.
  * The finish is at 0x17C for the drag race and 0x3FC for the road race.
  * After the last course feature, the road runs flat with its final heading up to 0x45F
    (`race_track.c:285-293`).
* **The projection has no rotation** (`road_proj_x/y`, `race_road.c:97-112`):
  * `sx = 0xA0 - ((camX - (x + z*dx/4))*4)/z` with `dx = heading - YAW[seg]`
  * `sy = ((y + z*dy - camY)*4)/z + 0x3C` with `dy = cur_height - HGT[seg]`

  It can be evaluated in floating point at any depth, not only at the 15 rows the original uses (z = 4, then
  `12-substep`, then +8 per row).
* **The car state** is in DGROUP:
  * segment `7D30`, sub-step `7D32` (8 per segment), step `7D34`, lateral position `7D36`, heading `7D3C`
  * the opponent or police segment, fraction and lateral position
  * camera x `DS_cam_x`, eye height 0x76C
* **Pacing:** a frame every 9 ticks (about 8.1 fps). The track advance happens once per frame, so the game rate
  stays as it is (`ENGINE.md` timing rule). Smoothness must come from interpolation.
* **Layout:**
  * The view occupies x 0–319, rows 0x12–0x65. The dashboard sits below the split at 0x63.
  * The mirror clip is (0xF2,0x16)–(0x13D,0x2A).
  * The pages are at A200 and A400, flipped in `road_page_flip`.

## 2. Architecture (as in TD3E)

**Files in `src/enhanced/`**

| File | Contents |
|---|---|
| `enhanced.c` | hooks, options, snapshots, compose/coverage |
| `enh_road.c` | road geometry: the segment table turned into a continuous road strip |
| `enh_mirror.c` | the dashboard's side mirror from the enhanced rear-view mirror |
| `enh_raster.c` | sample buffers, polygon fill, resolve, worker bands |
| `enh_sprite.c` | LIB2 picture decode, scaled sprite drawing, mips |
| `enh_internal.h` | shared declarations |

**Hooks** (marked `ENH:` in the engine, calls through `modules.h`)

| Hook | Site | Purpose |
|---|---|---|
| `enh_init()` | `main.c` | parse options; `--classic` skips everything |
| `enh_race_begin()` / `enh_race_end()` | `race_run`, `race_drive.c:225/240` | decode sprites, allocate buffers, overlay on/off; also on quitting a race |
| `enh_frame_built()` | `road_frame`, `race_road.c:257-269`, after the projection | snapshot of the car, opponent and view state, with the tick due time |
| `enh_frame_presented()` | `road_page_flip`, `race_road.c:234-252` | which page is shown; basis for coverage |
| `enh_compose()` | `platform/ega.c` compose | draw the enhanced view and mirror at output resolution over the scaled EGA picture |

**Host changes**

* Raise `HOST_FRAME_MAX_W/H` (`host.h:24-25`) to 320×8 by 200×8. Let `ega.c` register the frame at 320×200 × scale.
* The EGA image is scaled up nearest-neighbour. The enhanced view replaces the pixels in the view window that
  coverage allows.
* While racing, the frame source reports a change on every present, so the host draws at the display rate
  (VSync).

**Coverage.** A pixel in the view window shows the enhanced picture only if all of these hold:

* the page still holds what `road_page_flip` presented;
* no VRAM write has hit it since. This is a per-pixel written mask. TD2E found that comparing values alone lets
  black message boxes show through.
* it is outside the dashboard and the split.

The start light, the "pulled over" and finish messages, and the results screens therefore stay original with no
special cases. The A-pillar (LIB2 #6) and the hood (#7) are redrawn by the enhanced code over its own view, at
their scaled positions, so they don't punch holes in the view.

## 3. Stage 1 — high resolution, smooth motion, original draw distance

1. **Snapshots and interpolation.**
   * Record the continuous position `seg + substep/8`, the lateral position, the heading, the height and the
     opponent's continuous position, each with its tick time.
   * Interpolate one game frame behind (TD3E `--motion-delay 100`), blending with smoothstep across snapshot
     pairs.
   * Snap on discontinuities: race start, reset, a jump of 2 or more segments, a heading step of 16 or more.
   * Road depth is measured from the continuous position, so crossing a segment never shifts the picture.
   * Interpolate YAW and HGT linearly between segments. The original steps them per row.
2. **Road geometry.**
   * For each segment from the car forward, compute the edges in world space: L and R, the outer edges, the
     centre and the dash flags.
   * Subdivide each segment into 4–8 slices with interpolated heading and height.
   * Project in float with the original formula at the output scale (factor `res_scale`). Emit trapezoids far
     to near, in the original's painter's order.
   * Road elements, all from the same flags: asphalt, grass and outer bands; centre dashes (every 4th segment);
     kerbs and sidewalks; the white finish band (`C&0x10`); crossings; town walls (`quad_fill` equivalents).
3. **Raster and anti-aliasing.**
   * Samples are palette indices at `res_scale × aa`; defaults are 1280×800 and 2×2.
   * Resolve in linear light through the current EGA palette.
   * Bands of rows run on a worker pool, as TD3E does.
4. **Sky and horizon.**
   * The sky (LIB2 #5) and the mountains (#267, town #1008) are decoded at native size and scaled.
   * They scroll continuously with the interpolated heading (`heading*2` and `heading*4`, without the
     original's rounding to 8).
   * The horizon y comes from `road_proj_y(0xF0, …)` in float.
5. **Sprites.**
   * Road-side objects (RR §4.11): the originals are hline shapes in 7 fixed sizes. Draw the largest size scaled
     continuously by `1/z`, matched so that the result equals the original size at the original's row depths.
   * Billboards and signs from their largest pre-scaled page image.
   * The opponent and the police from the native LIB2 pictures, not the `pic_shrink_planar` versions. Use the
     same view choice (rear 2–8, alongside 0/1/3) with the same hysteresis, and one world size.
   * Banner posts and the top bar as polygons.
   * Mip levels for downscaling, built from the most frequent pattern and not by averaging indices (TD2E
     `build_mips`).
6. **Mirror.**
   * The same renderer with the mirror projection (centre 0x118, horizon 0x24, reversed signs), 8 segments
     behind.
   * Background LIB2 #9 and #243, clipped to the mirror rectangle.
   * The frame lines are redrawn at output resolution.
7. **Town cruise.** It uses the same renderer and data, so it is enhanced in stage 1. It can be switched off
   separately if it causes trouble.
8. **Options:** `--classic`, `--res-scale 1..8` (default 4) and `--aa 1..4` (default 2), with
   `res_scale × aa ≤ 16`. Also `--motion-delay` (default 100) and `--town-enhanced on|off`.

   Launcher: a "Picture" group with Graphics (enhanced / original), Resolution, Anti-aliasing and Motion.

**Stage 1 acceptance:**

* At draw distance 15, with the time stopped (`TD_ENH_COMPARE`), the enhanced view scaled down to 320×200
  matches the original frame. Road edge rows should be within ±1 px, and objects at the same row and size.
* No regressions in the faithful `--check`.

## 4. Stage 2 — longer draw distance

The road ahead exists in the table, so the geometry comes almost for free. The work is in making it look right.

1. **`--draw-distance N` segments**: default 60, range 15–120.
   * Roads: every segment up to `seg + N`, with fewer slices per segment further away. Far segments whose
     projected width is below a sample get a single slice.
   * `top_band_row` culling is replaced by a horizon clip. Hills that hide the road behind them occlude it
     naturally in painter's order. The original's culling is kept only for the classic view.
2. **Table end.** Never read past 0x45F. The original only reads correctly because the race ends first.
   * Beyond the table, synthesise flat straight road with the final YAW and HGT and no objects.
   * In a drag race, the table past 0x17C holds the rest of the road-race course. Beyond the finish, show plain
     flat road with no objects, so the drag strip doesn't show buildings of a race that isn't being run.
   * The town (0–179) must not show race-road segments: clamp the town view to segment 179, or to the
     destination.
3. **Scenery at distance.**
   * The original draws objects only in rows 1–10 and sizes them for that range. Extend them up to N with the
     same continuous scaling.
   * Fade them in over the last 10% of the distance with a dithered alpha (TD2E).
   * Tiny sprites below 1 sample are dropped.
4. **Opponent and police at distance.**
   * The original hides the opponent at 15 segments or more (`d >= 0x78`). Draw it from its continuous position
     up to N, using the rear view scaled down.
   * **The game logic is unchanged:** the police spawn, the siren at `opp_seg+15` and the pull-over keep their
     distances. Only the picture sees further.
5. **Haze.** `--haze 0..100` (default 30): blend towards the horizon sky colour with smoothstep from about 20
   segments to N, using the extended colours (point 7).
6. **Horizon consistency.** The mountain panorama stays on the horizon line. Far road must not draw above it
   except on crests; clip the far road to below the sky layer where the original would (TD2E pitfall with
   mountains).
7. **Palette.** Extended colours from stage 1 (§3): linear-light mixes of the palette registers, so they
   follow palette changes (TD2E `enh_colours_setup`). Stage 2 uses them for haze.
8. **Mirror distance:** 8 segments in the original, raised to `min(N/2, 30)`. Objects behind the car come from
   the table, which is read-only history, so no ring buffer is needed (unlike TD2E).
9. **Cost.** Measure the render time at 1280×800 2×2 with N = 60 and 120. The target is under 4 ms a frame on
   the worker pool (TD3E: 4.2 ms with its far ring).

**Stage 2 acceptance:**

* On straights, crests, S-bends and the town, with N in {15, 60, 120}:
  * no popping at the far edge;
  * no road drawn over the sky;
  * no reads past the table;
  * the drag race after the finish looks sane;
  * the opponent is visible far ahead and matches the original's picture from 15 segments in.

## 5. Test tooling (from TD2E/TD3E)

* `SR_KEYS` (scripted input) and `SR_SNAPSHOT_DIR` already exist or are ported from TD3E.
* `SR_ENH_COMPARE=1|2`: side-by-side dumps of the original and the enhanced view at the same time, plus a text
  dump of the rows.
* `SR_ENH_TRACE`: per-frame interpolation jerk and delay, and render time.
* `SR_ENH_DRIVER`: scripted drivers (follow, weave, off-road) for repeatable runs.
* `--start/--race` shortcuts straight into a drag or road race.

## 6. Order of work

1. Host: frame size, compose hook, written mask, `--classic` (a no-op picture at first).
2. Snapshots and interpolation, verified with the trace. Nothing drawn yet.
3. Extended palette, road geometry, raster and resolve at draw distance 15, and the compare tool. **Milestone A:** high-resolution
   road.
4. Sky, horizon, cockpit overlays.
5. Sprites: road-side objects, signs, banners, opponent, police.
6. Mirror. Town.
7. Launcher and options. **Release 0.2:** stage 1.
8. Draw distance, table-end handling and far scenery fade.
9. Opponent at distance and haze.
10. Performance pass, launcher "Draw distance" and "Haze". **Release 0.3:** stage 2.

## 7. Risks and lessons carried over

* **Game speed.** Smooth motion made TD2/TD3 feel slow. Here the pace is tied to the frame (9 ticks). Keep it
  faithful first; a `--frame-ticks` option would be a separate decision, since the per-frame advance must
  scale with it.
* **Delayed heading.** Delaying the whole heading makes the view swing into bends. Delay only the steering part,
  and add the road-curve YAW from the interpolated position (TD2E).
* **Shimmer from coverage timing.** Compare against what was actually presented, not against the next frame
  (TD3E bug).
* **Object size discontinuity** when switching between the original's discrete sizes. The continuous scale must
  pass through the original sizes exactly (TD3E `atan` fit).
* **Stale eye-y.** The original keeps a stale eye-y for rows without `C&1` or `C&0xF000`. The enhanced road
  should use the true height, and the compare tool will show the difference. Record it as an intentional
  deviation.

## 8. Decisions

1. Defaults: 1280×800 with 2×2 AA, draw distance 60 and haze 30.
2. Colours: extended colours (linear-light mixes of the palette) from stage 1. Smooth shading where the
   original's colours step, e.g. a sky gradient, is allowed.
3. The town cruise is enhanced in stage 1.
4. Drag race after the finish: plain flat road with no objects.

## 9. Street Rod 2

`src2/enhanced/` gives Street Rod 2 the same stage 1: the same options (`--classic`, `--res-scale`, `--aa`,
`--motion-delay`, `--town`), the same layers, and the drawing core shared with Street Rod 1 (`common/enhanced`:
`enh_raster.c`, `enh_sprite.c`, and `enh_bands.c`'s worker threads: Street Rod 2's own drawing calls, the sky rows
`enh_sheet_row`, `enh_fill_poly_incl`, `enh_line1` and the turned images `enh_draw_image_xf`, record commands
through `enh_submit` too). `SR_ENH_TRACE=<file>` logs each composed frame's mode and drawing time. Median ms at
1280 x 800, aa 2, 10 cores, one band / all, without art and with an identity pack: the drag 4.8 / 1.0 and
19.0 / 12.7, the town drive 4.9 / 1.7 and 18.4 / 3.5, the Aqueduct 5.1 / 2.1 and 6.3 / 1.6, Mulholland
5.9 / 2.2 and 31.9 / 5.6. Done: the town drive (road mode 0), the street drag (1), the Grudge drag strip
(2 and 4), Mulholland (10h), the Aqueduct (8) and the rear-view mirror in each of them: every race kind. Still
the original's: the dashboard's side mirror, and the art until Street Rod 2 has its remastered pictures and
shapes.

* **Road** (`enh_road2.c`): Street Rod 2's own 15 rows, row 0 at depth 4, row k at 8k + 4 - q, row 14 fixed at
  116, with its projection (half Street Rod 1's height term, the moving eye `DS_cam_y`, `DS_view_bounce_y`), its
  edges (the zero-width quirk of `edges_of`) and its bands drawn as the original draws them: straight from row
  to row, walls and storeys filled inclusive of their last row, grass across a plain band after the walls, the
  town walls' storey ends and side streets, the centre dash as wide as the game's per-band table, the road beyond
  a crest hidden as the original hides it. Between frames the position within a segment moves smoothly; the
  snapshot's position is quantised as the projection sees it (`substep >> 3`, 1/8 segment) and the heading kept
  absolute, so at a game frame's own position the result is the original's geometry. Polygons (walls, hillsides,
  rails, posts) are filled as `poly_fill` fills them, edge pixels included (`enh_poly2.c`); in exact mode their
  integer vertices are first clipped by a port of `clip_polygon`. Screen-space constants (hillside peaks, post
  widths, truss thickness) step at the row index in both modes, as the original's do.
* **The Grudge drag strip** (modes 2 and 4): its own flags, flat (no height anywhere in the view), the wide dash
  at the start, and the barrier walls in their 5-segment pink and white blocks.
* **Mulholland** (10h, `enh_hills2.c`): the backdrop (`road_draw_sky_mulholland`: the valley at road level, its
  dy from the climb 4 segments back; below it colour 0Eh), the hillsides as the game's polygons vertex for vertex,
  the shoulders beside the banks, the grass on the flat sides, the row-0 dx clamp. The guard rails, their posts
  and the bridges' trusses are drawn as geometry too: the game's polygons and 1-px lines, the posts' parity from
  the rail phase. A cliff fall (`DS_fallen`) shows the original's picture.
* **Mirror** (`enh_mirror2.c`): a second pass of the same renderer with the mirror's projection, in its glass
  F2h..13Dh x 16h..2Ah; its frame and mount stay the game's pixels. The original's quirks are kept in the exact
  and the smooth view alike, so that its picture stays the original's: rows 5..7 frozen at depth 40, 48, 56
  within a segment (the k == 5 depth freeze), a band's flags one segment off, the banks read forward and YK
  written twice per row, the strip's walls coloured by the front's phase, Mulholland's shoulders not stepped over
  a band, the finish band white only from segment 169h (3ADh in Mulholland). The dashboard's side mirror (the
  page-0 copy of `mirror_finish`) stays the original's picture (Street Rod 1 draws its own from the enhanced
  rear-view mirror).
* **The Aqueduct** (8, `enh_aq2.c`): `aq_project_front`, `aq_project_mirror`, `aq_draw_sky` and `aq_draw_scene`
  ported in one code path, with the game's integer arithmetic (exact) and in floating point (smooth). The front
  rows 12..14 are fixed at depth 100, 108, 116 and the channel ends at row 14; the projection's cam_x is clamped
  to 564h..D5Ch. The roll is a rotation of the projected picture about a pivot (`DS_rot_cx`, `_cy`): exact from
  the game's own sine table, smooth from the interpolated roll and pivot, the 255/256 shrink kept, also at roll 0;
  the opponent turns about the pivot of the pass that draws it (the mirror's when it is behind). The original's
  two constructions are both kept, keyed on the game's roll value: the unrotated sky fill, floor fill and wall
  quads at roll exactly 0, the extended rolled polygons otherwise. The roll, the pivot and the six channel
  deformations are per-frame series. The sky is its fill with the stars and clouds captured from the game's
  drawing (ids 24, 25, the mirror's stars 26), scrolled with the heading and turned with the roll. A roll step of
  more than 60 degrees (wrapped to -180..180) is the bounce's snap back and is not interpolated; the bounce's
  offset (`DS_view_bounce_y`) is kept after it, as the original keeps it. The mirror keeps the original's quirks:
  rows 5..7 at depth 40, 48, 56, XC with +heading, its wall tops from `DS_aq_wall_l_shift` / `_r_shift`, its flags
  `AQM[179h - seg - k]`, the starter at raw row `N - i` of the row it is drawn at.
* **Objects**: not drawn again from the flags. A third display layer (`EGA_LAYER_OBJ`) tags every pixel the game
  draws for an object with its id, one table for the front view, the mirror and the Aqueduct: 1..14 the front
  rows, 15 the opponent (or the police in its slot), 16..22 the mirror's rows 1..7, 23 the mirror's starter
  figure, 24 and 25 the Aqueduct's stars and clouds, 26 its mirror's stars (`ENH_OBJ_IDS` 27). Nearer road drawn
  later clears the tags. When the frame is shown, each id's pixels are cut out of the page and drawn again in the
  band loop at their own segment, moved and scaled from their place in the frame to their smooth place (the
  opponent from its own position and lane, by the place it was drawn at): band, row objects, rails, opponent,
  walls, in the original's order, so nearer bands, walls and hillsides cover farther cuts. All of Street Rod 2's
  object logic (shapes and their sizes, signs, billboards, banners, the start tree, the police) stays the game's
  own, and every drawing routine of the game still runs: the enhanced view only captures. In the Aqueduct a cut is
  rotated as well: it maps from its frame to the view by a similarity about its rotated anchor (the row shapes at
  `aq_draw_row_shape`'s own anchor), turned by the change of its angle, the view's roll or, for the opponent, its
  bank. The opponent's pre-rotated pictures (`opp_bank_variant`) still change where the game changes them; that
  pop is the original's and is accepted.
* **Remastered art** (with `--remaster-dir`): Street Rod 1's enhanced view draws road-side pictures and opponents
  from their source bitmaps; Street Rod 2's captures what the game drew into the page, so the art comes through
  the VRAM tags of the captured pixels, each kept only where its picture has art and the tag's original colour
  is the pixel's (as `attach_art`). The sky and horizon sheets (`enh_sheets_read` / `enh_sheet_row`, also
  Mulholland's backdrop) and the cut objects drawn at their picture's own size (location signs, billboards, the
  start props, the nearest opponent and police views; `capture_objects` keeps their tags in `EnhImage.rm`, taken
  anew at each capture) show the art. The Aqueduct's turned cuts (`enh_draw_image_xf`) take the art the same way.
  Pixels from shrunk bitmaps carry no tags and stay the original's, and so does everything drawn as geometry. The check draws palette
  indices only (`enh_cur_rgb` off while it draws).
* **Check** (`enh_compare.c`): `SR_ENH_COMPARE=<file>` draws every enhanced frame again at scale 1, aa 1, from that
  frame's state without interpolation and with the game's integer rounding (the sky scrolled as `sky_scroll`
  does), and compares it with the page shown, for the front view and for the mirror's glass apart. It measures
  the road's edges per row (from the ground down to the split, in the mirror from its far row; runs of road
  colour shorter than 3 px are not an edge; pixels under the original's foreground and object pixels are not
  measured, and the sky's (the SKY layer, `EGA_TAG_SKY`) are never taken for the road), the mismatched pixels
  (from 12h in every mode), the enhanced exact rows against the game's row tables (front and mirror), and each
  cut's re-projection: the old cut placed in the new frame by the drawing's own mapping against the new cut's
  bottom (the opponent's centre too), for cuts not cut off or partly hidden. The log has a line per frame and a
  summary per road mode. `SR_ENH_COMPARE_ROWS=1` logs the rows that differ, `SR_ENH_COMPARE_DUMP=<dir>` writes the
  original and the enhanced view side by side. In mode 8, where per-row edges cannot measure the shallow,
  rotated lines, a line metric takes their place: the Chebyshev distance of each black and white line pixel to
  the nearest of its colour in the other picture, and in the front view the sky's border as well (the channel's
  far end has no line, only the sky meets the concrete there); the mirror's glass is measured without the
  border. The geometry check compares the rotated row table (the trough included); the opponent's exact anchor
  takes the byte snap of `drv_blit_masked` (`(sx + g_px_shift) & ~7`); a rotated cut is judged only while its
  angle (and for a row its shape group, for the opponent its picture and variant) is unchanged and nothing of
  the foreground lies right below it, the sky's cuts never. Each frame line logs the roll and the bounce. The
  test `sr2_enh_compare` (label `slow`) drives each race kind, the town and the police (`SR_DEBUG_POLICE`), the
  Aqueduct three times (`aqueduct`, `aqking` against the King's rotated pictures, `aqwall` into the wall with
  `SR_DEBUG_AQ_WALL`: bounced, steep, the bounce kept), and judges each mode: road edges (mode 8: the line
  metric) within 1 px in every frame, front and mirror, the median frame within 10 per mille of mismatched
  pixels and every frame within 20, the row tables exactly, the objects within 1 px, the mirror's glass drawn.
