# Street Rod 2 enhanced race view, part C Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Aqueduct (road mode 8) is drawn by the enhanced race view: its rolled channel and walls, the trough, boxes, pillars, openings, the night sky with its stars and clouds, the Aqueduct's row shapes and the banked opponent turning with the view, the wall bounce, and its rear-view mirror. After part C every Street Rod 2 race kind and its mirror are enhanced, and the compare tool checks the Aqueduct mode by mode.

**Architecture:**
- Part A's and B's design stays: the game draws every frame as before, the enhanced view redraws the background from the race state and re-places the objects the game drew (captured by the tag layer).
- A new module `src2/enhanced/enh_aq2.c` ports `aq_project_front`, `aq_project_mirror`, `aq_draw_sky` and `aq_draw_scene` twice in one code path: with the game's integer arithmetic (exact mode, the compare tool) and in floating point (the smooth view). The roll is a rotation of the projected picture about a pivot, exact from the game's own sine table, smooth from the interpolated roll and pivot.
- The roll, the pivot and the six channel deformations join the per-frame series. A roll step of more than 60 degrees is the bounce's snap back and is not interpolated.
- Captured cuts get a rotation: a cut maps from its frame to the view by a similarity about its rotated anchor, turned by the change of its angle (the view's roll, or the opponent's bank).
- The compare tool gains a line metric (Chebyshev distance of the black and white line pixels), because per-row edges cannot measure the Aqueduct's shallow, rotated lines, and per-frame roll and bounce fields.

**Tech Stack:** C11, SDL3; tests in C (`tests/test_sr2_enh.c`), POSIX sh (`tests/test_sr2_enh_compare.sh`); the launcher in C++17/wxWidgets (`tests/test_args.cpp`).

**Spec:** `docs/superpowers/specs/2026-11-11-street-rod-2-design.md`, section "Enhanced race view". Parts A and B, for the existing design: `docs/superpowers/plans/2026-11-11-sr2-race-view-a.md`, `docs/superpowers/plans/2026-11-12-sr2-race-view-b.md`. `ENHANCED.md` section 9 describes the state after part B.

## Global Constraints

- Street Rod 1 does not change. After every task, `tools/sr1-codecheck.sh 2739cab HEAD --allow host.c.o` must report `0 differ`. Nothing in `src/` or `common/` is edited in part C. New helpers live in `src2/enhanced/`.
- Every game drawing routine keeps running. The enhanced view only captures, never skips the original's drawing: `aq_draw_scene` sets `DS_aq_hit_bits`, `opponent_draw` sets the opponent's sprite state, `aq_draw_front` sets the roll and the next frame's deformation.
- The rear-view mirror is enhanced in every race kind, the Aqueduct's included. The dashboard's side mirror (the page-0 copy of `mirror_finish`) stays the original's picture.
- The original's quirks are reproduced in both exact and smooth mode, so the picture stays the original's:
  - the front rows 12..14 fixed at depth 100, 108, 116; the mirror rows 5..7 fixed at 40, 48, 56;
  - the channel ends at row 14 (nothing is drawn beyond it);
  - the 255/256 shrink of the rotation, also at roll 0;
  - the projection's cam_x clamped to 564h..D5Ch;
  - the mirror's XC with +heading, its wall tops from `DS_aq_wall_l_shift` / `_r_shift`, its flags `AQM[179h - seg - k]`;
  - the mirror starter at raw row `N - i` of the row it is drawn at.
- The stars and clouds (front and mirror) are captured from the game's drawing, never drawn from the point tables or the `SR2_SHP` spans.
- The roll follows the original's snap at a bounce: a roll step whose size, wrapped to -180..180, is more than 60 degrees is a jump (`enh_ser_snap`), never interpolated.
- Remastered `SR2_SHP` shapes are not part of C (plan 5 handles them). Cuts keep their `EnhImage.rm` field (NULL); `enh_draw_image_xf` does not draw art. Nothing reads `rm_vtag`.
- Acceptance (spec), measured with time stopped (exact mode, `SR_ENH_COMPARE`), at scale 1, per mode in the compare log:
  - road edges within +-1 px in every compared frame; in mode 8 measured by the line metric (front and glass);
  - the median frame within 10 per mille of mismatched pixels, every frame within 20;
  - the game's row tables exactly (`geo_max 0`), the Aqueduct's rotated rows and trough included;
  - objects at the same row and size (re-projected cuts within 1 px);
  - for each race kind and for the mirror.
- Original draw distance; no gameplay change. The new developer aid `SR_DEBUG_AQ_WALL` (Task 6) changes nothing when it is unset.
- Lines added to `src2/game/` carry `ENH:` (hooks) or `PORT: developer aid` (the aid).
- No macOS username, no real name or email, no AI attribution, ASCII hyphen-minus only, in anything written. Commit messages: one short imperative subject under about 60 characters, no body, no trailers.
- Work on `master` directly. Never push. Never touch `.worktrees/`: another track works on branch `sr2-art` there.

## Review Focus

1. A wall bounce: the view spins about 32 degrees a frame for eight frames, then snaps back by about 256. The enhanced view must spin with it, snap at the same frame (not turn the long way back), and keep the bounce's vertical offset after it (critic D2). (Task 1 `sr2_enh_aq_roll_unwrap`; Task 3 `sr2_enh_aq_frame_state`; Task 6 `sr2_enh_aq_bounce_persists` and the `aqwall` run.)
2. The roll between 0 and not 0 from one frame to the next (the car reaching a wall, the trough's +-4 kick): the two constructions of the floor and walls (`ROLL == 0` and the rolled one) must not make the picture jump. (Task 4 `sr2_enh_aq_roll_zero_continuous`.)
3. The car on a wall top, cam_x below 564h or above D5Ch: the projection uses the clamped value, the opponent's height and bank the unclamped lane. (Task 1 `sr2_enh_aq_rows_exact` with cam_x 400h and E00h; `sr2_enh_aq_opp_anchor`.)
4. The opponent banked 56 degrees or more on a wall (rotated every frame, variant -1), the King's rotated sprites, and the opponent crossing into the mirror: it shows in one place only, at its row, turning with the view. (Task 1 `sr2_enh_aq_opp_bank` sweep; Task 4 the `aqking` run; Task 7 `sr2_enh_capture_regions`.)
5. Leaving the Aqueduct (the drive home in mode 0, the next race) carries no roll, pivot, bounce or rotated cut into the next view. (Task 3 `sr2_enh_aq_race_resets`; the compare's `check aqueduct 0 0 enh`.)

## Decisions this plan takes (owner rulings are in Global Constraints)

- **A line metric for mode 8.** The channel's lines are shallow even at roll 0 (the floor edges at about 5 degrees, the wall tops at about 9), and floor and walls share colour 5, so a per-row first/last scan is off by a whole run whenever a line is one row off. In mode 8 the per-row edge passes are not run; the line metric measures every covered, non-object pixel of colour 0 or 15 against the nearest pixel of the same colour in the other picture, both ways, Chebyshev distance, capped at 8. `line_err_max <= 1` in every frame is the mode's "edges within 1 px". The distance is measured across a line, not along it (a shallow line moved sideways scores its vertical offset), which is the error the eye sees.
- **Rotation.** Exact mode uses the frame's own `DS_rot_sin`, `DS_rot_cos` and pivot (snapshotted), with `rotate_about_centre`'s arithmetic (`mul8`, floor). Smooth mode uses c = 255 cos(roll), s = 255 sin(roll), divided by 256 (the 255/256 shrink kept), about the interpolated pivot. The mirror's pivot is computed from the view's own mirror rows.
- **Series.** `ENH_SER_N` grows by two pivot channels. `enh_frame_built` writes aq_dy and the six deformations (the values this frame's projection used). `enh_aq_begin`, called by `aq_draw_front` after the rotation, patches the newest sample's roll (unwrapped) and pivot.
- **Constructions keyed on the game's roll value:** the unrotated sky fill, floor fill and wall quads when the view's `roll_raw` is exactly 0, the extended rolled polygons otherwise. `roll_raw` is the unwrapped roll moved back to the game's range by the newest frame's offset (a multiple of 360).
- **Cut mapping.** `M(p) = A_v + s Rot(theta_v - theta_f) (p - A_f)`, s = z_f / z_v, A the anchor rotated as the game rotates it (always for row shapes, clouds and the opponent; for stars and the starter only when the roll is not 0). theta is the view's roll for row shapes, stars and clouds, for the starter the roll once |roll| > 5 (else 0: `drv_rotate` leaves it upright), and for the opponent its bank (`opp_bank_variant`'s formula, recomputed, never the pre-rotated variant's angle), so the exact view maps every cut to itself. Pictures turn the same way as the view: `pic_rotate_planar`'s inverse map (race_aq.c:801-829: u += c, v += s along a row, u -= s, v += c down) is the transpose of `rotate_about_centre`'s matrix. A cut pops when the game changes a pre-rotated variant; that is accepted.
- **Sky cuts.** Stars, clouds and the mirror's stars are one cut each, s = 1, their anchor `(sky_x, 0)`: stars `sc >> 1`, clouds `sc`, mirror stars `-(sc >> 2)`, with `sc = cmod16(shl16(heading, 1), 140h)` (the smooth view adds 2 frac(heading)). They are drawn right after the sky fill (front) or the backdrop (mirror), so the walls cover them as in the original.
- **Object ids.** 24 the stars, 25 the clouds (front, outside the mirror rectangle), 26 the mirror's stars (inside the glass); `ENH_OBJ_IDS` 27. The sky cuts' segment is `INT_MAX` (front) and `INT_MIN` (mirror): the first thing of their pass in `drawn_after`'s far-to-near order, so a row's cut whose rectangle holds star pixels is not taken as covered by them (`cut_covered`). `enh_cuts_at` never draws them; `enh_cut_id` does, right after the sky fill or the backdrop.
- **The sky's border in the line metric.** The far end of the channel (the floor's far edge and the walls' far ends, `Lf`-`Rf`, `TLf`-`Lf`) has no line drawn on it: only the sky (colour 1) meets the concrete there. The front's line metric therefore also measures the sky's border: a covered pixel of colour 1 with a covered 4-neighbour (inside rows y0..y1 - 1) of another colour, matched against such pixels of the other picture. Uncovered neighbours (the foreground, objects) never make a border, so the mirror's frame and the stars do not. The mirror's glass is measured without it (its backdrop is a picture, not one colour).
- **Re-projection in mode 8.** A cut is judged only when its angle did not change between the two frames (and, for the opponent, its sprite and `DS_opp_variant` did not); the sky cuts are never judged. `cut_judged`'s two tests of the cut's bottom against `yg` (enhanced.c:530-532) hold only for an unrotated cut (rot 0): a rotated row cut holds both of its row's shapes, at heights the roll moves apart, and the anchor (world x 0) is off to the side, so its bottom is nowhere near `yg`; a rotated cut is judged on the hidden and overdrawn tests alone. The demo's lanes hold the car on a wall top at a steady roll (48 or -30) and drive the middle lanes at roll 0, so judged frames occur.
- **Anchors' y.** `yg` is the rotated anchor's line y plus `ROW_TOP` (part B's convention, enh_geo2.h:74): the rotation is applied to the game's line coordinates, then `ROW_TOP` added.
- **The opponent's place.** From `DS_opp_last_d` as in part B; pictures 9 and up through the mirror projection (10 - heading, -aq_dy), the others through the front's. The rotation is about the pivot of the pass that draws it (race_opp.c:234 rotates about whatever `DS_rot_cx` / `_cy` hold): the front's when `opp_seg >= seg` (aq_draw_scene :500, :506), the mirror's when `opp_seg < seg` (:503), whatever the picture. So a picture below 9 drawn by the mirror's pass (the opponent just behind, d about -1..-127, the sprite still stepping down) lands in the front view rotated about the mirror's pivot; a picture 9 and up drawn by the front's pass (`opp_seg == seg`) goes into the mirror rectangle and the mirror's backdrop covers it (no cut). Exact mode also repeats `opponent_draw`'s averaging with the last sx (picture 2 off the right side), from `DS_opp_last_sx` read at `enh_frame_built`.
- **The opponent's byte snap.** Pictures below 9 are blitted by `drv_blit_masked` at `r.dx = sx + DS_g_px_shift` (race_opp.c:253), and `clip_rect` keeps whole bytes (gfx.c:164), so the picture's left edge lands at `(sx + g_px_shift) & ~7`, sx being `DS_opp_last_sx` (the rotated sx minus adx). As part B's `enh_opp_drawn_sx` does for the other modes, the exact anchor x takes that snap (`((opp_sx + g_px_shift) & ~7) - opp_sx`, `snap.opp_sx` kept raw in mode 8); without it the re-projection check is off by up to 7 px between two frames. The mirror's pictures go through `bitmap_blit_masked_shift` (pixel exact).
- **Developer aid** `SR_DEBUG_AQ_WALL=<seg>`: from that segment on, the demo autopilot's lane step is replaced by a drift of 40h left at every segment, so the car climbs the left wall and hits it (car_xl below 439h, cam_x below 4A7h, at most about 30 segments later). It is the only way the compare sees a bounce (the demo's lanes never touch 439h / DEDh). A push of 40h added after the lane step would not do: `aq_lane_target` steps back by (target - x) / 7 with no obstacle ahead, so the car settles 1C0h left of its lane and never reaches the wall from the middle lanes. The hit sets the crash bits (race_phys.c:478), so a run sees one bounce.
- **Staging.** Task 3 records the Aqueduct's state with its picture still the game's; Task 4 composes the front view (the mirror stays the game's); Task 7 the mirror.

## File map

| File | Responsibility |
|---|---|
| `src2/enhanced/enh_aq2.h` (new, Task 1) | the Aqueduct's camera, rows, rotation, flags, sky offsets, opponent placement |
| `src2/enhanced/enh_aq2.c` (new, Task 1) | the geometry (Task 1), the front scene (Task 4), cut geometry (Tasks 4, 7), the mirror (Task 7) |
| `src2/enhanced/enh_xf2.c` (new, Task 5) | `enh_draw_image_xf`, an image under an affine map |
| `src2/enhanced/enh_poly2.c`, `.h` | the outlined polygon and the float polygon clip (Task 2) |
| `src2/enhanced/enh_compare.c`, `.h` | the line metric, the new log fields |
| `src2/enhanced/enhanced.c`, `enhanced.h`, `enh_internal.h` | series, snapshot, the `enh_aq_begin` hook, ids, capture, cut drawing, compose and compare for mode 8 |
| `src2/game/race_aq.c` | `ENH:` hooks |
| `src2/game/race_road.c` | the `SR_DEBUG_AQ_WALL` aid |

Line numbers below are those of HEAD 2739cab.

---

### Task 1: The Aqueduct's geometry

**Files:**
- Create: `src2/enhanced/enh_aq2.h`, `src2/enhanced/enh_aq2.c`
- Modify: `src2/enhanced/enh_internal.h`: `EnhView` (:60-72) gains `double roll_raw, piv_x, piv_y; s16 rot_sin, rot_cos, opp_sx_prev;` (0 until Task 3); new `EnhAqTab`.
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Consumes: `enh_proj_x_i`, `enh_proj_y_i`, `enh_proj_x_mirror_i`, `enh_proj_y_mirror_i` (part B); the game's pure helpers `sin256`, `line_x_at_y`, `line_y_at_x`, `cdiv16`, `q_shr`, `shl16` (they read only their arguments and the sine table). Never `rotate_about_centre`, `opp_bank_variant`, `shp_b_draw`, `opp_sprite_aq` or `clip_polygon`: they read or write the game's DS state.
- Produces, in `enh_aq2.h`:
  ```c
  /* race_aq.c's file-local DGROUP addresses: the rotated rows (k = 0 the farthest) and the trough */
  #define AQ_R_LX  0x9732
  #define AQ_R_LY  0x9760
  #define AQ_R_RX  0xB170
  #define AQ_R_RY  0xB1A0
  #define AQ_R_TLX 0x9BC6
  #define AQ_R_TLY 0x9CFC
  #define AQ_R_TRX 0x9DCE
  #define AQ_R_TRY 0x9F0E
  #define AQ_TROUGH_NEAR_XL 0xB16E
  #define AQ_TROUGH_NEAR_XR 0xB872
  #define AQ_TROUGH_FAR_XL  0xB7EA
  #define AQ_TROUGH_FAR_XR  0xB8D8
  #define AQ_TROUGH_NEAR_Y  0xADA6
  #define AQ_TROUGH_FAR_Y   0xB162
  #define ENH_AQ_ROLL_SNAP  60        /* degrees: a larger roll step is the bounce's snap back */
  enum { ENH_AQ_STARS, ENH_AQ_CLOUDS, ENH_AQ_MIR_STARS };
  typedef struct {
      bool exact, mirror, roll0;      /* roll0: roll_raw == 0 (the unrotated constructions) */
      int base, n;                    /* n: the last row, 14 front, 7 mirror */
      double q;                       /* 8 frac(pos) */
      double lat, cx;                 /* DS_cam_x; cx clamped to 564h..D5Ch (cam_x_clamped) */
      double heading, aq_dy;          /* the projection's dx and dy (the mirror negates both, XC excepted) */
      double cam_y, bounce;           /* the mirror: no bounce */
      double def[6];                  /* EnhView.aq_def: floor_h, xl, xr, top_h, top_xl, top_xr */
      s16 wall_l, wall_r;             /* DS_aq_wall_l_shift, DS_aq_wall_r_shift, read at run time */
      double cs, sn, pcx, pcy;        /* the rotation (x256) and its pivot */
      double roll, roll_raw;          /* EnhView.roll (unwrapped), EnhView.roll_raw */
  } AqCam;
  typedef struct { double y, xl, xr, xc, yt, xtl, xtr; } AqRow;
  typedef struct { AqRow r[15]; double tnl, tnr, tfl, tfr, tny, tfy; } AqRows;   /* raw rows, row 0 nearest */
  void enh_aq_cam(const EnhView *v, bool mirror, AqCam *c);   /* the mirror's pivot from its own rows 7 */
  double enh_aq_z(int i, double q);    /* 4; 8i + 4 - q for 1..11; 100, 108, 116 for 12..14 */
  double enh_aq_mz(int i, double q);   /* 2; 8i + q for 1..4; 8i for 5 and up (40, 48, 56) */
  void enh_aq_rows(const AqCam *c, AqRows *r);   /* aq_project_front (rows 0..14) / aq_project_mirror (0..7) */
  void enh_aq_rot(const AqCam *c, double x, double y, double *ox, double *oy);
  u16 enh_aq_flag(const AqCam *c, int i);   /* front TRKW(TRK_AQ, base + i); mirror TRKW(TRK_AQM, 179h - base - (7 - i)) */
  double enh_aq_roll_unwrap(double prev, s16 prev_raw, s16 raw, bool *snap);
  double enh_aq_sky_x(double heading, int which, bool exact);
  double enh_aq_opp_bank(const AqCam *front, double opp_lat);   /* opp_bank_variant's DS_opp_bank, from roll_raw */
  double enh_aq_opp_wy(const AqCam *front, double opp_lat);     /* opponent_draw's wy up the walls */
  /* opponent_draw's place in mode 8: z (|z| + 4 front, -z mirror), sx, sy, then rotated about the pivot of the pass
   * that draws it (mirror_pass: opp_seg < seg, the mirror's; else the front's), whatever the picture; false when
   * not drawn */
  bool enh_aq_opp_anchor(const AqCam *front, const AqCam *mirror, double d, int sprite, double opp_lat, int prev_sx,
                         bool mirror_pass, double *ax, double *ay, double *z);
  void enh_rows_exact_aq(const EnhView *v, bool mirror, EnhRowTab *t);   /* y, xl, xr into their slots, XC into ybl,
                         YT into ye, XTL into xol, XTR into xor_ (race_aq.c's A_*); rows 0..n; yk and ybr untouched */
  ```
- Produces, in `enh_internal.h`:
  ```c
  typedef struct { s16 lx[15], ly[15], rx[15], ry[15], tlx[15], tly[15], trx[15], try_[15];
                   s16 tnl, tnr, tfl, tfr, tny, tfy; s16 roll, sn, cs, cx, cy; } EnhAqTab;
  void enh_aqtab_read(EnhAqTab *t);     /* the AQ_R_ arrays, the trough, DS_rot_enable, _sin, _cos, _cx, _cy now */
  void enh_aqtab_exact(const EnhView *v, bool mirror, EnhAqTab *t);   /* rows 0..n rotated into k = n - i, the
                                           trough unrotated (as the game keeps it), roll/sn/cs/cx/cy of the view */
  int enh_aqtab_err(const EnhAqTab *a, const EnhAqTab *b, int n);    /* largest difference, rows 0..n and trough */
  ```
- Rules the code must follow (race_aq.c:163-223, race_gfx.c:61-68, race_aq.c:139-158, race_opp.c:138-237):
  - Rows: dx = heading, dy = aq_dy; front Y = proj_y(z, def[0] + 2000), XL = def[1] + 5C8h, XR = def[2] + CF8h, XC = mid + def[2], YT = def[3] + 67Ch, XTL = def[4] + 438h, XTR = def[5] + E88h, mid = `(def[2] + def[1] + 12C0h) >> 1` (s16); trough near at z 4 (mid + def[1] - 28h, def[2] + mid + 28h), far at z 116, tny = Y[0], tfy = Y[14].
  - Mirror: dx = -heading, dy = -aq_dy, no bounce, XC with +heading, XTL = wall_l + 438h, XTR = wall_r + E88h, trough near at z 2, far at 56, tfy = Y[7].
  - Exact: every value through the game's s16 operations (`add16`, `sub16`, `>> 1` on s16) and the integer projections with cx clamped. Smooth: the same formulas in doubles.
  - Rotation exact: `ox = (s16)(mul8(dx, cs) + mul8(dy, sn) + pcx)`, `oy = (s16)(mul8(dy, cs) + mul8((s16)-dx, sn) + pcy)`, dx = (s16)(x - pcx), dy = (s16)(y - pcy), `mul8(a, b) = (s16)((s32)a * b >> 8)` (race_gfx.c:58-67). Smooth: `ox = pcx + (dx cs + dy sn) / 256`, `oy = pcy + (dy cs - dx sn) / 256`.
  - `enh_aq_roll_unwrap`: d = raw - prev_raw wrapped to -180..180; `*snap = |d| > ENH_AQ_ROLL_SNAP`; returns prev + d.
  - `enh_aq_sky_x`: sc = `cmod16(shl16(enh_s16_of(h), 1), 0x140)`; exact: stars `sc >> 1`, clouds `sc`, mirror `-(sc >> 2)` (arithmetic shifts); smooth: sc + 2 (h - floor h), then sc / 2, sc, -sc / 4.
  - Bank: as `opp_bank_variant` with sxl = def[1], sxr = def[2] (the saved values are this frame's def); exact s16 and `roll >> 2`; smooth: e clamped 0..100, times 0.45, roll / 4; the roll term only while -5Ah < roll < 0 (race_aq.c:149); both wrap at +-167h.
  - wy: `def[0] + 2000`, lifted on the left wall when `lat < sxl + 5C8h` by `(s16)(sxl - lat + 5A0h) * th / (s16)(400 - wall_l + sxl)`, on the right when `lat > sxr + CF8h` by `th * (s16)(lat - sxr - CF8h) / (s16)(400 - sxr + wall_r)`, th = def[3] - def[0] + 154h; exact truncating s32 division, 0 when the divisor is 0.
  - Anchor: z = `cdiv16(d, 8)` (smooth d / 8), +16 for sprite 0, +8 for sprite 2 when z < -2. Pictures below 9: z = |z| + 4, `sx = proj_x(z, opp_lat, heading)`, `sy = proj_y(z, wy, aq_dy)` with the bounce; exact only: sprite 2 with sx > 13Fh and prev_sx < 200 takes `sx = (sx + prev_sx) / 2`; pictures 2 and 3 clamp sy to 100. Pictures 9 and up: z = -z (smooth: false when z < 1), the mirror projections with dx `10 - heading`, dy `-aq_dy`. Then rotated with `mirror` when `mirror_pass`, else with `front` (both kinds of picture). Pictures above 12 or below 0: false. The byte snap of pictures below 9 is not applied here (it needs the drawn sx): `enh_obj_geo_aq` adds it in exact mode.

- [ ] **Step 1: Write the failing tests** in `tests/test_sr2_enh.c` (each restores every DS word it sets):
  - `sr2_enh_aq_depths`: `enh_aq_z(i, q)` for q in {0, 3, 7} is {4, 12 - q, 20 - q, ..., 92 - q, 100, 108, 116}; `enh_aq_mz(i, q)` is {2, 8 + q, 16 + q, 24 + q, 32 + q, 40, 48, 56}.
  - `sr2_enh_aq_rows_exact`: `DSS(DS_road_mode) = 8`, `DS_cam_y` 76Ch. For each case (cam_x, heading, substep, cur_height, aq_pitch, bounce, def[6]):
    - (A00h, 0, 0, 0, 0, 0, {0, 0, 0, 0, 0, 0});
    - (400h, 7, 40, 0, 2, 0, {157, 45, 45, 0, 0, 45});
    - (E00h, -12, 63, 0, -3, 10, {102, -408, -408, -34, -272, 102});
    - (A00h, 3, 16, 0, 1, 36, {20, 10, -30, 50, 0, 0}) (the bounce, D2).

    Set the DS words (`DS_aq_def_*`, `DS_cur_height`, `DS_aq_pitch`, `DS_heading`, `DS_cam_x`, `DS_substep`, `DS_view_bounce_y`), call `aq_project_front()`, and build an exact `EnhView` with lat = cam_x, heading, pos = seg + (substep >> 3) / 8, aq_dy = cur_height - pitch, aq_def, bounce, cam_y. `enh_rows_exact_aq(&v, false, &t)` equals `DSS(DS_row_y + 2i)` (y), `DS_row_xl`, `DS_row_xr`, `DS_row_ybank_l` (XC), `DS_row_yeye` (YT), `DS_row_xol` (XTL) and `DS_row_xor` (XTR) for i 0..14; `enh_aq_rows` gives the six trough values of `AQ_TROUGH_*`.
  - The exact views of these tests set `rot_sin`, `rot_cos`, `piv_x`, `piv_y` and `roll_raw` by hand (Task 3 fills them from the snapshot).
  - `sr2_enh_aq_mirror_rows_exact`: the same cases (`aq_project_front()` first: it sets `DS_aq_dy` and `DS_aq_dx`), then `DS_aq_saved_floor_h` / `_xl` / `_xr` / `_top_h` = def[0..3], `DS_aq_wall_l_shift` 32, `DS_aq_wall_r_shift` -48, `aq_project_mirror()`. `enh_rows_exact_aq(&v, true, &t)` equals rows 0..7 of the seven tables, and the trough values match.
  - `sr2_enh_aq_rot`: for roll in {0, 4, -4, 30, 48, -30, 104, -256} with `DS_rot_sin` = `sin256(roll)`, `DS_rot_cos` = `sin256(roll + 90)`, pivot (7Ah, 3Fh), and points x in {-900, -20, 0, 7Ah, 160, 13Fh, 700}, y in {-200, 12h, 3Fh, 65h, 400}: the exact `enh_aq_rot` equals `rotate_about_centre`. The smooth one at the same roll is within 2 px for points with |x - 7Ah| and |y - 3Fh| at most 320.
  - `sr2_enh_aq_roll_unwrap`:
    - (0, 0, 48) gives 48, no snap; (48, 48, 80) gives 80, no snap;
    - (332, 332, 4) gives 364, no snap (the game's +-167h wrap);
    - (304, 304, 48) gives 408 with snap (the left bounce: -256 wraps to +104);
    - (-302, -302, -30) gives -390 with snap (the right bounce: +272 wraps to -88);
    - (0, 0, -4) gives -4, no snap (the trough's kick).
  - `sr2_enh_aq_sky_x`: exact (stars, clouds, mirror stars) for heading 5: (5, 10, -2); -5: (-5, -10, 3); 170: (10, 20, -5); 16400: (-48, -96, 24). Smooth at 5.5: (5.5, 11, -2.75).
  - `sr2_enh_aq_flags`: after `aq_build()`, a front cam at base 100: `enh_aq_flag(c, i) == TRKW(TRK_AQ, 100 + i)` for i 0..14, and equals the word `aq_flag(14 - i)` reads after road_frame's pointer set-up (`trk_ptr(TRK_AQ, 100)`, offset + 1Ch - 2k). A mirror cam: `== TRKW(TRK_AQ, 100 - 1 - i)` for i 0..7 (the critic's identity) and the word read through `far_make(SEG(TRK_SEG), 0x6570 - 2 * 100)` + 10h - 2k with k = 7 - i.
  - `sr2_enh_aq_opp_bank`: for opp_lat in {400h, 5A0h, 600h, 900h, CC0h, D40h, E00h}, sxl = sxr in {0, 40, -60} and roll in {0, 48, -30, -100, 200}: set `DS_opp_lat`, `DS_aq_saved_xl`, `DS_aq_saved_xr`, `DS_rot_enable`, call `opp_bank_variant()`; `enh_aq_opp_bank` of an exact cam (def[1] = sxl, def[2] = sxr, roll_raw = roll) equals `DSS(DS_opp_bank)`. Some cases give |bank| >= 56 (variant -1).
  - `sr2_enh_aq_opp_wy`: def all 0, wall shifts 0: lat 500h gives 1864, D00h gives 1994, A00h gives 2000. def {20, 10, 0, 50, 0, 0}, wall_l 20h: lat 500h gives 1854.
  - `sr2_enh_aq_opp_anchor`: an exact front cam (case 3 of the rows test, roll 48 with its `sin256` values, pivot (7Ah, 3Fh)) and its mirror cam. Sprite 5, d 300, front pass: z == 41 and (ax, ay) equals `road_proj_x(41, lat, heading)`, `road_proj_y(41, wy, aq_dy)` rotated by `rotate_about_centre` with the same DS state. Sprite 2 with sx beyond 13Fh and prev_sx 150 gives the averaged x. Sprite 10, d -100, mirror pass: z == 12, the mirror projections with dx `10 - heading`, rotated about the mirror cam's pivot. Sprite 3, d -40, mirror pass (the opponent just behind, still a rear picture): z == 9, the front projections rotated about the mirror cam's pivot (`DS_rot_cx` / `_cy` set to it for the reference `rotate_about_centre`), not the front's.
  - `sr2_enh_aqtab_exact`: rows test case 3 with the rotation of `sr2_enh_aq_rot`'s roll 48: after `aq_project_front()` the test fills the `AQ_R_*` arrays as `aq_rotate_rows(14)` does (`rotate_about_centre` of raw row i into k = 14 - i) and `enh_aqtab_read`; `enh_aqtab_err(&mine, &read, 14) == 0` with `enh_aqtab_exact(&v, false, &mine)`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh$' --output-on-failure`
  Expected: build FAIL (`enh_aq2.h` not found).
- [ ] **Step 2: Implement** `enh_aq2.h` and the geometry part of `enh_aq2.c` as in the Interfaces and their rules.
- [ ] **Step 3: Run** the Step 1 command. Expected: `100% tests passed`; with `-V`, every new `sr2_enh_aq_*` line `ok`. Then `ctest --test-dir build-mac -LE slow`. Expected: `100% tests passed`.
- [ ] **Step 4: Commit** `git commit -m "Project the Aqueduct for the enhanced view"`, then `tools/sr1-codecheck.sh 2739cab HEAD --allow host.c.o`. Expected: `0 differ`.

### Task 2: The line metric and the outlined polygon

**Files:**
- Modify: `src2/enhanced/enh_compare.h` (:7-15 the struct, the new function), `src2/enhanced/enh_compare.c` (:118-145 the frame line, :183-201 the mode lines, `ModeStats`)
- Modify: `src2/enhanced/enh_poly2.h`, `src2/enhanced/enh_poly2.c`
- Modify: `src2/enhanced/enhanced.c` `compare_frame` (:591-687): `bounce`
- Test: `tests/test_sr2_enh.c` (`sr2_enh_compare_log` :1268-1311 and new tests)

**Interfaces:**
- Produces:
  ```c
  /* enh_compare.h; EnhCompare gains, after opp_sprite: */
  int line_px, line_err_max;        /* front: the line metric's pixels and its largest distance (0 outside mode 8) */
  int m_line_px, m_line_err_max;    /* the same over the mirror's glass */
  int roll, bounce;                 /* DS_rot_enable of the frame (mode 8; else 0), DS_view_bounce_y */
  #define ENH_LINE_CAP 8
  /* Every covered pixel of either picture whose colour is one of cols, against the nearest covered pixel of the same
   * colour in the other picture (Chebyshev distance, ENH_LINE_CAP when none is that near); and, when border >= 0,
   * every covered pixel of colour `border` with a covered 4-neighbour (rows y0..y1 - 1) of another colour, against
   * the nearest such border pixel of the other picture. *px: such pixels in orig; *err: the largest distance. Rows
   * y0..y1 - 1 of 320-wide pictures; `covered` is the one mask of both pictures. */
  void enh_compare_lines(const u8 *orig, const u8 *enh, const u8 *covered, int y0, int y1, const u8 *cols, int n,
                         int border, int *px, int *err);
  /* enh_poly2.h: poly_fill_pts with an edge colour, on a polygon already clipped (game coordinates: x the pixel, y
   * the line y): filled as enh_fill_poly_incl fills it (y + 0.5), then each edge as enh_line1 in `edge`; edge == fill
   * fills only. */
  void enh_fill_poly_edge(const double *x, const double *y, int n, u8 fill, u8 edge);
  /* clip_polygon's Sutherland-Hodgman in doubles (no truncation), on xmin..xmax, ymin..ymax, 64-point buffers */
  int enh_clip_poly_f(const double *x, const double *y, int n, double *ox, double *oy,
                      double xmin, double xmax, double ymin, double ymax);
  ```
- Log: every frame line ends with ` line_px %d line_err_max %d mirror_line_px %d mirror_line_err_max %d roll %d bounce %d`; every mode line ends with ` line_worst %d mirror_line_worst %d` (the largest `line_err_max`, `m_line_err_max`). The totals line is unchanged.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_compare_lines` (10 rows x 320, all covered, cols {0}, border -1 unless said):
    - the same vertical line of colour 0 (x 50, rows 0..9) in both: `px` 10, `err` 0;
    - the enhanced line one column right: `err` 1; three columns right: `err` 3;
    - one colour-0 pixel only in the enhanced picture, nothing near: `err` 8;
    - the same pixel left uncovered: `err` 0;
    - border 1: rows 0..4 colour 1 and 5..9 colour 5 in the original, the enhanced picture's boundary one row lower:
      `err` 1; three rows lower: `err` 3; the same boundary in both, with column 200 of the original uncovered and
      colour 7: `err` 0 (an uncovered neighbour makes no border).
  - `sr2_enh_poly_edge`: three polygons inside the view at res 1, aa 1:
    - a triangle with a shallow edge (10,20)-(200,40)-(30,60);
    - a quad (50,30)-(90,25)-(95,70)-(45,80);
    - a thin quad 2 px wide (120,20)-(121,20)-(130,80)-(129,80).

    Each is drawn by the game's `poly_fill(pts, n, 2, 0)` (declare `void poly_fill(const s16 *, s16, s16, s16);` in the test: race_gfx.c's slot-18 driver; `DSW(DS_draw_seg) = 0xA400`, `DSB(0x81DE) = 0x28`, `DSW(0x81DF) = 0`, after `vga_mode_0d()`) into page 2, and by `enh_fill_poly_edge(.., 2, 0)` into the buffer. Read both pictures into 320-wide arrays. `enh_compare_lines` with cols {0} and with cols {2} (border -1) gives `err <= 1` both ways, and `px` > 0 for each polygon (an empty enhanced fill scores 8).
  - `sr2_enh_clip_poly_f`: the four polygons of `sr2_enh_clip_poly_i` give the same point count as `enh_clip_poly_i`, every coordinate within 1.
  - `sr2_enh_compare_log`: frames a, b, c keep their values (the new fields 0), and frame d is new:
    `EnhCompare d = { .rows = 84, .pixels = 26880, .mismatched = 81, .m_rows = 21, .m_pixels = 1596, .m_mismatched = 8, .mirror_cov = 1596, .obj_err = -1, .opp_sprite = 5, .line_px = 900, .line_err_max = 1, .m_line_px = 120, .m_line_err_max = 0, .roll = 48, .bounce = 12 };`
    logged as `enh_compare_frame(8, &d)` after c and before `enh_compare_skipped(16)`. The expected text: frames 1..3 as now, each line ending ` line_px 0 line_err_max 0 mirror_line_px 0 mirror_line_err_max 0 roll 0 bounce 0`, then
    ```
    frame 4 mode 8 rows 84 edge_rows 0 edge_err_max 0 mismatch 3 mirror_rows 21 mirror_edge_rows 0 mirror_edge_err_max 0 mirror_mismatch 5 mirror_cov 1596 geo_err 0 obj_err -1 police 0 opp_sprite 5 line_px 900 line_err_max 1 mirror_line_px 120 mirror_line_err_max 0 roll 48 bounce 12
    ```
    the mode lines of 0, 1 and 16 as now ending ` line_worst 0 mirror_line_worst 0`, between 1 and 16
    ```
    mode 8 frames 1 skipped 0 worst_edge 0 mismatch_p50 3 mismatch_max 3 mirror_worst_edge 0 mirror_mismatch_p50 5 mirror_mismatch_max 5 mirror_cov_min 1596 geo_max 0 obj_max 0 shown 0 line_worst 1 mirror_line_worst 0
    ```
    and last `frames 4 skipped 1 shown 0`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh$' --output-on-failure`. Expected: build FAIL (`enh_compare_lines` undeclared).
- [ ] **Step 2: Implement** as in the Interfaces. `enh_compare_frame` and `enh_compare_close` print the new fields; `compare_frame` (enhanced.c) fills `bounce` from the newest view in every mode, the rest stays 0 until Task 4.
- [ ] **Step 3: Run** the Step 1 command, then `ctest --test-dir build-mac -LE slow`, then `ctest --test-dir build-mac -L slow --output-on-failure`. Expected: `100% tests passed` each time; the compare script's lines as after part B (`ok   aqueduct mode 8 not enhanced` among them).
- [ ] **Step 4: Commit** `git commit -m "Add a line metric to Street Rod 2's compare tool"`. Codecheck: `0 differ`.

### Task 3: The Aqueduct's frame state

**Files:**
- Modify: `src2/enhanced/enh_internal.h`: the channel enum (:12-13), `EnhSnap` (:31-51), the `EnhView` fields of Task 1 filled.
- Modify: `src2/enhanced/enhanced.h`: `enh_aq_begin`; the id comments (:49-54) say 24 and 25 are the Aqueduct's.
- Modify: `src2/enhanced/enhanced.c`:
  - `mode_enhanced` (:127) takes 8;
  - `enh_race_begin` (:129-140) clears `snap.aq_front`, `snap.aq_mirror`;
  - `enh_frame_built` (:156-185), `enh_aq_begin` (new), `enh_mirror_begin` (:383-388);
  - `snap_fill` (:413-436), `enh_frame_presented` (:438-460), `view_of` (:462-490);
  - `enh_tag_kept` (:236-241).
- Modify: `src2/game/race_aq.c`: `#include "enhanced/enhanced.h"   /* ENH: the enhanced race view */`; in `aq_draw_front` after `clip_set_view();` (:626) `enh_aq_begin();   /* ENH: the Aqueduct's frame, its roll and pivot */`.
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Consumes: `enh_aq_roll_unwrap`, `enh_aqtab_read`, `EnhAqTab` (Task 1).
- Produces:
  ```c
  enum { ..., ENH_F_DEF0, ENH_F_PIV_X = ENH_F_DEF0 + 6, ENH_F_PIV_Y, ENH_SER_N };   /* enh_internal.h */
  /* enhanced.h: aq_draw_front (race_aq.c), after the roll and the rotated rows: the Aqueduct's frame into the back
   * page starts (as enh_draw_begin), and its roll and pivot join the newest snapshot. */
  void enh_aq_begin(void);
  ```
  - `EnhSnap` gains: `EnhAqTab aq_front` (read at `enh_aq_begin`), `EnhAqTab aq_mirror` (read at `enh_frame_presented`), `s16 opp_sx_prev` (`DS_opp_last_sx` at `enh_frame_built`, before `opponent_draw` writes it), `s16 opp_variant` (`DS_opp_variant` at presentation), `bool opp_mpass` (`DS_opp_seg < DS_seg` at presentation: the mirror's pass drew the opponent); `EnhView` gains `opp_mpass` from it. `snap_fill` keeps `snap.opp_sx` raw in mode 8 (its byte snap is `enh_obj_geo_aq`'s, Task 4).
  - `enh_frame_built`, in mode 8: `ENH_F_AQ_DY` = `DS_aq_dy`, `ENH_F_DEF0..+5` = `DS_aq_def_floor_h`, `_xl`, `_xr`, `_top_h`, `_top_xl`, `_top_xr` (this frame's projection used them; `aq_draw_front` zeroes them later), `ENH_F_HOFF` 0; the roll and pivot channels carry the previous sample's values (a picture composed before the hook stays continuous).
  - `enh_aq_begin`: does `enh_draw_begin`'s work; then the newest sample's `ENH_F_ROLL` = `enh_aq_roll_unwrap(v[1][ROLL], last_raw, DS_rot_enable)` (the raw value itself when the series has one sample), `enh_ser_snap(&frames, ENH_F_ROLL)` when it reports a snap; `ENH_F_PIV_X/Y` = `DS_rot_cx`, `DS_rot_cy`; `enh_aqtab_read(&snap.aq_front)`.
  - `view_of`: `piv_x`, `piv_y` from the channels; `roll_raw = roll + (snap.aq_front.roll - frames.v[0][ENH_F_ROLL])`; `rot_sin`, `rot_cos` = `snap.aq_front.sn`, `.cs`; `opp_sx_prev` from the snapshot.
  - `enh_frame_presented` in mode 8: fills the snapshot (with `enh_aqtab_read(&snap.aq_mirror)`) and returns with `presented = false`, no capture and no compare: the Aqueduct's picture stays the game's until Task 4.
  - `enh_mirror_begin` in mode 8: base layer FG (the Aqueduct's mirror stays the game's until Task 7).
  - `enh_tag_kept`: `ENH_OBJ_STARS` and `ENH_OBJ_CLOUDS` are front ids (kept outside the mirror rectangle).

- [ ] **Step 1: Write the failing tests:**
  - An `aq_setup()` helper: as `race_setup` with `DS_road_mode` 8, `aq_build()`, `DS_cam_x` A00h, `DS_cam_y` 76Ch, the wall shifts 0, then `enh_race_begin()`. A helper `aq_frame(def[6], aq_dy, bounce, roll, cx, cy)` sets the DS words, calls `enh_frame_built()`, sets `DS_rot_enable`, `DS_rot_sin` / `_cos` (`sin256`), `DS_rot_cx` / `_cy`, calls `enh_aq_begin()` and `enh_frame_presented()`.
  - `sr2_enh_aq_frame_state`:
    - one frame with def {1, 2, 3, 4, 5, 6}, aq_dy 3, bounce 0, roll 48, pivot (50h, 3Fh): `enh_view_newest` gives roll 48, roll_raw 48, piv (50h, 3Fh), aq_def {1..6}, aq_dy 3, `rot_sin == sin256(48)`, `rot_cos == sin256(138)`; `CHECK(!enh_compose(small, rgb))` (Task 4 turns it into `CHECK(enh_compose(..))`);
    - then rolls 80, 112, ..., 304 (+32 each): the newest roll follows them, `enh_view_at` between the last two lies strictly between;
    - then roll 48: the newest roll 408, roll_raw 48, and `enh_view_at` at any time gives 408 (the snap);
    - a write after `enh_mirror_begin()` sets fg.
  - `sr2_enh_aq_race_resets`: after a roll-48 frame, `enh_race_end()`, mode 0, `DSB(DS_racing) = 0`, `enh_race_begin()`, `enh_frame_built()`, `enh_frame_presented()`: the newest view has roll 0, roll_raw 0, piv 0, aq_def all 0.
  - `sr2_enh_capture_regions` gains: `enh_tag_kept(ENH_OBJ_STARS, 0, 0x80, 0x20)` true, `(ENH_OBJ_STARS, 0, 0x100, 0x20)` false, the same two for `ENH_OBJ_CLOUDS`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh$' --output-on-failure`. Expected: build FAIL (`enh_aq_begin` undeclared).
- [ ] **Step 2: Implement** as in the Interfaces.
- [ ] **Step 3: Run** the Step 1 command, `-LE slow` and `-L slow`. Expected: `100% tests passed`; the compare script still prints `ok   aqueduct mode 8 not enhanced` and `ok   aqueduct mode 0`.
- [ ] **Step 4: Commit** `git commit -m "Keep the Aqueduct's roll in the enhanced view's state"`. Codecheck: `0 differ`.

### Task 4: The Aqueduct's front view

**Files:**
- Modify: `src2/enhanced/enh_aq2.c`, `enh_aq2.h` (the scene, `enh_draw_aq`, `enh_obj_geo_aq`)
- Modify: `src2/enhanced/enh_internal.h`: `EnhObjGeo` (:138) gains `bool xf; double rot;`; the cut enum (:131) gains `ENH_CUT_STARS, ENH_CUT_CLOUDS`; `enh_cut_id`.
- Modify: `src2/enhanced/enhanced.c`:
  - `cut_kind` / `cut_pass` (:243-252) and `capture_objects` (:282-332) for the sky kinds (seg `INT_MAX`, front pass; `cut_kind` maps 24 and 25 before its `id > ENH_OBJ_OPP` mirror branch);
  - `enh_cut_id` (new) and `enh_cuts_at` (:358-365: never the sky kinds);
  - `draw_view` (:369-374): mode 8 draws `enh_draw_aq` only;
  - `enh_frame_presented`: mode 8 composed again;
  - `ObjCut` and `CutPred` gain `variant` and `rot`; `cut_judged` (:523-533) and `reprojection_err` (:559-575) take the mode-8 rule (Decisions: equal rot, the bottom tests only at rot 0);
  - `compare_frame` (:591-687): the mode-8 branch.
- Modify: `src2/enhanced/enh_road2.c` `enh_obj_geo` (:462): in mode 8 it returns `enh_obj_geo_aq` first, before its dispatch of the mirror kinds to `enh_obj_geo_mirror` (:465-466), which knows nothing of mode 8.
- Modify: `src2/game/race_aq.c`, each line `ENH:`:
  - `aq_draw_night_sky` (:235-243): `enh_draw_objects(ENH_OBJ_STARS)` before the first `drv_plot_points`, `enh_draw_objects(ENH_OBJ_CLOUDS)` before the first `shp_b_draw`, `enh_draw_objects(0)` at the end;
  - `aq_draw_scene` row shapes (:448-451): `if (N == 14) enh_draw_objects(i);` before the two `if`s, `if (N == 14) enh_draw_objects(0);` after them;
  - the front opponent (:500, :506): `enh_draw_objects(ENH_OBJ_OPP); opponent_draw(); enh_draw_objects(0);` as a braced block.
- Modify: `tests/test_sr2_enh_compare.sh`
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Consumes: Task 1's geometry, `enh_fill_poly_edge`, `enh_clip_poly_f`, `enh_compare_lines` (Task 2); `enh_clip_poly_i`, `enh_clip_line_i`, `enh_fill_poly_incl`, `enh_line1` (part B); the snapshot of Task 3.
- Produces:
  ```c
  void enh_draw_aq(const EnhView *v);       /* aq_draw_sky + aq_draw_scene(14), cuts at their place */
  EnhObjGeo enh_obj_geo_aq(const EnhView *v, int kind, int seg, int sprite);
  void enh_cut_id(const EnhView *v, int id);   /* draws the frame's cut of id, if it has one not yet drawn */
  ```
  - `EnhObjGeo` with `xf`: (a, yg) is the rotated anchor, rot its angle in degrees, `b` = 4 / z (front), so part B's axis-aligned mapping in `draw_cut` is exact while the angle does not change (Task 5 adds the rotation).
  - `enh_obj_geo_aq`, front kinds:
    - ROW at seg: k = seg - base, z = `enh_aq_z(k, q)` for 0..14, else 8 (seg - pos) + 4 (not ok below 1); anchor = rotated (x of world 0, y of `def[0] + 2000`) at z; rot = roll;
    - OPP: `enh_aq_opp_anchor` with `v->opp_d`, `v->opp_sprite`, `v->opp_lat`, `v->opp_sx_prev`, `v->opp_mpass`; b = (sprite < 9 ? 4 : 1) / z; rot = `enh_aq_opp_bank`; in exact mode a picture below 9 adds the byte snap to a (`((v->opp_sx + DSS(DS_g_px_shift)) & ~7) - v->opp_sx`);
    - STARS: anchor (sky_x stars, 0), rotated only when not roll0; CLOUDS: (sky_x clouds, 0), always rotated; z = 1, b = 1, rot = roll;
    - the mirror kinds (MIR, STARTER, MIR_STARS, OPP with sprite >= 9 drawn in the mirror): not ok until Task 7 (the mirror is the game's picture until then and holds no cuts: its foreground clears the tags).
- The scene (`enh_draw_aq`), a port of `aq_draw_sky` and `aq_draw_scene` (race_aq.c:255-265, 392-507), statement by statement, with these substitutions:

  | the game's | the enhanced view's |
  |---|---|
  | `RW(A_*, i)` | `enh_aq_rows` of the view |
  | `rp(R_*, k)`, `rot()` | `enh_aq_rot` of raw row n - k |
  | `poly4(q, f, e)`, `clip_polygon` + `poly_fill_pts` | exact: `enh_clip_poly_i` on the pass's game clip (front 0..13Fh x 12h..65h), then `enh_fill_poly_edge`; smooth: `enh_clip_poly_f` on the same, then `enh_fill_poly_edge` |
  | `clip_line(.., c)` | exact: `enh_clip_line_i` on the game clip, then `enh_line1`; smooth: `enh_line1` |
  | `drv_fill_rows(y, c)` | `enh_fill_rows(y, ymax, c)`, rows y..ymax - 1: ymax 65h, or the pivot's y (`rot_cy`, Y[14]) for the sky when roll0 |
  | `aq_flag(k)` | `enh_aq_flag(c, n - k)` |
  | the s16 helpers (`shl16`, `sub16`, `cdiv16`, `q_shr`, `abs16`, `line_*_at_*`) | themselves in exact mode; plain doubles in a smooth view |
  | hit-bit writes (`DS_aq_hit_bits`) | dropped (the game's own drawing keeps setting them) |
  | `aq_draw_row_shape` at row i (step 2) | `enh_cuts_at(v, FRONT, base + i, base + i, false)` once per k (row 14 included: a smooth view behind the newest frame) |
  | `opponent_draw` at row i (step 4), at i == 0 after it | `enh_cuts_at(v, FRONT, base + i, base + i, true)`; after the loop `(INT_MIN, base)` for rows and the opponent |
  | after `aq_draw_night_sky` | `enh_cut_id(v, ENH_OBJ_STARS)`, `enh_cut_id(v, ENH_OBJ_CLOUDS)` |

  Before the band loop, after the wall lines: `enh_cuts_at(v, FRONT, base + 15, INT_MAX, false)` and `(.., true)`.

  An opponent picture below 9 drawn by the mirror's pass (`opp_mpass`: opp_seg behind the car) is a front cut (kept outside the mirror rectangle, `cut_pass` front) at a segment below base: the front scene draws it with the cuts after the loop, as the original draws it after the front view (from the mirror's pass, on the view's clip). Until Task 7 the game draws it in the foreground (its hook is Task 7's), so it stays the game's pixels.

- `compare_frame` in mode 8:
  - the two mismatch passes as now; the per-row edge passes are not run (edge fields 0);
  - `enh_compare_lines` with cols {0, 15} and border 1 (the sky) over `cov_fe` rows 12h..y_end - 1 (y0 12h, y1 y_end) into `line_px` / `line_err_max`, and with cols {0, 15}, border -1 over `cov_me` rows 16h..2Ah (y0 16h, y1 2Bh) into `m_line_px` / `m_line_err_max`;
  - `geo_err`: `rows = snap.game_front`, `enh_rows_exact_aq(&v, false, &rows)`, `rows_err(&rows, &snap.game_front, 15)`, and `enh_aqtab_err` of `enh_aqtab_exact(&v, false, ..)` against `snap.aq_front` over rows 0..14; the mirror's stays out until Task 7;
  - `roll` = `snap.aq_front.roll`;
  - the dump's "bad" frames include `line_err_max > 1` and `m_line_err_max > 1`.
- Re-projection in mode 8: a cut is judged as in part B, and only when the old and new geometry's `rot` are equal; the opponent also needs the same `variant` (`snap.opp_variant`); sky kinds never. `cut_judged`'s tests of the bottom against `yg` (:530-532) apply only to a cut of rot 0 (Decisions).
- `tests/test_sr2_enh_compare.sh`:
  - `check` takes the options after `<orig|enh>` as a list: `noobj` as now, and `lines`. With `lines`, `line_worst <= 1` is required, and `mirror_line_worst <= 1` with `enh`; "measured something" counts frames with `line_px > 0` (and `mirror_line_px > 0` with `enh`) instead of `edge_rows` / `mirror_edge_rows`.
  - `rolled <kind> <mode> <min>`: at least min frame lines of the mode with `roll` not 0; prints `ok   <kind> mode <m> rolled (<n> frames)` or `FAIL ...`.
  - `run aqking 80 aqueduct SR_DEBUG_OPP=21` (the King's rotated sprites).
  - `none aqueduct 8` becomes `check aqueduct 8 20 orig lines` and `rolled aqueduct 8 20`; new `check aqking 8 20 orig lines`.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_aq_frame_state` (Task 3): `CHECK(enh_compose(small, rgb))` now.
  - `sr2_enh_aq_roll_zero_continuous`: `aq_setup()`, seg 100 (`aq_build` flags), res 1, aa 1. A smooth view (exact false) with roll_raw 0 and roll 0, and the same with roll and roll_raw 1e-6 (the rolled construction), each drawn by `enh_draw_aq` (no cuts) into a 320-wide picture. Over rows 12h..63h: `enh_compare_rows` mismatch at most 10 per mille, `enh_compare_lines` cols {0, 15}, border 1, err at most 1 (both constructions put the far end and the walls' tops in the same place).
  - `sr2_enh_aq_cut_geo`: the exact newest view of an `aq_frame` with roll 48: `enh_obj_geo(&v, ENH_CUT_ROW, base + 5, 0)` is ok, xf, rot 48, z `enh_aq_z(5, q)`, yg the rotated row-5 floor y plus `ROW_TOP`; `ENH_CUT_OPP` with sprite 5 and opp_d 300 has z 41 and rot `enh_aq_opp_bank`, and its a moves by the byte snap: with `DS_opp_last_sx` 101 at presentation (`DS_g_px_shift` 3) a is the rotated sx plus 3 (104 & ~7 = 104), with 100 the rotated sx minus 4 (103 & ~7 = 96); `ENH_CUT_STARS` has z 1 and a = the rotated `enh_aq_sky_x`.
  - The compare script as in the Interfaces.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: build FAIL, then `FAIL aqueduct mode 8 (no frames)`.
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the Step 1 command, then `-L slow`. Expected: all `ok`, among them `ok   aqueduct mode 8 (mode 8 frames ...)` with `mirror_cov_min 0`, `geo_max 0`, `obj_max` 0 or 1 and `line_worst` 0 or 1, `ok   aqueduct mode 8 rolled (...)`, `ok   aqking mode 8 (...)`, and `100% tests passed`.
  - The demo's roll on the walls is 48 or -30, so rolled frames are common. If any frame shows `line_err_max` over 1, `mismatch` over 20 or `geo_err` above 0, use superpowers:systematic-debugging on its dump (`SR_ENH_COMPARE_DUMP` in a scratch folder outside the repository) before going on: `geo_err` first (rows, then the rotated table), then the lines. The thresholds are not loosened without an owner ruling.
  - If no mode-8 frame has `obj_err >= 0`, report it to the owner rather than adding `noobj`.
  - Look at snapshots at `--res-scale 4` (`SR_SNAPSHOT_DIR` in a scratch folder; `SR_DEBUG_RACE=aqueduct SR_KEYS=1:03 demo v`, the game folder is `SRE_TEST_GAME2_DIR` in `build-mac/CMakeCache.txt`): the rolled channel, the stars and clouds, the boxes and pillars, the opponent; the mirror still the game's.
- [ ] **Step 4: Commit** `git commit -m "Draw the Aqueduct's channel enhanced"`. Codecheck: `0 differ`.

### Task 5: Objects turning with the view

**Files:**
- Create: `src2/enhanced/enh_xf2.c`
- Modify: `src2/enhanced/enh_internal.h` (`enh_draw_image_xf`), `src2/enhanced/enhanced.c` `draw_cut` (:344-356) and `predict_cuts` (:537-556)
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Consumes: `EnhObjGeo.xf`, `.rot` (Task 4).
- Produces:
  ```c
  /* The image whose top-left texel corner is at (x0, y0) in the frame, drawn through the affine map m: a frame point
   * p goes to (m[0] p.x + m[1] p.y + m[2], m[3] p.x + m[4] p.y + m[5]). Each sample's centre is mapped back and takes
   * the texel it lands on (nearest), ENH_CLEAR skipped, under the current clip. img->rm is not drawn (no remastered
   * art rotated yet). */
  void enh_draw_image_xf(const EnhImage *img, double x0, double y0, const double m[6]);
  ```
  - `draw_cut` for an xf cut (not fixed): s = f.z / g.z, a = (g.rot - f.rot) degrees, `C = s cos a`, `S = s sin a` (`rotate_about_centre`'s sense: x' = x C + y S, y' = y C - x S); `m = { C, S, g.a - C f.a - S f.yg, -S, C, g.yg + S f.a - C f.yg }`; `enh_draw_image_xf(&c->img, c->x0, c->y0, m)`. Other cuts as now.
  - `predict_cuts` maps an xf cut's bottom and centre through the same m.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_image_xf` (res 2, aa 1):
    - a 5x3 image with distinct colours at (40, 30): the identity m gives exactly the samples of `enh_draw_image(img, 40, 30, 45, 33)`;
    - a 2x1 image {3, 4} at (100, 50) with the +90-degree map about (100, 50), `m = { 0, 1, 50, -1, 0, 150 }`, at res 1: sample (100, 49) is 3, (100, 48) is 4, nothing else is drawn.
  - `sr2_enh_aq_cut_turns`: two `aq_frame`s, roll 0 then roll 20, each writing the same tagged pixels into the shown page (a 3x3 row cut, id 5, and the opponent, id 15, sprite 5, opp_d 300, opp_seg ahead); the cuts are the newest frame's (roll 20). A smooth view between the two at a moment where the view's roll is 10:
    - the row cut's drawn samples lie inside the quadrilateral its frame rectangle's corners go to under `M` (the anchor turned by -10 degrees), and its centre sample is within 1 px of `M` of the frame centre;
    - with the opponent on the floor (opp_lat A00h), `g.rot - f.rot` is exactly -10 (a positive roll adds no roll / 4 term).
  - `sr2_enh_aq_sky_cuts`: smooth views (exact false, so no `mul8` floors) at heading h and h + 3, roll 0: the stars' anchor moves by exactly 3 in x (plot_points does not rotate at roll 0), the clouds' by 6 x 255/256 (shp_b_draw always rotates its anchor).

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh$' --output-on-failure`. Expected: build FAIL (`enh_draw_image_xf` undeclared).
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the Step 1 command, `-LE slow`, `-L slow`. Expected: `100% tests passed`, the compare's lines as after Task 4 (exact frames map every cut to itself). Look at two snapshots 150 ms apart while the car climbs a wall (`SR_SNAPSHOT_MS`, `--res-scale 4`): the row shapes, the stars and the opponent turn with the channel between the game's frames.
- [ ] **Step 4: Commit** `git commit -m "Rotate the Aqueduct's objects with the view"`. Codecheck: `0 differ`.

### Task 6: The wall bounce

**Files:**
- Modify: `src2/game/race_road.c` `road_advance_segment` (:1502-1505), the developer aid, each line `PORT: developer aid`: a helper `static s16 aq_wall_aid(void)` reading `SR_DEBUG_AQ_WALL` (`<stdlib.h>`, as race_phys.c's `police_aid_seg`), and in the mode-8 branch, between the `aq_lane_target` line and `DSS(DS_cam_x) = t;`, `if (aq_wall_aid() && DSS(DS_seg) >= aq_wall_aid()) t = sub16(DSS(DS_cam_x), 0x40);` (the drift replaces the lane step; Decisions)
- Modify: `tests/test_sr2_enh_compare.sh`
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Consumes: the roll snap (Task 3), the bounce channel (part B: `ENH_F_BOUNCE` from every snapshot).
- Produces: the aid; script helpers:
  - `bounced <kind> <mode> <min>`: frame lines of the mode with `bounce` not 0; `ok   <kind> mode <m> bounced (<n> frames)`;
  - `steep <kind> <mode> <min>`: frame lines with |roll| >= 55 (`shp_b_draw`'s steep path); `ok   <kind> mode <m> steep (<n> frames)`;
  - `kept <kind> <mode>`: the last frame line of the mode has `bounce` not 0 (D2: the offset stays after the bounce); `ok   <kind> mode <m> bounce kept`.

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_aq_bounce_persists`: `aq_setup()`, a frame of roll 48 and bounce 0, then the bounce's eight frames as `aq_wall_bounce` makes them (the shift added before each `road_frame`): bounce 0, 12, 24, 36, 48, 36, 24, 12 and roll 80, 112, ..., 304, then the frame after the bounce: roll 48, `DS_wall_hit_shift` 0, bounce 12. The newest view: bounce 12, roll snapped (`enh_view_at` gives the newest roll at every time), and `enh_rows_exact_aq(newest)` row 14's y equals `DSS(DS_row_y + 28)` after `aq_project_front()` with `DS_view_bounce_y` 12, and differs from it with 0.
  - The compare script:
    - `run aqwall 60 aqueduct SR_DEBUG_AQ_WALL=60`;
    - `check aqwall 8 20 orig lines`, `bounced aqwall 8 8`, `steep aqwall 8 1`, `kept aqwall 8`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: `FAIL aqwall mode 8 (no frames)` or `FAIL aqwall mode 8 bounced (0 frames)` (no aid yet).
- [ ] **Step 2: Implement** the aid and the script helpers.
- [ ] **Step 3: Run** the Step 1 command, then `-L slow`. Expected: all `ok`, `100% tests passed`.
  - Run the `aqwall` drive by hand once (the script's `run` line, `SR_ENH_COMPARE` and the output in a scratch folder outside the repository): its output shows `race_debug: race_results` with the crash bits 21h in the player's result.
  - The fast bounce gives 8 frames with `bounce` not 0 (the loop's frames 2..8 and the frame after it; more only while the race goes on after the crash), so `bounced aqwall 8 8` has no slack: every one of them must be compared. A slow bounce (speed at most 1Eh) is one frame; then `bounced` and `steep` fail. If so, raise the aid's segment (a faster car) until the log has the 8-frame bounce, the one that spins and snaps.
  - Look at snapshots of the bounce at `--res-scale 4`: the view spins with the original and snaps back at the same frame.
- [ ] **Step 4: Commit** `git commit -m "Check the Aqueduct's wall bounce"`. Codecheck: `0 differ`.

### Task 7: The Aqueduct's mirror

**Files:**
- Modify: `src2/enhanced/enh_aq2.c`, `enh_aq2.h` (`enh_draw_aq_mirror`, the mirror kinds of `enh_obj_geo_aq`, `enh_aq_starter_k`)
- Modify: `src2/enhanced/enhanced.h`: `#define ENH_OBJ_MIR_STARS 26` (the Aqueduct's mirror stars), `ENH_OBJ_IDS 27`.
- Modify: `src2/enhanced/enh_internal.h`: `ENH_CUT_MIR_STARS`; `EnhSnap` / `EnhView` gain `s16 starter_seg` (`DS_starter_seg` at presentation).
- Modify: `src2/enhanced/enhanced.c`: `enh_tag_kept` (26 only inside the glass), `cut_kind` / `cut_pass` (26: mirror pass, seg `INT_MIN`), `enh_mirror_begin` (mode 8's FG branch of Task 3 goes), `draw_view` (mode 8 also `enh_draw_aq_mirror`), `compare_frame` (the mirror's geometry in mode 8).
- Modify: `src2/game/race_aq.c`, each line `ENH:`:
  - `aq_mirror_stars` (:246-252): `enh_draw_objects(ENH_OBJ_MIR_STARS)` first, `enh_draw_objects(0)` last;
  - the row shapes (:448-451, Task 4's lines): `enh_draw_objects(N == 14 ? i : ENH_OBJ_MIR(i))` and `enh_draw_objects(0)` for both passes;
  - the starter (:502): `enh_draw_objects(ENH_OBJ_STARTER)` / `enh_draw_objects(0)` around `starter_draw_mirror_aq`;
  - the mirror's opponent (:503): `enh_draw_objects(ENH_OBJ_OPP)` / `enh_draw_objects(0)` around `opponent_draw`.
- Modify: `tests/test_sr2_enh_compare.sh`
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Produces:
  ```c
  void enh_draw_aq_mirror(const EnhView *v);   /* aq_draw_mirror: its glass F2h..13Dh x 16h..2Ah */
  int enh_aq_starter_k(int base, int starter_seg);   /* base - starter_seg - 1: the raw row starter_draw_mirror_aq
                                                       reads (row N - i of the row i it is drawn at) */
  ```
- The mirror pass (`aq_draw_mirror`, race_aq.c:633-647):
  - the glass clip `enh_clip(0xF2, 0x16, 0x13E, 0x2B)`; polygons and lines clipped on the game's mirror clip F0h..13Fh x 16h..2Ah (what lands on columns F0h, F1h, 13Eh, 13Fh is `mirror_finish`'s frame, foreground);
  - the backdrop `enh_image_vram(0x4000, 0xEA, 0x6A, 76, 21)` at (F2h, 16h) (part B's, `mirror_bg_prepare(0)`: no sky strip in mode 8), then `enh_cut_id(v, ENH_OBJ_MIR_STARS)`;
  - the scene with n = 7 and the mirror cam: roll0's floor is the rows Y[7]..2Ah across F0h..13Fh (`drv_hspan`), not `fill_rows`; no sky fill;
  - cuts before the loop `(INT_MIN, base - 8)`; row shapes at row i (step 2) `enh_cuts_at(v, MIRROR, base - i, base - i, false)`; step 4 at row i: the starter by `enh_cut_id(v, ENH_OBJ_STARTER)` when `7 - i == enh_aq_starter_k(base, v->starter_seg)`, then the opponent `(base - i, base - i, true)`; after the loop `(base, INT_MAX)`.
- Mirror kinds of `enh_obj_geo_aq` (b = 1 / z, rot = roll unless said, yg + `ROW_TOP`):
  - MIR at seg: k = base - seg, z = `enh_aq_mz(k, q)` (not ok for k < 0), anchor the mirror cam's rotated (x of world 0, y of the floor);
  - STARTER: k = `enh_aq_starter_k(base, seg)`, ok for 0 <= k <= 5; y = the mirror's raw Y[k], x = `line_x_at_y(tfl, tfy, tnl, tny, y) + 1` (the mirror's trough, unrotated), z = `enh_aq_mz(k, q)`; the anchor rotated when not roll0 (race_aq.c:380); rot = roll while |roll_raw| > 5 (`drv_rotate` turns the picture only then, :371-376), else 0;
  - OPP with sprite >= 9: `enh_aq_opp_anchor` (the mirror's projection, rotated about the pivot of `v->opp_mpass`'s pass: the mirror's for every cut that survives, since the front pass's pictures 9 and up are covered by the backdrop);
  - MIR_STARS: anchor (sky_x mirror stars, 0), rotated with the mirror cam when not roll0, z = 1.
- `compare_frame`, mode 8: `mrows = snap.game_mirror`, `enh_rows_exact_aq(&v, true, &mrows)`, `rows_err(.., 8)`, and `enh_aqtab_err` of `enh_aqtab_exact(&v, true, ..)` against `snap.aq_mirror` over rows 0..7 join `geo_err` (part B's mirror rows and YK check is not run in mode 8).

- [ ] **Step 1: Write the failing tests:**
  - `sr2_enh_aq_frame_state`: its mirror check becomes: a write after `enh_mirror_begin()` leaves fg clear (the glass enhanced), and after `enh_mirror_end()` sets it.
  - `sr2_enh_capture_regions` gains: `enh_tag_kept(ENH_OBJ_MIR_STARS, 0, 0x100, 0x20)` true, `(ENH_OBJ_MIR_STARS, 0, 0xF1, 0x20)` false, `(ENH_OBJ_MIR_STARS, 0, 0x80, 0x40)` false.
  - `sr2_enh_aq_starter`: `enh_aq_starter_k(100, 95) == 4`; for an exact mirror cam of rows case 1 at roll 0, `enh_obj_geo(&v, ENH_CUT_STARTER, 95, 0)` has yg = the mirror's raw Y[4] + `ROW_TOP` and a = `line_x_at_y` of the mirror's trough at that y, plus 1; `enh_obj_geo(&v, ENH_CUT_STARTER, 93, 0)` (k 6, where starter_draw_mirror_aq returns) is not ok.
  - `sr2_enh_aq_mirror_geo`: `enh_obj_geo(&v, ENH_CUT_MIR, base - 3, 0)` has z `enh_aq_mz(3, q)` and its anchor rotated about the mirror cam's pivot ((XL7 + XR7) >> 1, Y7).
  - The compare script: `check aqueduct 8 20 enh lines`, `check aqking 8 20 enh lines`, `check aqwall 8 20 enh lines`.

  Run: `cmake --build build-mac && ctest --test-dir build-mac -R 'sr2_enh' --output-on-failure`. Expected: build FAIL (`ENH_OBJ_MIR_STARS` undeclared).
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the Step 1 command, then `-L slow`. Expected: all `ok`, the mode-8 lines with `mirror_cov_min 1596`, `mirror_line_worst` 0 or 1, `mirror_mismatch_p50 <= 10`, and `100% tests passed`. A bad mirror frame is debugged from its dump as in Task 4. Look at snapshots at `--res-scale 4`: the mirror's channel rolls with the front, its stars and the starter at the start, the opponent behind; the dashboard's side mirror still the original's pixels.
- [ ] **Step 4: Commit** `git commit -m "Draw the Aqueduct's mirror enhanced"`. Codecheck: `0 differ`.

### Task 8: The launcher, the docs and the final check

**Files:**
- Modify: `launcher/gameargs.cpp` :11: SR2's `pictureNote` becomes `"Street Rod 2: the art is original for now."`.
- Test: `tests/test_args.cpp` :86: `CHECK(wxString(b.pictureNote) == "Street Rod 2: the art is original for now.");`
- Modify: `ENHANCED.md` section 9:
  - the "Done" sentence: every race kind (town, street drag, Grudge drag strip, Mulholland, the Aqueduct) and the rear-view mirror in each; still original: the dashboard's side mirror, and the art until Street Rod 2's remastered pictures and shapes;
  - an Aqueduct bullet: its rows (12..14 fixed), the clamp, the rotation (the game's sine table, the 255/256 shrink, the pivot; the opponent about the pivot of the pass that draws it), the two constructions, the sky with stars and clouds captured, the bounce's snap and its kept offset, the mirror's quirks (XC, the wall shifts, the flags, the starter's row);
  - the objects bullet: ids 24, 25, 26 and `ENH_OBJ_IDS` 27; rotated cuts (the mapping, the opponent's bank, the variant pops accepted);
  - the Check paragraph: the line metric in mode 8 (the black and white lines, and the sky's border in the front view), the rotated table in the geometry check, the opponent's byte snap and the rule for judging rotated cuts, the roll and bounce fields, the `aqueduct`, `aqking` and `aqwall` runs (`SR_DEBUG_AQ_WALL`); "the Aqueduct's mode 8 must log no frames" goes.
- Modify: `ENGINE2.md` :33-37 (the `race_aq.c` hooks: `enh_aq_begin`, the stars, clouds, row shapes, opponent, starter and mirror stars tagged) and :44-47 (`SR_DEBUG_AQ_WALL=<seg>` in `race_road.c`: from that segment the demo autopilot drifts into the left wall and bounces; unset, it changes nothing).
- Modify: `CHANGELOG.md` Unreleased (:15-19): the SR2 enhanced entry covers the Aqueduct (the rolled channel, its sky, the bounce) and the mirror in every race kind; "The Aqueduct is the original picture for now." goes.
- Modify: `README.md` :31-33 and :216 (no Aqueduct exception), `launcher/README.md` :44-46 (the race view covers every race kind and the mirror; only the art is greyed, with the note).
- Modify the header comments:
  - `src2/enhanced/enhanced.h` (the id table's "part C" notes);
  - `src2/enhanced/enh_internal.h` :11 ("part C fills ...");
  - `src2/enhanced/enhanced.c` :125-126 (`mode_enhanced`'s list) and :233-235;
  - `src2/enhanced/enh_road2.c` :1-9 (mode 8 is `enh_aq2.c`'s);
  - `tests/test_sr2_enh_compare.sh` :2-8 (the line metric; the `none` helper and "a mode not enhanced yet" go when no check uses them).

- [ ] **Step 1: Update `tests/test_args.cpp`.** Run: `cmake --build build-mac && build-mac/tests/test_args`. Expected: `FAIL traits_table`.
- [ ] **Step 2: Implement** the trait and the docs. Every new sentence uses ASCII hyphen-minus only.
- [ ] **Step 3: Full check.**
  - `cmake --build build-mac && ctest --test-dir build-mac --output-on-failure`. Expected: `100% tests passed`, `sr2_enh_compare` and `sr2_smoke` included.
  - `build-mac/tests/test_args`. Expected: `ok   traits_table`.
  - `tools/sr1-codecheck.sh 2739cab HEAD --allow host.c.o`. Expected: `... 0 differ (allowed: host.c.o )`.
  - `git diff 2739cab HEAD | grep -nP '^\+.*[\x{2010}-\x{2015}]'`. Expected: no output.
  - Screenshot the launcher with SR2 chosen: the note reads "Street Rod 2: the art is original for now.". Back up `settings.ini` first and restore it byte-identical after. If the screen cannot be captured, say so.
- [ ] **Step 4: Commit** `git commit -m "Offer the whole Street Rod 2 race view enhanced"`.
