# Street Rod 2 enhanced race view, part A Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Street Rod 2's town drive and street drag (road modes 0 and 1) get Street Rod 1's enhanced race view: high resolution, smooth motion, anti-aliasing, with the road-side objects, signs, billboards, opponent, police and mirror. A compare tool checks the enhanced view against the original frame. The other race kinds stay original until parts B and C.

**Architecture:**
- The engine-independent drawing code (`enh_raster.c`, `enh_sprite.c` and their declarations) moves unchanged to `common/enhanced/`. Each game compiles it with its own `mem.h`, `platform/` and `remaster/` headers, as `common/band/` already does.
- Street Rod 2 gets its own `src2/enhanced/`:
  - the hooks and the snapshots (SR2's units: 1/64 segment, its symbols);
  - `enh_road2.c`, the road, objects, opponent and mirror from SR2's race data and formulas;
  - `enh_compare.c`, the check.
- `src2/platform/ega.c` and `vga.c` get the `ENH:` layer Street Rod 1's have: the written mask, the foreground layer, the output scale and the compose hook. Coverage then works as in Street Rod 1: whatever the enhanced view does not draw stays the original's pixels.

**Tech Stack:** C11, SDL3; tests in C and POSIX sh with Python 3.

**Spec:** `docs/superpowers/specs/2026-11-11-street-rod-2-design.md`, section "Enhanced race view"; `ENHANCED.md` (Street Rod 1's stage 1, the approach to follow). Plan 4 is split in three: A (this: groundwork, compare tool, modes 0 and 1), B (Grudge drag strip modes 2/4, Mulholland 0x10), C (Aqueduct 8).

## Global Constraints

- Street Rod 1 does not change. `tools/sr1-codecheck.sh bba3310 HEAD` must report no differing object: the move to `common/enhanced` and the header split change no code.
- Same coverage rules as Street Rod 1: the messages, the start light, the dashboard and the cockpit stay original.
- Same options as Street Rod 1: `--classic`, `--res-scale N` (1..8, default 4), `--aa N` (1..4, default 2), `--motion-delay P` (0..100, default 100), `--town on|off` (default on). The defaults are those of SR1's `src/main.c`.
- Acceptance as for Street Rod 1, measured by the compare tool: with time stopped, the enhanced view at scale 1 matches the original frame. Road edges are within +-1 px, and objects sit at the same row and size.
- Original draw distance; no gameplay change. `--classic` shows the original picture, scaled.
- In road modes 2, 4, 8 and 0x10 the enhanced view draws nothing in this plan: the original frame shows, scaled.
- No macOS username, no real name or email, no AI attribution, no Unicode dashes in anything committed. Commit messages: one short imperative line.

## Review Focus

1. The interpolated view between two game frames must not put the original's foreground (cockpit, messages, start light) out of place. Only background pixels are replaced. (Task 2 `sr2_enh_layers`; Task 4 compare runs.)
2. A race kind not yet enhanced (Grudge, Mulholland, Aqueduct) must show exactly the original picture, scaled, with nothing of the enhanced view left over from an earlier drive. (Task 4 `sr2_enh_compare.sh` runs `grudge` and `mulholland` and expects 0 enhanced frames.)
3. `SR_DEBUG_RACE` drives and the town cruise must run the same with the enhanced view on as off: the smoke paths pass with the default options. (Task 6 `sr2_smoke`.)
4. Police in the town drive use the opponent slot with police pictures: the enhanced view must draw the police car, not the last opponent. (Task 5 compare on `cruise` with the police.)
5. With AdLib music the game tick follows the song's rate, not the PIT divisor the SR1 code assumes. The smooth motion must keep pace with SR2's actual frame times. (Task 4: frame times come from the host clock at `enh_frame_built`, never from a fixed tick length.)

---

### Task 1: The shared drawing core

**Files:**
- Move (git mv): `src/enhanced/enh_raster.c`, `src/enhanced/enh_sprite.c` to `common/enhanced/`
- Create: `common/enhanced/enh_draw.h`, holding the raster and sprite declarations taken out of `src/enhanced/enh_internal.h` (`enh_buf_setup`, `enh_clip*`, `enh_fill_*`, `enh_resolve`, `EnhImage`, `enh_images_reset`, `enh_image_*`, `enh_draw_image`, `enh_cur_rgb`, and the constants they use)
- Modify: `src/enhanced/enh_internal.h` (includes `enh_draw.h`), the two moved files (include `enh_draw.h`), `CMakeLists.txt` (the moved sources in `sre_core`, `common/enhanced` on its include path), `common/README.md`

**Interfaces:**
- Produces: `enh_draw.h`, reachable as `"enh_draw.h"` from either game. The moved `.c` files keep including `"mem.h"`, `"platform/ega.h"`, `"platform/video.h"`, `"remaster/rm_pack.h"` and `"remaster/rm_tags.h"`, each resolved to the including game's own.

- [ ] **Step 1: Move and split.** No code changes beyond includes and the split declarations.
- [ ] **Step 2: Build and test.** Run `cmake --build build-mac && ctest --test-dir build-mac -LE slow`. Expected: all pass.
- [ ] **Step 3: Commit** `git commit -m "Move the enhanced view's drawing core into common"`. Then run `tools/sr1-codecheck.sh bba3310 HEAD`. Expected: 60 objects, 0 differ.

### Task 2: Street Rod 2's display gets the enhanced layer

**Files:**
- Modify: `src2/platform/ega.c`, `src2/platform/ega.h`, `src2/platform/vga.c`. Port Street Rod 1's `ENH:` parts: the written mask, `ega_set_layer` and the foreground mask, the output scale and the `enh_compose` call. Leave out the remaster calls; plan 5 brings them.
- Create:
  - `src2/remaster/rm_pack.h` and `rm_tags.h`: inline stubs (`rm_pack_active()` false, `rm_tags_active` false, the rest unreachable) so the shared core builds.
  - `src2/enhanced/enhanced.h`: Street Rod 1's API, the same names.
  - `src2/enhanced/enhanced.c`: `enh_init` and its options, `enh_enabled`, `enh_res_scale`, and `enh_compose` returning false (no road yet); the hooks as empty functions.
- Modify: `src2/main.c` (the five options, parsed and defaulted as SR1's `src/main.c`, then `enh_init`), `CMakeLists.txt` (common/enhanced in `sr2_core`)
- Test: `tests/test_sr2_enh.c` (new ctest `sr2_enh`, under `SRE_TEST_GAME2_DIR`)

**Interfaces:**
- Produces: as in SR1's `src/platform/ega.h`: `ega_written(off)`, `ega_fg(off)`, `ega_clear_written(page)`, `ega_set_layer(int)`, `ega_set_scale(int)`; the frame size `320*scale x 200*scale`.

- [ ] **Step 1: Write the failing tests** in `test_sr2_enh.c`:
  - `sr2_enh_scale`: a VRAM pattern composed at scale 1 and at scale 4. Each 4x4 block of the second equals the pixel of the first.
  - `sr2_enh_layers`: a write in the background layer marks the pixel written and not foreground. A write in the foreground layer marks both. `ega_clear_written` clears the page.
  - `sr2_enh_options`: `--res-scale 9` and `--aa 0` are refused as SR1 refuses them (the usage, exit 2). Run through the program with `--check`.

  Run `cmake --build build-mac && ctest --test-dir build-mac -R sr2_enh --output-on-failure`. Expected: build FAIL (no `ega_set_layer` in src2).
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run** the same command. Expected: PASS. Then `ctest --test-dir build-mac -R "sr2_" -LE slow`. Expected: all pass.
- [ ] **Step 4: Commit** `git commit -m "Give Street Rod 2's display the enhanced layer"`.

### Task 3: The compare tool

**Files:**
- Create: `src2/enhanced/enh_compare.c`, `src2/enhanced/enh_compare.h`
- Test: `tests/test_sr2_enh.c`

**Interfaces:**
- Produces:
  - `typedef struct { int rows, edge_rows, edge_err_max, pixels, mismatched; } EnhCompare;`
  - `void enh_compare_rows(const u8 *orig, const u8 *enh, const u8 *covered, int y0, int y1, const u8 *road_colours, int n_road, EnhCompare *out);` (pure).
    - `orig` and `enh` are 320-wide rows of palette indices.
    - `covered` is nonzero where the enhanced view replaces the pixel (not written, not foreground).
    - Per row, the road's left and right edges are the first and last column whose colour is one of `road_colours`. `edge_err_max` is the largest edge difference in px over the rows where both have a road.
    - `mismatched` counts covered pixels that differ.
  - `SR_ENH_COMPARE=<file>`: on each presented frame the enhanced view draws, it is rendered again at scale 1 and aa 1 from that frame's snapshot with no interpolation, and mapped back to palette indices. It is compared with the shown page's view rows (0x12..0x65), and one line is appended per frame: `frame <n> mode <m> rows <r> edge_rows <e> edge_err_max <x> mismatch <permille>`.
  - The summary at exit: `frames <n> worst_edge <x> mismatch_p50 <a> mismatch_max <b>`.

- [ ] **Step 1: Write the failing tests** `sr2_enh_compare_rows`, on synthetic rows:
  - equal rows: 0 mismatches, edge error 0;
  - the enhanced road shifted 1 px: edge error 1;
  - shifted 3 px: edge error 3;
  - uncovered pixels never counted;
  - rows without road ignored for the edges.

  Run `ctest --test-dir build-mac -R sr2_enh --output-on-failure`. Expected: build FAIL.
- [ ] **Step 2: Implement** `enh_compare_rows` and the `SR_ENH_COMPARE` log, written from `enh_frame_presented`. Until Task 4 no frame is drawn, so the log stays empty.
- [ ] **Step 3: Run.** Expected: PASS.
- [ ] **Step 4: Commit** `git commit -m "Add a compare tool for Street Rod 2's enhanced view"`.

### Task 4: The road of the town drive and the street drag

**Files:**
- Create: `src2/enhanced/enh_internal.h`, `src2/enhanced/enh_road2.c`
- Modify:
  - `src2/enhanced/enhanced.c`: the snapshots and the interpolation, in SR2's units; the frame time is taken from the host clock at `enh_frame_built`.
  - The hooks, each `ENH:`:
    - `enh_race_begin` / `enh_race_end` in `race_drive.c`, before `road_pages_init` and after the loop;
    - `enh_frame_built` at the top of `road_frame`;
    - `enh_draw_begin` and `enh_draw_fg` in `race_road.c` around the background and the foreground;
    - `enh_frame_presented` in `road_page_flip`;
    - `enh_phys_step` in `race_phys.c`.
- Test: `tests/test_sr2_enh_compare.sh` (new ctest `sr2_enh_compare`, label `slow`, timeout 600)

**Interfaces:**
- Produces:
  - `enh_draw_road(const EnhView *v)` for modes 0 and 1: the sky, the ground and the road bands, from SR2's projection.
    - `road_proj_y` with the halved, LSB-masked height term and `DS_view_bounce_y`.
    - The depth schedule with the fixed far row.
    - `edges_of` with its zero-width cases.
    - The sky's dy from `HGT[seg-1]` with its clamp.
    - The cam_x clamp of mode 8 is not needed here.
  - Every enhanced hook returns at once for other modes. `enh_compose` returns false then, so the original shows scaled.

- [ ] **Step 1: Write the failing test** `test_sr2_enh_compare.sh <exe> <game dir>`. It runs headless (`demo v`, `SR_KEYS=1:03`, the dummy drivers, `--run-ms` as `sr2_smoke.py` uses):
  - `SR_DEBUG_RACE=drag` and `cruise`, with `SR_ENH_COMPARE`. Expected: at least 20 compared frames each, `worst_edge <= 1`, `mismatch_p50 <= 10` permille.
  - `SR_DEBUG_RACE=grudge` and `mulholland`. Expected: 0 compared frames (not enhanced yet).

  Run `ctest --test-dir build-mac -R sr2_enh_compare --output-on-failure`. Expected: FAIL with 0 compared frames for drag.
- [ ] **Step 2: Implement** the snapshots, the hooks and the road background.
- [ ] **Step 3: Run.** Expected: PASS. Then look at a snapshot of each (`SR_SNAPSHOT_DIR` in the scratchpad, `--res-scale 4`). The road is smooth and sharp, and the objects drawn by the game sit on it.
- [ ] **Step 4: Commit** `git commit -m "Draw Street Rod 2's town and drag road enhanced"`.

### Task 5: Road-side objects, signs, billboards, opponent and police

**Files:**
- Modify: `src2/enhanced/enh_road2.c`, `src2/enhanced/enhanced.c` (the opponent and object state in the snapshots)
- Test: `tests/test_sr2_enh_compare.sh`, `tests/test_sr2_enh.c`

**Interfaces:**
- Produces:
  - `int enh_shape_a_decode(u16 set, int lod, EnhSpan *out, int max)` (pure over game memory). SR2_SHP format A from `DS_shp_a_ptrs`: per LOD six {colour, count} groups and their 3-byte spans; side < 1 is mirrored, as `race_gfx.c` draws it.
  - The objects, signs (page 0 row 100), billboards (`DS_billboard_pics`), the opponent (1/64 units, the shrunk sprites 3-5, 6-8, 10-12) and the police (`DS_police_sprites` in the opponent slot) drawn by the enhanced view. The engine's own drawing of them goes to the background layer, so it is replaced.
- [ ] **Step 1: Write the failing tests**
  - `sr2_enh_shape_a`: decode set 0's LOD 0 from the loaded SR2_SHP. The span count and the first span's colour and width equal what `race_gfx.c`'s drawer reads (record it with a small probe in the test).
  - The compare script, for `drag` and `cruise`: `mismatch_p50 <= 10` still holds with the objects counted as covered.

  Run the band of tests. Expected: FAIL (no decoder; the objects' pixels mismatch once covered).
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Run.** Expected: PASS. Look at snapshots: the signs, billboards, opponent and police are sharp and move smoothly with the road.
- [ ] **Step 4: Commit** `git commit -m "Draw Street Rod 2's road-side objects and cars enhanced"`.

### Task 6: The mirror, the launcher and the docs

**Files:**
- Modify:
  - `src2/enhanced/enh_road2.c`: the mirror, with its backdrop from page 2's rectangles and object depth `(row>>1)+7`. The opponent in it uses `10-dx`, the banners and the starter figure as the game draws them.
  - `launcher/gameargs.cpp`: SR2's `enhanced` trait is true.
  - `launcher/launcher.cpp`: the picture note for SR2 reads "Street Rod 2: the Grudge drag strip, Mulholland and the Aqueduct are original for now."
  - `tests/test_args.cpp`, `ENHANCED.md` (a Street Rod 2 section), `ENGINE2.md`, `README.md`, `launcher/README.md`, `CHANGELOG.md`
- Test: the compare script (the mirror rows), `test_args`

- [ ] **Step 1: Update the tests.**
  - `traits_table`: SR2's `enhanced` is true.
  - `args_sr2_minimal`: now also has `--res-scale 4 --aa 2 --town on`, in SR1's order.
  - The compare script also covers the mirror rows.

  Run them. Expected: FAIL.
- [ ] **Step 2: Implement.**
- [ ] **Step 3: Full check.**
  - Run `tools/macos/package.sh --version 0.4.2`. Expected: every test passes, `sr2_smoke` included, `package ok`.
  - Run `tools/sr1-codecheck.sh bba3310 HEAD`. Expected: 0 differ.
  - Screenshot the launcher with SR2 shown, backing up `settings.ini` first and restoring it after.
- [ ] **Step 4: Commit** `git commit -m "Offer Street Rod 2's enhanced race view"`.
