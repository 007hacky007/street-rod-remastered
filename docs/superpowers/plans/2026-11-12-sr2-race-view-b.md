# Street Rod 2 enhanced race view, part B Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The rear-view mirror (road modes 0, 1, 2, 4 and 10h), the Grudge drag strip (modes 2 and 4) and Mulholland (10h, with its hills, hillsides, guard rails, bridges and the police) are drawn by the enhanced race view. The compare tool checks every race kind and the mirror, mode by mode. The Aqueduct (mode 8) stays the original picture until part C.

**Architecture:**
- Part A's design stays: the game draws every frame as before. The enhanced view redraws the background (sky, ground, road, walls, hillsides, rails) from the race data, and it re-places the objects the game drew. Those are captured by their tag layer and re-projected.
- The view gets more state per game frame: the eye, the height offset, the bounce, and channels reserved for part C. Each cut keeps its absolute segment and is drawn inside the band loop at its own row: band, row objects, rails, opponent, walls, as the original orders them.
- The mirror becomes a second pass of the same renderer with the mirror's own projection. It reproduces the original's quirks.
- The compare tool gains per-mode statistics, a mirror metric over the glass, a geometry check against the game's row tables, a re-projection check for the cuts, and a sky tag layer. With these it can judge Mulholland, whose backdrop has the road's colours.

**Tech Stack:** C11, SDL3; tests in C (`tests/test_sr2_enh.c`), POSIX sh (`tests/test_sr2_enh_compare.sh`); the launcher in C++17/wxWidgets (`tests/test_args.cpp`).

**Spec:** `docs/superpowers/specs/2026-11-11-street-rod-2-design.md`, section "Enhanced race view". Part A's plan, for the existing design: `docs/superpowers/plans/2026-11-11-sr2-race-view-a.md`. `ENHANCED.md` section 9 describes part A's state.

## Global Constraints

- Street Rod 1 does not change. After every task, `tools/sr1-codecheck.sh 50331e9 HEAD --allow host.c.o` must report `0 differ`. Nothing in `src/` or `common/` is edited in part B. New helpers live in `src2/enhanced/`.
- Every game drawing routine keeps running. The enhanced view only captures, never skips the original's drawing: drawing sets game state (`DS_rock_hit`, `DS_obstacle14_hit`, the opponent's sprite state).
- The rear-view mirror is enhanced in every race kind part B enables (modes 0, 1, 2, 4, 10h). The dashboard's side mirror (the page-0 copy made by `mirror_finish`) stays the original's picture, as in Street Rod 1.
- The original's mirror quirks are reproduced in both exact and smooth mode:
  - the k == 5 depth freeze (rows 5..7 at 40/48/56);
  - flags one segment off (seg - r - 1);
  - banks read forward, and the YK quirk;
  - the Grudge wall phase;
  - Mulholland's unstepped shoulders and its row-0 dx clamp;
  - the finish band white only at DS_seg >= 169h (3ADh in Mulholland).
- Acceptance (spec), measured with time stopped (exact mode, `SR_ENH_COMPARE`), at scale 1:
  - road edges within +-1 px in every compared frame;
  - the median frame within 10 per mille of mismatched pixels;
  - objects at the same row (re-projected cut bottoms within 1 px);
  - all of this for each race kind and for the mirror, per mode in the compare log.
- Remastered `SR2_SHP` shapes are not part of B or C. Cuts keep their `EnhImage.rm` field (NULL) so art can be carried later. Nothing reads `rm_vtag`.
- The Aqueduct (mode 8), its front view and its mirror stay original in part B. The `aqueduct` compare run must log 0 mode-8 frames.
- Original draw distance; no gameplay change. The new developer aid `SR_DEBUG_POLICE` (Task 8) changes nothing when it is unset.
- Lines added to `src2/game/` carry `ENH:` (hooks) or `PORT: developer aid` (the aid).
- No macOS username, no real name or email, no AI attribution, ASCII hyphen-minus only, in anything written. Commit messages: one short imperative subject under about 60 characters, no body, no trailers.
- Work on `master` directly. Never push. Never touch `.worktrees/`: another track works on branch `sr2-art` there.

## Review Focus

1. A frame where the game draws no new front view must show the original picture, never a stale enhanced view. Two cases: after a Mulholland cliff fall (`DS_fallen`: `road_draw_front` returns before `enh_draw_begin`), and when a message box is drawn over the shown page. (Task 1 `sr2_enh_fallen_not_composed`; Task 4: the compare counts fallen frames as `skipped`. Message boxes stay part A's written mask, unchanged.)
2. The opponent or the police crossing between the front view and the mirror must show in one of them only, and leave no stray pixels in the front view. This covers `opp_seg == seg`, the 2 <-> 9 sprite hysteresis and Mulholland's sticky `d`. (Task 2 `sr2_enh_capture_regions`; Task 5 `sr2_enh_mirror_opp_geo`; Task 8: the `police` run's `obj_max <= 1`.)
3. The road beyond a crest must stay hidden as the original hides it (`top_band_row`), in every race mode. Between frames, objects on a slope must stay on the road. (Task 3 `sr2_enh_top_band_row`; Task 1 `sr2_enh_obj_geo_exact_slope`; Task 4's geometry check `geo_max 0`.)
4. A long race must not stutter when the heading's unwrapped value leaves the s16 range. (Task 1 `sr2_enh_heading_wrap`.)
5. A run that switches race kinds must carry no state from one race into the next: cuts, the base layer, the image and sheet caches, the rail phase. The Grudge run goes town, qualifier, four rounds, town; Mulholland goes race, then home. (Task 2 `sr2_enh_race_begin_resets`; Task 4 onward: the `grudge` run checks every one of its modes.)

## Decisions this plan takes (owner rulings are in Global Constraints)

- **One series, grown once:** `ENH_SER_N` = 16 channels:
  - lat, heading, pos, opp_pos, opp_lat;
  - cam_y, hoff (= cur_height - HGT[seg]), bounce;
  - roll, aq_dy and six Aqueduct deformations, which part C fills (0 in B).
  - `enh_ser_snap` lets a channel jump without interpolation. Part C uses it for the roll at a bounce.
- **One object-id table, for B and C:**
  - 1..14 front rows;
  - 15 the opponent;
  - 16..22 mirror rows 1..7;
  - 23 the mirror's starter figure;
  - 24 the Aqueduct's stars, 25 its clouds (part C);
  - `ENH_OBJ_IDS` 26;
  - the sky tag `EGA_TAG_SKY` = F0h.
- **Opponent geometry from what was drawn (critic D3).** The cut's frame geometry uses `DS_opp_last_d` and `DS_opp_last_sx` after the frame. The smooth view moves `d` on by the cars' motion since then. Contact and result-3 placements are fixed, like the side views 0 and 1.
- **Cut order:** cuts are drawn in the band loop at their own segment, so nearer bands, walls and hillsides cover farther cuts. This replaces part A's depth sort and fixes the deferred minor 1.
- **The mirror's frame and mount stay foreground** (the game's pixels). Only the glass F2h..13Dh x 16h..2Ah is enhanced. "The mirror rectangle" below means the mirror's own pixels: its body F0h..13Fh x 14h..2Ch and its mount 110h..11Fh x 12h..13h (`mirror_draw_backdrop`). Rows 12h..13h beside the mount are the front view's sky and belong to the front pass, its cuts and its statistics.
- **Dash:** the original's per-band table width in every mode. Exact mode uses `DS_dash_width[k]`. Smooth mode interpolates `lerp(t[k], t[k-1], q/8)`. Part A's mean world width (`dash_half`) goes.
- **Exact sky:** in exact mode the race sky and horizon scroll as `sky_scroll` does, and the horizon row is the game's integer `road_proj_y(F0h, 2000, dy)` with `road_draw_sky`'s clamp (part A's float horizon is 2/3 px off and puts a whole sky row on the wrong side). The compare then counts mismatches from 12h in every mode. The smooth town/drag sky keeps `SKY_SCROLL` 1/8. The smooth Mulholland backdrop scrolls at the original's 2 px per heading unit, not quantised, because the backdrop holds the valley at road level.
- **Screen-space constants step at the row index** in both modes, as the original does: hillside peaks, post widths, truss thickness.
- **Polygons** (barrier walls, hillsides, rails, posts):
  - one rasteriser, inclusive of poly_fill's edge pixels (`enh_fill_poly_incl`): each pixel row is filled from the leftmost to the rightmost point of the polygon within that row's band (y - 0.5 .. y + 0.5 around the original's line y), plus one pixel. For a convex polygon this is the inclusive hull; for the y-monotone but concave hillside poly6 it follows the polygon as poly_fill's two vertex chains do, where a convex hull would overfill;
  - in exact mode the integer vertices are first clipped by a port of `clip_polygon`.
- **Lines** (truss, rail edges): lines one original pixel thick (`res_scale` output pixels; a parallelogram along the minor axis) through pixel centres with inclusive ends, drawn n times with the original's offsets.
- **Below Mulholland's backdrop** (fewer than 53h rows): colour 0Eh. The original shows the stale page there, at most row 63h, under the hood.
- **Edges in the compare:** runs of covered road-colour pixels shorter than 3 px are ignored. This fixes the metric's half of the location-4 return failure. Its cause, in the frame-148 dump: the original leaves a 2-px gap of town ground (colour 1) at row 4Ah, between the crossing's kerb (stepped by `slope16`) and the facade (Bresenham), and the metric read that gap as the road. The other half is geometry: in that dump the enhanced facade starts at x 308 and the original's at 311 (the mapping report). Task 4 checks it and fixes it if it is more than 1 px; the owner ruled the failure fixed, not hidden.
- **The SKY tag in the edge pass:** pixels the game drew in the SKY layer count as covered but never as road (both images' colours are replaced by a non-road value there for the edge pass). So the backdrop beside a road (Mulholland's bridges, the town mirror's unfilled road) does not hide the edge from the visibility test, and the backdrop's own colour-1 pixels are never taken for the road. Object-tagged pixels stay uncovered for the edges, as in part A.

## File map

| File | Responsibility |
|---|---|
| `src2/enhanced/enhanced.c` | options, snapshots, series, hooks, layers, capture, cut drawing (`enh_cuts_at`), compose, compare plumbing |
| `src2/enhanced/enh_internal.h` | shared declarations (series, view, snapshot, row tables, cut kinds, regions) |
| `src2/enhanced/enh_geo2.h` (new, Task 5) | the Cam, rows, track access and the four projections (front and mirror, exact and smooth) |
| `src2/enhanced/enh_road2.c` | the front pass: sky, bands, town walls, strip walls, `enh_obj_geo`, `enh_rows_exact_front` |
| `src2/enhanced/enh_mirror2.c` (new, Task 5) | the mirror pass: backdrop, mirror bands and walls, `enh_rows_exact_mirror` |
| `src2/enhanced/enh_hills2.c` (new, Task 7) | Mulholland: backdrop, hillsides (front, mirror), guard rails and trusses (Task 8) |
| `src2/enhanced/enh_poly2.c`, `enh_poly2.h` (new, Task 6) | integer polygon clip, inclusive fill, 1-px lines |
| `src2/enhanced/enh_compare.c`, `.h` | compare rows, per-mode log |
| `src2/platform/ega.c`, `ega.h` | the SKY layer, `ega_mark_fg_rect` |
| `src2/game/race_road.c`, `race_phys.c` | `ENH:` hooks, the `SR_DEBUG_POLICE` aid |

Line numbers below are those of HEAD 50331e9.

---

### Task 1: The frame's state for parts B and C

**Files:**
- Modify: `src2/enhanced/enhanced.c`:
  - the series (`Series` becomes `EnhSeries`, :21-86);
  - `enh_frame_built` (:128-144) and `enh_frame_presented` (:256-268);
  - `view_of` (:270-285).
- Modify: `src2/enhanced/enh_internal.h` (EnhSnap, EnhView, the new declarations)
- Modify: `src2/enhanced/enh_road2.c`:
  - one `cam_of` for both `enh_draw_road` (:291-311) and `enh_obj_geo` (:315-349);
  - the sheets read once per frame (:120-148).
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Produces, in `enh_internal.h`:
  ```c
  enum { ENH_F_LAT, ENH_F_HEADING, ENH_F_POS, ENH_F_OPP_POS, ENH_F_OPP_LAT, ENH_F_CAM_Y, ENH_F_HOFF, ENH_F_BOUNCE,
         ENH_F_ROLL, ENH_F_AQ_DY, ENH_F_DEF0, ENH_SER_N = ENH_F_DEF0 + 6 };   /* DEF0..+5: def_floor_h, def_xl, def_xr, def_top_h, def_top_xl, def_top_xr */
  typedef struct { Uint64 t[3]; double v[3][ENH_SER_N]; int n; double period; } EnhSeries;
  void enh_ser_push(EnhSeries *s, Uint64 t, const double *v, double nominal_ns);
  void enh_ser_at(const EnhSeries *s, Uint64 now, double *out);
  void enh_ser_snap(EnhSeries *s, int k);       /* channel k jumped: the older samples take its new value */
  s16 enh_s16_of(double h);                     /* (s16)(s32)floor(h): wraps, never undefined */
  double enh_unwrap(double prev, s16 prev_raw, s16 raw);   /* prev + (s16)(raw - prev_raw) */
  typedef struct { s16 xl[15], xr[15], y[15], xol[15], xor_[15], ye[15], yk[15], ybl[15], ybr[15]; } EnhRowTab;
  void enh_rowtab_read(EnhRowTab *t);           /* DS_row_xl, _xr, _y, _xol, _xor, _yeye, _ykerb, _ybank_l, _ybank_r now */
  ```
  - `EnhView` drops `height`. It gains:
    - `double cam_y, hoff, bounce, roll, aq_dy, aq_def[6], opp_d;`
    - `s16 opp_seg, opp_sx, fall_dir, start_seg, rail_phase, ground_top_y;`
    - `bool opp_fixed;`
  - `opp_d` is the opponent's depth in 1/64 segment. In the newest view it is `DS_opp_last_d` after the frame. In a smooth view it is that value plus `64 * ((opp_pos - pos) - (newest opp_pos - newest pos))`.
  - `EnhSnap`, filled at presentation:
    - `mode`, `town`;
    - `opp_sprite`, `opp_seg`, `opp_d` (`DS_opp_last_d`), `opp_sx` (`DS_opp_last_sx`);
    - `opp_fixed`: sprite 0 or 1, or `DSW(DS_opp_contact)`, or `PHW(PLAYER, PH_RESULT) == 3`;
    - `fall_dir` (`DS_fall_dir`), `fallen` (`DS_fallen`);
    - `rail_phase` (`DS_rail_phase_seg`), `start_seg` (`DS_start_seg`);
    - `ground_top_y` (`DS_ground_top_y`), `top_band_row` (`DS_top_band_row`);
    - `police` (`DS_police_state`);
    - `game_front` (`EnhRowTab`, read at `enh_frame_built`: the mirror overwrites rows 0..7 and all of YK later);
    - `game_mirror` (read at `enh_frame_presented`).
  - `void enh_sheets_read(void);`: page 1 rows 66h..C7h into the cache. It is called once per presented frame. `sheet_row` clamps to C7h.
  - `enh_obj_geo(v, seg, sprite)` keeps its signature in this task. Task 2 changes it.

- [ ] **Step 1: Write the failing tests** in `tests/test_sr2_enh.c`. `view_at` fills the new fields: `cam_y` 0x76C, the rest 0.
  - `sr2_enh_series_snap`: push three samples 10 ms apart (nominal period 10 ms), channel 0 = 0, 10, 20 and channel 1 = 0, 0, 100, then `enh_ser_snap(&s, 1)`. With `motion_delay` 100, `enh_ser_at` halfway after the newest gives channel 0 strictly between 10 and 20, and `out[1] == 100` exactly.
  - `sr2_enh_heading_wrap`:
    - `enh_unwrap(32760.0, 32760, (s16)0x8002) == 32770.0`;
    - `enh_s16_of(32770.0) == (s16)0x8002`;
    - `enh_s16_of(-40000.5) == (s16)(s32)floor(-40000.5)`;
    - the real path: inside an active mode-1 race at a fixed `DS_seg`, `enh_frame_built` with `DS_heading` 7FF0h, 7FF8h, (s16)8000h, (s16)8008h in turn leaves `enh_view_newest`'s heading at 32776.0 (unwrapped, no reset). Part A's check compares the raw s16 with the unwrapped value, resets, and gives -32760.
  - `sr2_enh_obj_geo_exact_slope`:
    - Setup:
      - `TRKW(TRK_HGT, i) = 2 * i` for i 90..120 (restore after);
      - `DSS(DS_cam_y) = 0x76C`, `DSS(DS_view_bounce_y) = 0`, `DSS(DS_road_mode) = 1`;
      - an exact view at pos 100.875 with `hoff` 0.
    - Checks:
      - the cut at seg 105 has `g.yg == road_proj_y(37, 2000, (s16)(200 - 210)) + 0.5`, where 37 = 8 * 5 + 4 - 7;
      - smooth views at 100.99 and 101.01 give `|yg| differences < 0.5`.
  - `sr2_enh_opp_drawn_d`: exact view with `opp_pos = pos + 10` (deliberately inconsistent), sprite 5.
    - `opp_d` 300 gives `z == 41`;
    - sprite 2 with `opp_d` -20 gives `z == 6`;
    - sprite 2 with `opp_d` -40 gives `z == 7`;
    - two smooth views, `opp_d` 300 and 292 (the car 1/8 segment on), give `z` 41.5 and 40.5 for sprite 5 (smooth mode divides, exact mode truncates).
  - `sr2_enh_fallen_not_composed`:
    - Setup: `enh_init(true, 1, 1, 100, true)`, `DSS(DS_road_mode) = 1`, `DSB(DS_racing) = 1`, `DSS(DS_shown_page) = 0`, `enh_race_begin()`, `enh_frame_built()`.
    - With `DSW(DS_fallen) = 0`: `enh_frame_presented()`, then `CHECK(enh_compose(small, rgb))`.
    - Then `DSW(DS_fallen) = 1`, `enh_frame_built()`, `enh_frame_presented()`: `CHECK(!enh_compose(small, rgb))`.
    - Finally `enh_race_end()` and restore `DS_fallen`.
  - Keep `sr2_enh_obj_geo_continuous` and `sr2_enh_opp_depth` passing on the new view. `sr2_enh_opp_depth` sets `opp_d` to the d it used.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh$' --output-on-failure`
  Expected: build FAIL (`enh_ser_snap` undeclared).
- [ ] **Step 2: Implement.**
  - `enh_frame_built` pushes the 16 channels:
    - `cam_y = DS_cam_y`, `bounce = DS_view_bounce_y`;
    - `hoff = cur_height - HGT[seg]`, read where the game reads it (`seg + C0h` when `MODE & 6`, race_road.c:57-62);
    - part C's channels 0.
  - The heading unwraps against a static last raw s16 with `enh_unwrap`. The series resets when `|(s16)(raw - last_raw)| >= 0x40` or `|pos - last pos| >= 2`.
  - It reads `snap.game_front`. Every `(s16)v->heading` in `enh_road2.c` becomes `enh_s16_of`.
  - `cam_of(const EnhView *, Cam *)` is used by draw and geometry alike:
    - `hpos`: exact = `v->hoff`, smooth = `interp(hgt_rel) + v->hoff`;
    - `cam_y` and `bounce` from the view.
  - `enh_obj_geo`, row cuts: z = `row_k(c, seg - base).z` for `0 <= seg - base <= 14` (so row 14 is 116). Otherwise `8 (seg - pos) + 4`.
  - `enh_obj_geo`, the opponent, from `v->opp_d`:
    - z: exact `cdiv16(d, 8)`, then +16 for sprite 0, +8 for sprite 2 when z < -2, then `|z| + 4`. Smooth: the same with d / 8.
    - dx and dy at `v->opp_seg` in exact mode, interpolated at `opp_pos` in smooth mode. dy = 0 in mode 4.
    - exact: `a = v->opp_sx`.
    - sprites 4..8 lift `yg` to `max(yg, Y[row])`, with `row = ((int)z + 1) >> 3` and Y the view's own front row (`v->ground_top_y` beyond row 14).
    - sprites 2 and 3: `yg = min(yg, 100)`, then `min(yg, 0x65)` (race_opp.c:226-262).
  - `enh_frame_presented`:
    - when `DSW(DS_fallen)`: layer OFF, `presented = false`, return before the capture;
    - otherwise fill the snapshot, `enh_rowtab_read(&snap.game_mirror)`, `enh_sheets_read()`, then capture and compare as now.
- [ ] **Step 3: Run** the Step 1 command. Expected: every `sr2_enh_*` line `ok`. Then:
  - `ctest --test-dir build-mac -LE slow`. Expected: `100% tests passed`.
  - At the task's end the slow tests: `ctest --test-dir build-mac -L slow --output-on-failure` (`sr2_enh_compare` and `sr2_smoke`). Expected: `ok   drag`, `ok   cruise`, `ok   grudge not enhanced`, `ok   mulholland not enhanced`, and `100% tests passed`. Every later task ends its Step 3 with this command too (its own expected compare lines).
- [ ] **Step 4: Commit** `git commit -m "Keep more of Street Rod 2's race frame state"`, then run `tools/sr1-codecheck.sh 50331e9 HEAD --allow host.c.o`. Expected: `0 differ`.

### Task 2: One object table, cuts drawn at their row

**Files:**
- Modify: `src2/enhanced/enhanced.h`. The id table. `enh_draw_objects(0)` returns to the base layer.
- Modify: `src2/enhanced/enhanced.c`:
  - the base layer;
  - `enh_mark_fg` clears tags;
  - `ObjCut` gets a kind and an absolute segment;
  - capture filters by region;
  - `enh_cuts_begin` / `enh_cuts_at`;
  - one `draw_view(v)` for `enh_compose` and `compare_frame`;
  - the depth sort `draw_objects` goes.
- Modify: `src2/enhanced/enh_internal.h`, `src2/enhanced/enh_road2.c` (calls `enh_cuts_at` in the band loop)
- Modify: `src2/platform/ega.c`, `ega.h` (`ega_mark_fg_rect`)
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Produces, in `enhanced.h`:
  ```c
  #define ENH_OBJ_OPP      15          /* the opponent, or the police in its slot */
  #define ENH_OBJ_MIR(r)   (15 + (r))  /* mirror rows 1..7: 16..22 */
  #define ENH_OBJ_STARTER  23          /* the starter figure in the mirror (its y pinned, starter_draw_mirror) */
  #define ENH_OBJ_STARS    24          /* part C: the Aqueduct's stars */
  #define ENH_OBJ_CLOUDS   25          /* part C: its clouds */
  #define ENH_OBJ_IDS      26
  ```
- Produces, in `ega.h`: `void ega_mark_fg_rect(u16 page, int x0, int y0, int x1, int y1);`. It is inclusive: it sets the fg bits and clears the object tags.
- Produces, in `enh_internal.h`:
  ```c
  enum { ENH_CUT_ROW, ENH_CUT_OPP, ENH_CUT_MIR, ENH_CUT_STARTER };
  enum { ENH_PASS_FRONT, ENH_PASS_MIRROR };
  bool enh_in_mirror_rect(int x, int y);        /* the body F0h..13Fh x 14h..2Ch, or the mount 110h..11Fh x 12h..13h */
  bool enh_in_glass(int x, int y);              /* F2h..13Dh x 16h..2Ah */
  bool enh_tag_kept(int id, int opp_sprite, int x, int y);   /* the capture's region rule */
  EnhObjGeo enh_obj_geo(const EnhView *v, int kind, int seg, int sprite);   /* MIR and STARTER: ok = false until Task 5 */
  void enh_cuts_begin(void);
  void enh_cuts_at(const EnhView *v, int pass, int lo, int hi, bool opp);
  extern int enh_cut_log[ENH_OBJ_IDS], enh_cut_log_n;   /* ids in the order the last picture drew them (tests) */
  ```
  - `enh_tag_kept`:
    - front ids 1..14, and the opponent with sprite < 9, are kept only outside the mirror rectangle;
    - mirror ids 16..23, and the opponent with sprite >= 9, only inside the glass.
  - Each cut's absolute segment:
    - a front row: `floor(pos) + id`;
    - the opponent: `snap.opp_seg`;
    - a mirror row: `floor(pos) - r`;
    - the starter: `DSS(DS_starter_seg)`.
  - `enh_cuts_at` draws every cut not yet drawn in this picture whose pass is `pass` and whose segment is in `lo..hi`: the row cuts, or the opponent when `opp`. The opponent's pass is MIRROR for sprite >= 9.
  - The front pass calls, with `n = base + k`:
    1. `(base + 15, INT_MAX)` row cuts, then the opponent, before the loop;
    2. in the loop, after band k: `(n, n, false)`;
    3. then `(n, n, true)`;
    4. after the loop: `(INT_MIN, base)` row cuts, then the opponent.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_mark_fg_clears_tags`: tag 40 pixels with id 3 in the OBJ layer, then `ega_mark_fg_rect(0, x0, y, x0 + 7, y)`. Those 8 have fg set and tag 0; the others keep tag 3.
  - `sr2_enh_base_layer`:
    - after `enh_draw_fg(true)`, `enh_draw_objects(4)`, `enh_draw_objects(0)`, a write sets fg;
    - after `enh_draw_begin()`, `enh_draw_objects(4)`, `enh_draw_objects(0)`, a write clears fg.
    - Both run inside a race with `active` (mode 1).
  - `sr2_enh_capture_regions`:
    - `enh_in_mirror_rect`: (EFh,14h) false, (F0h,14h) true, (13Fh,2Ch) true, (F0h,2Dh) false, (F0h,13h) false, (110h,12h) true, (120h,12h) false;
    - `enh_in_glass`: (F1h,16h) false, (F2h,16h) true, (13Dh,2Ah) true, (13Eh,2Ah) false, (F2h,2Bh) false;
    - `enh_tag_kept(3, 0, 0x100, 0x20)` false and `(3, 0, 0x100, 0x30)` true;
    - `enh_tag_kept(ENH_OBJ_OPP, 10, 0x100, 0x20)` true and `(ENH_OBJ_OPP, 10, 0x80, 0x40)` false;
    - `enh_tag_kept(ENH_OBJ_OPP, 5, 0x100, 0x20)` false;
    - `enh_tag_kept(ENH_OBJ_MIR(2), 0, 0xF1, 0x20)` false.
  - `sr2_enh_cut_order`:
    - Setup: mode 1, racing, `DS_seg` 100, `DS_opp_seg` 105, `DS_opp_sprite` 5, `DS_opp_last_d` 320, shown page 0. Tag 4x1 pixels in the shown page: id 3 at (100,70), id 5 at (120,60), id 15 at (140,60).
    - Action: `enh_frame_built`, `enh_frame_presented`, `enh_compose(small, rgb)`.
    - Check: `enh_cut_log` = {5, 15, 3}, `enh_cut_log_n == 3`.
  - `sr2_enh_obj_geo_continuous` and `sr2_enh_opp_depth` move to the new signature (`ENH_CUT_ROW`, `ENH_CUT_OPP`) and stay as they are otherwise.
  - `sr2_enh_race_begin_resets`: after `sr2_enh_cut_order`'s frame, `enh_race_begin()`, one frame with no tags drawn and composed: `enh_cut_log_n == 0`. A write after `enh_draw_objects(0)` clears fg (base BG).

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh$' --output-on-failure`. Expected: build FAIL.
- [ ] **Step 2: Implement.**
  - `enh_mark_fg` calls `ega_mark_fg_rect`.
  - `capture_objects`:
    - keeps a tag only where `enh_tag_kept` is true;
    - ignores ids `>= ENH_OBJ_IDS` (the sky tag of Task 4 included);
    - sets each cut's kind and segment;
    - its `fixed` comes from `snap.opp_fixed`.
  - Fixed cuts keep their frame geometry and are still drawn at their segment.
  - `draw_view(v)` = `enh_draw_road(v)`. Task 5 adds the mirror.
- [ ] **Step 3: Run** the Step 1 command, then `-LE slow`, then `-L slow`. Expected as in Task 1. Look at two snapshots 150 ms apart of a drag (`SR_SNAPSHOT_DIR` in the scratchpad, `--res-scale 4`): the opponent is drawn over its row's objects, and nearer road covers farther objects.
- [ ] **Step 4: Commit** `git commit -m "Draw Street Rod 2's captured objects at their row"`. Run `tools/sr1-codecheck.sh 50331e9 HEAD --allow host.c.o`. Expected: `0 differ`.

### Task 3: Road fixes every race kind shares

**Files:**
- Modify: `src2/enhanced/enh_road2.c`:
  - the outer edges per mode;
  - row 0's outer-edge quirk;
  - the exact projections exported;
  - `top_band_row`;
  - the table dash;
  - the exact sky;
  - `enh_rows_exact_front`.
- Modify: `src2/enhanced/enh_internal.h`
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Produces:
  ```c
  int enh_proj_x_i(int z, int X, int dx, int cx);               /* road_proj_x with DS_cam_x = cx (mode != 8) */
  int enh_proj_y_i(int z, int Y, int dy, int cam_y, int bounce); /* road_proj_y */
  void enh_rows_exact_front(const EnhView *v, EnhRowTab *t);     /* road_project_front's 15 rows, all nine tables */
  int enh_top_band_row(const double y[15], double ground0, double *ground);   /* road_draw_sky :481-487 */
  double enh_dash_width(int k, double q, bool exact);            /* DS_dash_width[k]; smooth lerp(t[k], t[k-1], q / 8) */
  int enh_sky_shift(s16 h, int sh);                              /* sky_scroll's a (race_road.c:430-440) */
  ```
  - `w = mode == 0x10 ? 200 : 400`. The outer edges are world `0x5DC - w` and `w + 0xCE4`, replacing `X_OUT_L` / `X_OUT_R`.
  - Row 0's quirk (race_road.c:184-187): XOL[0] is X = `-(w - lat)`, XOR[0] is X = `(w - lat) + 0x19C8`, both at z = 4. Exact mode uses s16 arithmetic with the integer lat. It applies to every band and wall that uses row 0 as its near row (the row-1 band).

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_proj_exact`: compare `enh_proj_x_i` with `road_proj_x` after `DSS(DS_cam_x) = cx`, `DSS(DS_road_mode) = 1`, over:
    - z 4..116, and 240 (F0h, the horizon);
    - X in {-2000, 0, 0x5DC, 0xCE4, 0x19C8 + 400 - 0xA00};
    - dx from -0x60 to 0x60 step 0x18;
    - cx in {0x500, 0xA00, 0xCE1}.
    - `enh_proj_y_i` is compared with `road_proj_y` likewise (Y in {2000, 0x76C, 0x79E, 2000 - 4000}, dy -40..40 step 10, cam_y 0x76C and 0x74A, bounce 0 and 10, set in `DS_cam_y` / `DS_view_bounce_y` for the game's call). No difference anywhere.
  - `sr2_enh_rows_front_row0`: flat zero track, mode 0, `DSS(DS_cam_x) = 0xCE1` for the game's calls, an exact view at pos 50.0, lat 0xCE1, heading 0. `enh_rows_exact_front` gives:
    - `xor_[0] == road_proj_x(4, (s16)((400 - 0xCE1) + 0x19C8), 0)`;
    - `xol[0] == road_proj_x(4, (s16)-(400 - 0xCE1), 0)`;
    - `xor_[1] == road_proj_x(12, 400 + 0xCE4, 0)`;
    - `y[14] == road_proj_y(116, 2000, 0)`.
  - `sr2_enh_top_band_row`:
    - y = {160, 100, 80, 70, 65, 62, 61, 60.5, 60.4, 60.3, 60.2, 60.1, 60, 59, 58} with ground0 69: returns 14, ground 58;
    - y[7..14] all 63, y[6] 61, rows 0..5 as in the first case, ground0 69: returns 6, ground 61 (a crest);
    - y[13] = y[14] = 50: returns 14 (strict <, scanning from 14).
  - `sr2_enh_dash_width`:
    - exact: `enh_dash_width(k, 3, true) == DSS(DS_dash_width + 2k)` for k 1..14;
    - smooth: `enh_dash_width(5, 4, false) == (t5 + t4) / 2`.
  - `sr2_enh_sky_shift`: (5,1) gives 8; (-5,1) gives 312; (0,1) gives 320; (165,1) gives 8; (200,2) gives 160; (-200,2) gives 160.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh$' --output-on-failure`. Expected: build FAIL.
- [ ] **Step 2: Implement.**
  - **Crests:** in race modes the bands of rows above `top_band_row` are not drawn. Exact mode compares the integer row y; smooth mode compares `py - ROW_TOP`. The grass, in modes other than 10h, starts at `max(ground, 0x12)`, and `enh_ground_y = ground`. Town keeps the fixed 3Fh ground and no cull.
  - **Dash:** drawn per band as a quad from `mid - (w >> 1)` to `mid - (w >> 1) + w + 1` at both rows, only when `xl < 0x13F`. Modes 2/4 widen it in Task 6.
  - **Exact sky:** the sky's shift is `enh_sky_shift(enh_s16_of(heading), 1)` and the horizon strip's is `enh_sky_shift(.., 2)`. The horizon `hy` is `enh_proj_y_i(0xF0, 2000, dy, cam_y, bounce)` with `dy` the integer `cur_height - HGT[seg - 1]` (0 in modes 2/4), then the clamp; the sky rows end at `hy - 12` and the strip at `hy + 8`, both integers. Smooth mode is unchanged.
  - `enh_rows_exact_front` uses the same Cam as the drawing.
- [ ] **Step 3: Run** the Step 1 command, `-LE slow` and `-L slow`. Expected as in Task 1.
- [ ] **Step 4: Commit** `git commit -m "Fix Street Rod 2's near road edges, crests and sky"`. Codecheck: `0 differ`.

### Task 4: The compare tool for every race kind and the mirror

**Files:**
- Modify: `src2/enhanced/enh_compare.h`, `enh_compare.c`:
  - the struct;
  - edges ignore short runs;
  - the per-mode log;
  - `enh_compare_log_rows`, `enh_compare_skipped`.
- Modify: `src2/enhanced/enhanced.c`:
  - `compare_frame` with its masks and the geometry and re-projection checks;
  - skipped frames;
  - `enh_draw_sky`.
- Modify: `src2/enhanced/enhanced.h` (`enh_draw_sky`), `src2/platform/ega.c`, `ega.h` (`EGA_LAYER_SKY`, `EGA_TAG_SKY`)
- Modify: `src2/game/race_road.c`, the sky hooks:
  - `road_draw_front` :1275, `enh_draw_sky(true)` before the sky call and `enh_draw_sky(false)` after it;
  - `road_draw_sky`, `enh_draw_sky(false)` right before each `drv_fill_rows` (:477, :486);
  - `road_draw_sky_mulholland` :513, the same before its fill.
- Test: `tests/test_sr2_enh.c`, `tests/test_sr2_enh_compare.sh`

**Interfaces:**
- Produces:
  ```c
  typedef struct {
      int rows, edge_rows, edge_err_max, pixels, mismatched;            /* front view, outside the mirror rectangle */
      int m_rows, m_edge_rows, m_edge_err_max, m_pixels, m_mismatched;  /* the mirror's glass */
      int mirror_cov;         /* covered glass pixels: 1596 when the mirror is enhanced, 0 when it is the game's */
      int geo_err;            /* largest |enhanced exact row value - game's row table value| */
      int obj_err;            /* largest re-projection error of a cut's bottom (opponent: and centre x), px; -1 none */
      int police, opp_sprite; /* DS_police_state, DS_opp_sprite */
  } EnhCompare;
  #define ENH_EDGE_MIN_RUN 3   /* road-colour runs shorter than this are not the road's edge */
  void enh_compare_log_rows(bool on);    /* SR_ENH_COMPARE_ROWS logs rows only while on (the edge pass) */
  void enh_compare_skipped(int mode);    /* a frame the enhanced view did not show (fallen) */
  #define EGA_LAYER_SKY 4                /* ega.h: like BG, and every written pixel is tagged EGA_TAG_SKY */
  #define EGA_TAG_SKY 0xF0
  void enh_draw_sky(bool on);            /* the sky's and the mirror backdrop's blits: SKY layer; off: the base layer */
  ```
  - `enh_compare_rows` keeps its signature. Its `edges()` takes the first and last pixel of covered road-colour runs at least `ENH_EDGE_MIN_RUN` long.
  - Frame line:
    `frame %d mode %d rows %d edge_rows %d edge_err_max %d mismatch %d mirror_rows %d mirror_edge_rows %d mirror_edge_err_max %d mirror_mismatch %d mirror_cov %d geo_err %d obj_err %d police %d opp_sprite %d`
  - At close, one line per mode seen (skipped frames included), modes ascending:
    `mode %d frames %d skipped %d worst_edge %d mismatch_p50 %d mismatch_max %d mirror_worst_edge %d mirror_mismatch_p50 %d mirror_mismatch_max %d mirror_cov_min %d geo_max %d obj_max %d shown %d`
    then `frames %d skipped %d shown %d`.
    - p50 is `sorted[(n - 1) / 2]`.
    - `obj_max` is the largest `obj_err >= 0`, or 0.
    - `mirror_cov_min` is 0 for a mode with no frames.
    - `shown` counts compositions under the last presented frame's mode.
  - `compare_frame`'s regions:
    - Front: `cov && !enh_in_mirror_rect`. Mismatch over rows 12h..y_end. Edges from the ground to y_end, over `cov` minus object-tagged pixels, with the SKY rule of the Decisions. The ground is `snap.ground_top_y` in a race, and 3Fh in town: `road_draw_sky` returns before it writes `DS_ground_top_y` in town, so the DS word is stale there.
    - Mirror: `cov && enh_in_glass`. Mismatch over 16h..2Ah. Edges from `snap.game_mirror.y[7]` to 2Ah (rows 2Bh exclusive), with the same object and SKY rules. The mirror's road colours are {1, 15} in a race. In town they are {4}, the kerbs: the town mirror leaves its road unfilled (`road_draw_band_mirror`'s C & 1 rows), so the glass's colour 0 shows there and colour 1 never does.
    - `mirror_cov` counts covered glass pixels.
  - `geo_err`: the largest difference, over all nine tables of rows 0..14, between `enh_rows_exact_front(newest)` and `snap.game_front`. Task 5 adds the mirror's.
  - `obj_err`:
    - Before the new capture, each old cut is mapped to the new exact view with the drawing's own mapping (`enh_obj_geo` of its kind and segment).
    - It is matched with the new cut of the same kind and absolute segment; the opponent also needs the same sprite. (Not the same id: a row cut's id is its segment minus the car's, so it changes whenever the car crosses a segment between the two frames.)
    - The error is `|predicted bottom - actual bottom|`, and for the opponent also `|predicted centre x - actual centre x|`.
    - Skipped:
      - fixed cuts;
      - cuts whose rectangle touches the border of their pass (front: x 0 or 13Fh, top row 12h, or bottom row >= y_end - 1, i.e. 63h, the last row the capture scans above the split; mirror: the glass edges);
      - cuts partly hidden in either frame: the bottom more than 1 px above their own geometry's `yg` (nearer road, a wall, a hillside, a nearer row's objects, the hood or the A-pillar over them);
      - every old cut after a series reset or a new race (a jump: the segments need not be the same road).
    - If `obj_max` fails, look at the offending cut in the dump first: a hidden cut the rules above miss is a rule to add; anything else is a geometry bug.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_compare_rows` gains two cases:
    - orig and enh rows with road 100..200 plus a 2-px road run at 260..261 in orig only: `edge_err_max == 0`;
    - a row whose only road is 2 px wide: not an edge row.
  - `sr2_enh_compare_log` (replaces the old strings). Frames, as designated initializers:
    - a = {rows 84, edge_rows 80, edge_err_max 1, pixels 26880, mismatched 27, m_rows 21, m_edge_rows 10, m_edge_err_max 0, m_pixels 1596, m_mismatched 16, mirror_cov 1596, geo_err 0, obj_err 1, police 0, opp_sprite 5}, mode 1;
    - b = {84, 82, 0, 26880, 54, 21, 12, 1, 1596, 0, 1596, 0, -1, 2, 10}, mode 1;
    - c = {82, 30, 1, 26240, 0, 21, 0, 0, 1596, 3, 1596, 0, 0, 0, 0}, mode 0;
    - then `enh_compare_skipped(16)`.

    The log must contain exactly:
    ```
    frame 1 mode 1 rows 84 edge_rows 80 edge_err_max 1 mismatch 1 mirror_rows 21 mirror_edge_rows 10 mirror_edge_err_max 0 mirror_mismatch 10 mirror_cov 1596 geo_err 0 obj_err 1 police 0 opp_sprite 5
    frame 2 mode 1 rows 84 edge_rows 82 edge_err_max 0 mismatch 2 mirror_rows 21 mirror_edge_rows 12 mirror_edge_err_max 1 mirror_mismatch 0 mirror_cov 1596 geo_err 0 obj_err -1 police 2 opp_sprite 10
    frame 3 mode 0 rows 82 edge_rows 30 edge_err_max 1 mismatch 0 mirror_rows 21 mirror_edge_rows 0 mirror_edge_err_max 0 mirror_mismatch 2 mirror_cov 1596 geo_err 0 obj_err 0 police 0 opp_sprite 0
    mode 0 frames 1 skipped 0 worst_edge 1 mismatch_p50 0 mismatch_max 0 mirror_worst_edge 0 mirror_mismatch_p50 2 mirror_mismatch_max 2 mirror_cov_min 1596 geo_max 0 obj_max 0 shown 0
    mode 1 frames 2 skipped 0 worst_edge 1 mismatch_p50 1 mismatch_max 2 mirror_worst_edge 1 mirror_mismatch_p50 0 mirror_mismatch_max 10 mirror_cov_min 1596 geo_max 0 obj_max 1 shown 0
    mode 16 frames 0 skipped 1 worst_edge 0 mismatch_p50 0 mismatch_max 0 mirror_worst_edge 0 mirror_mismatch_p50 0 mirror_mismatch_max 0 mirror_cov_min 0 geo_max 0 obj_max 0 shown 0
    frames 3 skipped 1 shown 0
    ```
  - `sr2_enh_sky_tags`:
    - in `EGA_LAYER_SKY` with `ega_set_obj` irrelevant, a `vga_wr` that leaves the colour unchanged still tags all 8 pixels `EGA_TAG_SKY` and clears fg;
    - a later BG write clears the tags.
  - Rewrite `tests/test_sr2_enh_compare.sh`:
    - Runs, in parallel, with `SR_SEED` 2: `cruise 90`, `drag 90`, `grudge 200`, `mulholland 110`, `aqueduct 80`. The grudge run is done at about 167 s (the tests report); 200 leaves room for five parallel runs, so the return drive is always in the log.
    - `check <kind> <mode> <min frames> <orig|enh> [noobj]` reads that kind's `mode <m>` line.
    - It passes when:
      - `frames >= min`;
      - `worst_edge <= 1`, `mismatch_p50 <= 10`;
      - `geo_max = 0`, `obj_max <= 1` (not checked with `noobj`);
      - `shown >= frames`;
      - with `orig`: `mirror_cov_min = 0`;
      - with `enh`: `mirror_cov_min = 1596`, `mirror_worst_edge <= 1`, `mirror_mismatch_p50 <= 10`;
      - with min > 0, the numbers measured something: some `frame` line of that mode has `edge_rows > 0`, with `enh` also one with `mirror_edge_rows > 0`, and without `noobj` one with `obj_err >= 0`. (A run whose cuts never match, or whose rows never hold a road, would otherwise pass on zeros.)
    - With min 0, a missing line passes; a present one must pass.
    - `after <kind> <mode> <last mode> <min>` passes when at least min `frame` lines of `<mode>` follow the last `frame` line of `<last mode>`: `after grudge 0 4 20` proves the return town route (location 4, frame ~148 of seed 2) was compared, not only the drive there.
    - `none <kind> <mode>` passes when there is no `mode <m>` line.
    - Output: `ok   <kind> mode <m>`, `ok   <kind> mode <m> not enhanced`, or `FAIL <kind> mode <m> (<line or no frames>)`.
    - This task's checks:
      - `check cruise 0 20 orig`;
      - `check drag 1 20 orig`, `check drag 0 0 orig`;
      - `check grudge 0 20 orig` (the location-4 routes, there and back), `after grudge 0 4 20`;
      - `none grudge 2`, `none grudge 4`;
      - `check mulholland 0 0 orig`, `none mulholland 16`;
      - `none aqueduct 8`, `check aqueduct 0 0 orig`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: build FAIL, then the compare FAIL on `grudge mode 0` until the edge rule is in.
- [ ] **Step 2: Implement** as in the Interfaces.
  - The two `enh_compare_rows` passes are the mismatch pass and the edge pass. Only the edge pass logs rows.
  - A fallen frame (Task 1) calls `enh_compare_skipped`.
- [ ] **Step 3: Run** the Step 1 command. Expected: all `ok`, and `ok   grudge mode 0` with `worst_edge 1` or 0.
  - Check frame 148 of the grudge run's return drive with `SR_ENH_COMPARE_ROWS=1`: row 4Ah no longer differs by more than 1.
  - In that frame's dump (`SR_ENH_COMPARE_DUMP`) read row 4Ah's right facade in both images (palette indices, as the mapping run's `idx.py` printed them). If the enhanced facade starts more than 1 px from the original's (the report saw 308 against 311), find out why with superpowers:systematic-debugging (`face()` against `quad_fill`'s `poly_fill_pts` on a near crossing band) and fix it in `enh_road2.c`, with a unit test of that band's facade against the game's own row values. Edges and mismatch must not get worse in `cruise`.
  - If any frame still shows an edge over 1, use superpowers:systematic-debugging on its dump before going on. The thresholds are not loosened without an owner ruling.
  - Then `-L slow`. Expected: `100% tests passed`.
- [ ] **Step 4: Commit** `git commit -m "Check every Street Rod 2 race kind per mode"`. Codecheck: `0 differ`.

### Task 5: The rear-view mirror

**Files:**
- Create: `src2/enhanced/enh_geo2.h`. Moved from `enh_road2.c`, with no change in behaviour:
  - `Cam` (gaining `int mode, w`), `Row`, `cam_of`;
  - `trk` / `yaw_rel` / `hgt_rel` / `interp`;
  - `edges`, `px` / `py`, `ROW_TOP`, `face` / `quad`.
- Create: `src2/enhanced/enh_mirror2.c`
- Modify: `src2/enhanced/enh_road2.c`, `enhanced.c`:
  - `draw_view` adds the mirror pass;
  - mirror kinds in `enh_obj_geo`;
  - `geo_err` covers the mirror;
  - `enh_mirror_begin` / `_end`.
- Modify: `src2/enhanced/enhanced.h`
- Modify: `src2/game/race_road.c`, the hooks (each `ENH:`):
  - `road_frame` :1452 `enh_draw_fg(true)` becomes `enh_mirror_begin()`;
  - :1460 `enh_mark_fg(...)` becomes `enh_mirror_end()`;
  - `road_draw_mirror` :1418: `enh_draw_sky(true)` / `enh_draw_sky(false)` around `mirror_draw_backdrop()`;
  - after `road_draw_row_mirror()` (:1423): `enh_draw_objects(ENH_OBJ_MIR(row))`;
  - `enh_draw_objects(ENH_OBJ_OPP)` before the opponent test (:1431) and `enh_draw_objects(0)` after it;
  - `mirror_objects` :1397: `enh_draw_objects(ENH_OBJ_STARTER)` before `starter_draw_mirror` and `enh_draw_objects(ENH_OBJ_MIR(row))` after it, both inside the `if` (the call becomes a braced block).
- Test: `tests/test_sr2_enh.c`, `tests/test_sr2_enh_compare.sh`

**Interfaces:**
- Produces:
  ```c
  void enh_mirror_begin(void);   /* enhanced.h: the mirror is drawn into the background (base layer BG) */
  void enh_mirror_end(void);     /* the frame stays the game's: ega_mark_fg_rect on the mount 110h..11Fh x 12h..13h,
                                    F0h..13Fh x 14h..15h and 2Bh..2Ch, F0h..F1h and 13Eh..13Fh x 16h..2Ah; then base FG */
  double enh_mirror_z(int k, double q);   /* 8; 8k + q for k 1..4; 40, 48, 56 for k 5..7 (the k == 5 freeze) */
  int enh_proj_x_mirror_i(int z, int X, int dx, int cx);   /* road_proj_x_mirror */
  int enh_proj_y_mirror_i(int z, int Y, int dy, int cam_y); /* road_proj_y_mirror (no bounce) */
  void enh_rows_exact_mirror(const EnhView *v, EnhRowTab *t);   /* road_project_mirror: rows 0..7, yk 0..14 with the quirk */
  void enh_draw_mirror(const EnhView *v);
  ```
- Mirror row k (k = 0..7) sits at segment n = base - k:
  - its edges come from C[n];
  - dx = yaw_rel(n) - hv, dy = hgt_rel(n) - hpos (0 in modes 2/4);
  - in mode 10h, k == 0 clamps dx: > 3Ch becomes 46h, < -3Ch becomes -46h;
  - XOR / XOL at world `w + 0xCE4` / `0x5DC - w` (no row-0 quirk);
  - YE at 76Ch, YK at 79Eh, written to `yk[0]` for k = 0 and to `yk[2k - 1]`, `yk[2k]` for k >= 1;
  - the banks come from BL / BR at base + k (forward).
- The mirror pass (`enh_draw_mirror`), following `road_draw_mirror` (:1411-1437):
  - Clip: the glass, `enh_clip(0xF2, 0x16, 0x13E, 0x2B)`.
  - Backdrop:
    - `enh_image_vram(0x4000, 0xEA, 0x6A, 76, 21)` at (F2h,16h);
    - the sky strip: `enh_image_vram(0x4000, 0xE8, 0x81, 0x50, h)` with h 2 (14h in mode 10h), at y = Y7 - 2 (Y7 - 10 in 10h), clipped as `blit_clip_xy` (:287-302).
  - Cuts before the loop: `(INT_MIN, base - 8)`.
  - For row r = 7..1 (the far row r, the near row r - 1, flags at base - r - 1):
    - hillsides: Task 7;
    - `town_walls_mirror` (:768-797, keyed on C & 1, not on the town flag). Its storey is half-height on the left and double on the right, and its end faces follow;
    - the band, as `road_draw_band_mirror` (:659-737):
      - the lines stop at y < 2Bh;
      - grass from F0h and to 13Fh;
      - no road fill on C & 1 rows, kerbs only;
      - the finish is white only when DS_seg >= 169h;
      - the dash `DS_dash_width[13 + r]`, stepped per row in both modes (the mirror's rows 5..7 are frozen anyway), with no `xl < 13Fh` test (the front has one, the mirror not);
    - cuts `(n, n, false)`, which covers the banner, the row objects and the starter;
    - rails: Task 8;
    - cuts `(n, n, true)`;
    - barrier walls: Task 6.
  - Cuts after the loop: `(base, INT_MAX)`.
  - Not reproduced: `opponent_draw`'s clip-not-restored return (race_opp.c:273). When the opponent's front picture (sprite < 9, cross-over (b)) is drawn from the mirror loop with its rectangle off the view, the rest of the mirror is drawn under the view clip and the original's mirror spans can spill left of F0h. That is a rare frame of mismatched sky pixels at rows 16h..2Ah, above every edge row; the enhanced mirror stays inside the glass. If a compare frame fails on it, show its dump to the owner; the threshold stays.
- `enh_obj_geo` for the mirror kinds:
  - **MIR, at seg s:** k = base - s, z = `enh_mirror_z(k, q)`, ok for k >= 0 (beyond row 7, z = 8k continues the frozen rows). a = mirror px of X 0, b = 1/z, yg = mirror py of 2000 + ROW_TOP.
  - **OPP with sprite >= 9:**
    - z = `-cdiv16(d, 8)` (smooth: -d/8);
    - x from `proj_x_mirror(z, opp_lat, 10 - dx)` with dx = heading - YAW[oseg]; exact `a = v->opp_sx`;
    - yg = `proj_y_mirror(z, 2000, -dy)`, with no row clamp (race_opp.c:221-225).
  - **STARTER:** x as MIR at `DS_starter_seg`. yg is the frame's own (its y is pinned, race_opp.c:71).

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_mirror_depths`: for q in {0, 3, 7}, `enh_mirror_z(k, q)` for k 0..7 is {8, 8+q, 16+q, 24+q, 32+q, 40, 48, 56}.
  - `sr2_enh_proj_mirror_exact`: `enh_proj_x_mirror_i` and `enh_proj_y_mirror_i` sweep as in `sr2_enh_proj_exact`, against `road_proj_x_mirror` and `road_proj_y_mirror`, with z 2..56. No difference.
  - `sr2_enh_mirror_yk`: flat track, mode 1. `enh_rows_exact_mirror` has `yk[0] == enh_proj_y_mirror_i(8, 0x79E, 0, 0x76C)`, and `yk[2k-1] == yk[2k] == enh_proj_y_mirror_i(enh_mirror_z(k, q), 0x79E, 0, 0x76C)` for k 1..7.
  - `sr2_enh_mirror_opp_geo`: an exact view with `opp_d` -100, sprite 10, `opp_sx` 0x120. `enh_obj_geo(v, ENH_CUT_OPP, opp_seg, 10)` gives `z == 12`, `a == 0x120`, `b == 1.0 / 12`.
  - `sr2_enh_mirror_hooks`, inside an active mode-1 race:
    - after `enh_mirror_begin()`, a write at (100h,20h) leaves fg clear;
    - after `enh_mirror_end()`: fg is set at (F0h,20h), (100h,14h), (115h,12h) and (100h,2Ch); it is clear at (100h,20h), and its tag is 0;
    - a write after `enh_mirror_end` sets fg.
  - `test_sr2_enh_compare.sh`: every `check` of cruise, drag, grudge mode 0, mulholland mode 0 and aqueduct mode 0 switches from `orig` to `enh`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: build FAIL.
- [ ] **Step 2: Implement.** `geo_err` also covers `enh_rows_exact_mirror(newest)` against `snap.game_mirror`: rows 0..7, and `yk` 0..14.
- [ ] **Step 3: Run** the Step 1 command, then `-L slow`. Expected: all `ok`, with `mirror_cov_min 1596` in each checked mode and `100% tests passed`. Then look at snapshots at `--res-scale 4` of a drag and a town drive (`SR_SNAPSHOT_DIR` in the scratchpad):
  - the mirror is sharp and its road moves smoothly, with rows 5..7 stepping per segment as the original's;
  - the opponent shows in it behind;
  - the dashboard's side mirror still shows the original's pixels.
- [ ] **Step 4: Commit** `git commit -m "Draw Street Rod 2's rear-view mirror enhanced"`. Codecheck: `0 differ`.

### Task 6: The Grudge drag strip

**Files:**
- Create: `src2/enhanced/enh_poly2.c`, `src2/enhanced/enh_poly2.h`
- Modify: `src2/enhanced/enh_geo2.h`, `enh_road2.c` (the strip's track, flat projection, wide dash, barrier walls), `enh_mirror2.c` (the strip's mirror walls and dash)
- Modify: `src2/enhanced/enhanced.c` (`mode_enhanced` :104 takes 0, 1, 2, 4)
- Test: `tests/test_sr2_enh.c`, `tests/test_sr2_enh_compare.sh`

**Interfaces:**
- Produces:
  ```c
  int enh_clip_poly_i(const s16 *in, int n, s16 *out, int xmin, int xmax, int ymin, int ymax);  /* clip_polygon's
          Sutherland-Hodgman (race_gfx.c:362-402), line_*_at_* truncating, 64-point buffers; returns the count */
  void enh_fill_poly_incl(const double *x, const double *y, int n, u8 col);   /* each pixel row from the polygon's
          leftmost to its rightmost point within the row's band, plus 1 px: poly_fill's edge pixels included (the
          hull of the vertices' pixel squares when convex; y-monotone concave polygons as poly_fill fills them) */
  void enh_line1(double x0, double y0, double x1, double y1, u8 col);         /* one original px thick through pixel
          centres, both ends included (Task 8 uses it) */
  u16 enh_track(int mode, u16 arr, int i);   /* the word the game reads: modes 2/4 take TRK_C/L/R from TRK_C2/L2/R2
          at i (no clamp) and the other arrays at i + C0h; other modes as trk() (clamped) */
  int enh_wall_colour(int phase, int n, int row, int seg);   /* abs(n - row - seg + phase) % 10 < 5 ? 0x0B : 4 */
  ```
- **Strip (modes 2/4):**
  - Cam reads through `enh_track`;
  - hpos = 0, the rows' dy = 0, the horizon's dy = 0, the opponent's dy = 0;
  - the dash is wide where n < start_seg + 5: front width `t[k] << 3`, mirror `t[13 + r] << 2`, colour 2;
  - the finish band (C2[57h] & 10h) is white in the front view only.
- **Front barrier walls** (`road_draw_barrier_walls` :986-993), every row 14..1, after that row's opponent cut, regardless of `top_band_row`:
  - the right side is the quad (XR_p, YK_p), (XR_p, Y_p), (XR_q, Y_q), (XR_q, YK_q); the left side the same with XL;
  - the colour is `enh_wall_colour(start_seg, 14, row, base)`.
- **Mirror walls:** n = 7, the mirror's YK quirk tops, and `enh_wall_colour(start_seg, 7, r, base)` (the original's phase, not the shown segment).
- **Walls, exact mode:** integer vertices, `enh_clip_poly_i` to the view (0, 13Fh, 12h, 65h) or to the mirror's clip (`clip_set_mirror`: F0h, 13Fh, 16h, 2Ah; wider than the glass, whose clip still applies to the fill), then `enh_fill_poly_incl` (y + ROW_TOP). Smooth mode: `enh_fill_poly_incl` of the float vertices under the current clip.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_strip_tracks`:
    - `enh_track(2, TRK_C, 5) == TRKW(TRK_C2, 5)`;
    - `enh_track(4, TRK_HGT, 5) == TRKW(TRK_HGT, 5 + 0xC0)`;
    - `enh_track(1, TRK_C, 5) == TRKW(TRK_C, 5)`.
  - `sr2_enh_wall_colour`:
    - front `enh_wall_colour(8, 14, row, seg)`: seg + row 18 gives 0x0B, 17 gives 4, 22 gives 0x0B, 27 gives 4, 32 gives 0x0B;
    - mirror `enh_wall_colour(8, 7, 3, 20)` gives 4.
  - `sr2_enh_clip_poly_i`: set `DS_clip_xmin/xmax/ymin/ymax` to the view, then to the mirror. For four polygons:
    - inside;
    - crossing the right edge;
    - crossing the bottom and the left;
    - wholly outside;

    the result equals `clip_polygon`'s point for point (the last one has no points).
  - `sr2_enh_poly_incl`: at res 1, aa 1, after `enh_buf_setup()` and `enh_clip_view()`:
    - the quad (10,20),(10,30),(11,30),(11,20), with y + 0.5, fills exactly the 22 samples of columns 10..11, rows 20..30;
    - the degenerate quad (40,50),(60,50),(60,50),(40,50) fills the 21 samples of row 50, x 40..60;
    - the concave (y-monotone) L-shaped hexagon (10,10),(20,10),(20,15),(15,15),(15,20),(10,20) fills exactly 96 samples: rows 10..15 over x 10..20 and rows 16..20 over x 10..15 (its convex hull would fill more).
  - Compare script:
    - `check grudge 2 20 enh` and `check grudge 4 20 enh` replace the two `none`;
    - `check grudge 0 20 enh` stays.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: build FAIL, then the compare FAIL with `grudge mode 2` not enhanced.
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the Step 1 command, then `-L slow`. Expected: all `ok`, `100% tests passed`.
  - A few rounds' first frames may show a 2-px edge at row 61h, by the hood (seen in a prototype). Mode 4's "Drivers stage" box comes after presentation (race_drive.c:364-372). If any frame still shows such an edge, debug it with its dump (superpowers:systematic-debugging). The threshold stays.
  - Look at snapshots of a qualifier and a round at `--res-scale 4`: walls in 5-segment pink and white blocks, the grey start box, the tree, and the opponent beside you and in the mirror.
- [ ] **Step 4: Commit** `git commit -m "Draw the Grudge drag strip enhanced"`. Codecheck: `0 differ`.

### Task 7: Mulholland's road, backdrop and hillsides

**Files:**
- Create: `src2/enhanced/enh_hills2.c` (the backdrop, `front_hillsides` and `mirror_hillsides` ports)
- Modify: `src2/enhanced/enh_geo2.h`, `enh_road2.c` (the 10h bands, the row-0 dx clamp, L16), `enh_mirror2.c` (the 10h mirror bands, sky strip and hillsides)
- Modify: `src2/enhanced/enhanced.c` (mode 10h enhanced; `snap.before[5]`)
- Test: `tests/test_sr2_enh.c`, `tests/test_sr2_enh_compare.sh`

**Interfaces:**
- Produces:
  ```c
  int enh_mulholland_sky_dy(int dy, int fall_dir);   /* <= 8; >= -8 only while fall_dir == 0 (race_road.c:492-494) */
  void enh_hill_consts(int row, s16 *w0, s16 *h0, s16 *w1, s16 *h1, s16 *e);   /* DS words 7E1Eh / 7E20h / 7E22h /
          7E24h / 7E3Ah + 4 (15 - row): read at run time, never copied */
  void enh_draw_mulholland_sky(const Cam *c, const EnhView *v);
  void enh_front_hillsides(const Cam *c, const Row *rows, int row, bool l16, int fall_dir);
  void enh_mirror_hillsides(const Cam *c, const Row *mrows, int row, int fall_dir);
  ```
- **Backdrop** (`road_draw_sky_mulholland` :490-515):
  - dy = `enh_mulholland_sky_dy(hpos - hgt_rel(base - 4), fall_dir)`;
  - screen rows y = 12h .. 12h + h - 1 show sheet row y + 5Ch - dy, with h = min(53h, 199 - (6Eh - dy));
  - exact: shift `enh_sky_shift(enh_s16_of(heading), 1)`; smooth: 2 * heading;
  - colour 0Eh below the picture;
  - no grass fill;
  - the horizon is hy = proj_y(F0h, 2000, dy), with no clamp. `top_band_row` follows from hy + 8.
- **Bands** (`road_draw_band_front` :597-624), with w = 200 (Task 3):
  - shoulders: colour 2 from xol..xl and xr..xor, stepped, when !(C & 80h) and `v->hoff >= -0x0B`;
  - flat sides: colour 9 from 0..xl and xr..13Fh. With the frame's L16 and row >= 10 they span xl - ext..xl and xr..xr + ext instead, with ext = DS word 7E22h + 4 (16 - row). "Flat" is `YBL[q] == Y[q]` in exact mode (integer) and `BL[n_q] == 0` in smooth mode;
  - road colour 1, or 0Fh when C & 10h and the player's seg >= 3ADh;
  - the dash;
  - row 0's dx is clamped to +-30h.
  - L16 is one flag per frame (race_road.c:1277-1279).
- **Front hillsides:** before the band of each row whose C has no 80h, culled or not, with:
  - the constants by row index;
  - o = fall_dir;
  - dyaw = hv;
  - the sic quirks: the right side uses the left test, and a vertex at 140h.
  - Row 1's "q - 1" values are the DS words just before each table (`T_* - 2`). They are read into `snap.before` at `enh_frame_built` and used in both modes.
- **Mirror** (10h):
  - the sky strip is 14h rows at Y7 - 10;
  - shoulders xol - 6..xl and xr..xor + 6, unstepped over the band;
  - grass only where YBL / YBR[q] == Y[q];
  - `enh_mirror_hillsides` follows `mirror_hillsides` (:1305-1329) with XMIN F0h, XMAX 13Fh, YMAX 2Ah, the gates o != 1 / o != -1, and the forward banks.
- **Fall:** the eight fall frames are drawn from the series: cam_y, hoff, and fall_dir from the snapshot. A fallen frame is skipped (Task 1).
- The guard rails and trusses stay captured cuts in this task.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_mulholland_rows`: mode 10h, flat track, lat 0xA00, exact view.
    - `xol[1] == road_proj_x(12 - q, 0x514, dx)` and `xor_[1] == road_proj_x(12 - q, 0xDAC, dx)`;
    - with heading 0x50: `xl[0] == road_proj_x(4, 0x5DC, 0x30)`;
    - `enh_rows_exact_mirror` with heading -0x50 gives `xl[0] == enh_proj_x_mirror_i(8, 0x5DC, 0x46, 0xA00)`.
  - `sr2_enh_hill_consts`: `enh_hill_consts(14, ..)` gives w0 = `DSS(0x7E22)`, h0 = `DSS(0x7E24)`, w1 = `DSS(0x7E26)`, h1 = `DSS(0x7E28)`, e = `DSS(0x7E3E)`. Row 1 gives w0 = `DSS(0x7E1E + 56)`.
  - `sr2_enh_mulholland_sky_dy`: (12,0) gives 8; (-12,0) gives -8; (-12,1) gives -12; (3,0) gives 3.
  - Compare script:
    - `check mulholland 16 20 enh noobj` replaces `none mulholland 16`. The guard rails and trusses are still row cuts in this task; they span two rows, so their re-projection is off by design (the mapping report: about 20 % at the car) and `obj_max` would fail. Task 8 drops `noobj`;
    - `check mulholland 0 0 enh` stays;
    - `none aqueduct 8` stays.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: build FAIL.
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the Step 1 command, then `-L slow`. Expected: all `ok`, `geo_max 0` for mode 16, `100% tests passed`. Look at snapshots at `--res-scale 4` of the hills, a crest, the bridge (its ravine shows the backdrop) and a cliff fall if the run has one (otherwise steer one with `SR_KEYS`).
- [ ] **Step 4: Commit** `git commit -m "Draw Mulholland's road and hills enhanced"`. Codecheck: `0 differ`.

### Task 8: Mulholland's guard rails and bridges, and the police

**Files:**
- Modify: `src2/game/race_road.c`. Each hook is `ENH:` and moves the game's drawing to the background:
  - `road_draw_front` :1289: `enh_draw_objects(0)` before `mulholland_extras_front(row)`;
  - `road_draw_mirror` :1429: `enh_draw_objects(0)` before `mulholland_extras_mirror(row)`.
- Modify: `src2/enhanced/enh_hills2.c` (rails, posts, truss, truss end), `enh_road2.c`, `enh_mirror2.c` (call them in the row loop between the row cuts and the opponent cut, for every row 14..1 or 7..1 whether its band was culled by `top_band_row` or not, as `mulholland_extras_front` / `_mirror` run)
- Modify: `src2/game/race_phys.c`, the developer aid `SR_DEBUG_POLICE=<seg>`, marked `PORT: developer aid`:
  - `race_start` :280-284: in mode 10h, not vs the King, after the original's two `rnd` draws (so the random sequence stays the same), `police_state` 1 and `police_seg` = the value;
  - `race_step` case 1 :845-849: when the aid is set and `seg < opp_seg + 7`, set `DS_opp_seg = seg - 8` first, so the police comes from behind as in a real spawn.
- Test: `tests/test_sr2_enh.c`, `tests/test_sr2_enh_compare.sh`

**Interfaces:**
- Consumes: `enh_line1`, `enh_fill_poly_incl`, `enh_clip_poly_i` (Task 6).
- Produces:
  ```c
  bool enh_rail_post(int phase, int row, int seg, int n);   /* cmod16(phase - row - seg + n, 2) != 0 */
  int enh_rail_phase(const Cam *c, int fallback);           /* road_draw_front :1267-1271 at the view's base */
  void enh_guard_rail(const Cam *c, const Row *p, const Row *q, int row, int post, int n, int m, int phase);
  void enh_truss(const Cam *c, const Row *p, const Row *q, int row, int parity, bool near_flag);
  void enh_truss_end(const Cam *c, const Row *p, int row);
  ```
- **Guard rail** (`guard_rail` :1007-1023):
  - Front: post -1Eh, n 0Eh, m 0, when R & 8000h and !(C & 80h).
  - The post, when `enh_rail_post`: x from XORq + post / (row + 1) to XORq, y from YK[q] - 1 to Y[q]. Fill 5, then its four edges in colour 2 with `enh_line1`.
  - The rail: the quad (XORp, YEp - m), (XORp, YKp), (XORq, YKq), (XORq, YEq - m), fill 5. Colour-2 lines along its top and bottom.
  - Mirror: post -10, n 7, m 1, the mirror rows and the YK quirk. The parity uses the front's phase (the original's quirk).
- **Truss** (`scenery_draw_truss` :921-937), front:
  - drawn when C[p] & 80h and C[q] & 80h;
  - parity seg - row + 14 (world parity), near when row < 7;
  - lines colour 10, drawn n = `DS_dash_width[row] + 1` times with the original's per-n offsets;
  - the truss end when !(C[p + 1] & 80h).
- **Truss, mirror:**
  - drawn when C[p], C[p+1] and C[p+2] all have 80h;
  - parity seg - row, near when row < 4;
  - the end when !(C[p - 1] & 80h).
- The rail phase in a smooth view is recomputed at its base, falling back to `v->rail_phase`.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_rail_post`: (100, 3, 90, 14) is true (21 odd); (100, 4, 90, 14) is false; (5, 3, 10, 7) is true (-1, as cmod16 gives it).
  - `sr2_enh_line1`: at res 1, aa 1, the line (10,20)-(20,25) colours (10,20) and (20,25) and exactly one sample in each column 10..20. The line (30,40)-(30,40) colours one sample.
  - Compare script:
    - `run police 110` with `SR_DEBUG_RACE=mulholland SR_DEBUG_POLICE=480`;
    - `check mulholland 16 20 enh` (the `noobj` of Task 7 goes) and `check police 16 20 enh`;
    - at least one `frame` line with `police` >= 2 and `opp_sprite` >= 9, printed as `ok   police in the mirror`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: build FAIL, then `FAIL police` (the aid missing).
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the Step 1 command, then `-L slow` (`sr2_enh_compare` and `sr2_smoke`). Expected: all `ok`, `100% tests passed`. Look at snapshots of the rails near the car moving smoothly (no 20 % jump at the near end) and of the bridge's truss.
- [ ] **Step 4: Commit** `git commit -m "Draw Mulholland's rails and bridges, check the police"`. Codecheck: `0 differ`.

### Task 9: The launcher, the docs and the final check

**Files:**
- Modify: `launcher/gameargs.h`, `launcher/gameargs.cpp`. `GameTraits` gains `const char* pictureNote`:
  - SR1: `""`;
  - SR2: `"Street Rod 2: the art, and the race view of the Aqueduct are original for now."`.
- Modify: `launcher/launcher.cpp` :380-381 and :629: the note is the trait; it shows when the trait is not empty.
- Test: `tests/test_args.cpp` `traits_table`:
  - `CHECK(wxString(a.pictureNote).empty())`;
  - `CHECK(wxString(b.pictureNote) == "Street Rod 2: the art, and the race view of the Aqueduct are original for now.")`.
- Modify: `ENHANCED.md` section 9:
  - the "Done" sentence: the town, the street drag, the Grudge drag strip, Mulholland and the mirror; the Aqueduct still original;
  - a mirror paragraph: the quirks reproduced, the side mirror original;
  - the strip and Mulholland;
  - the objects: the id table, the row order, the police in the opponent's slot;
  - the Check paragraph: per mode, the masks, the geometry and re-projection checks, every frame within 1 px. The stale "at least 95 %" goes.
- Modify: `ENGINE2.md` :33-37: the mirror, sky and extras hooks, the SKY layer, `SR_DEBUG_POLICE` in the developer aids.
- Modify: `CHANGELOG.md` Unreleased: the SR2 enhanced entry now covers the Grudge drag strip, Mulholland and the mirror; the Aqueduct is the original picture for now.
- Modify: `README.md` :31 (the feature line) and :213 (SR2's options: `--classic`, `--res-scale`, `--aa`, `--motion-delay`, `--town`).
- Modify: `launcher/README.md` :44-46.
- Modify: the header comments:
  - `src2/enhanced/enhanced.h` :36-38 (the layer comment: the mirror is enhanced);
  - `src2/enhanced/enh_road2.c` :1-6;
  - `tests/test_sr2_enh_compare.sh` :2-5.

- [ ] **Step 1: Update `traits_table`.** Run: `cmake --build build-mac && build-mac/tests/test_args`. Expected: `FAIL traits_table` (no field).
- [ ] **Step 2: Implement** the trait, the launcher change and the docs. Every new sentence uses ASCII hyphen-minus only.
- [ ] **Step 3: Full check.**
  - `cmake --build build-mac && ctest --test-dir build-mac --output-on-failure`. Expected: `100% tests passed`, `sr2_enh_compare` and `sr2_smoke` included.
  - `build-mac/tests/test_args`. Expected: `ok   traits_table`.
  - `tools/sr1-codecheck.sh 50331e9 HEAD --allow host.c.o`. Expected: `... 0 differ (allowed: host.c.o )`.
  - `git diff 50331e9 HEAD | grep -nP '^\+.*[\x{2010}-\x{2015}]'`. Expected: no output (no Unicode dash added anywhere).
  - Screenshot the launcher with SR2 chosen. Back up `settings.ini` first and restore it byte-identical after. If the screen cannot be captured, say so.
- [ ] **Step 4: Commit** `git commit -m "Offer the Grudge and Mulholland race views enhanced"`.
