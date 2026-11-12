# Enhanced view worker threads Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The enhanced race view of both games records its drawing calls as a per-frame command list and runs it, with the resolve, in horizontal bands on worker threads, drawing exactly the frame one thread draws, and the repo can prove that (byte for byte, both games, with and without art) and measure it.

**Architecture:**
- The idea of upstream PR #5 by bsvdoom (kylofon/streetrod-enhanced, "Enhanced view: rasterize and resolve on worker threads"), ported onto our shared raster: `common/enhanced/enh_bands.c` owns the command list, the worker threads (SDL threads woken by semaphores, the calling thread takes band 0) and the band split; every function that writes `enh_buf.px` / `enh_buf.rgb` is a command body run per band under that band's rows.
- **State at the start of this plan:** commit `1fbc94f` ("Draw the enhanced view in bands on worker threads") already landed a first cut of all of that, without a plan: `enh_bands.c/.h`, `enh_submit` / `enh_flush` / `enh_bands_run`, the command bodies in both games, `host_set_shutdown_handler`, `SR_ENH_THREADS`, `SR_ENH_THREADS_CHECK`, `tests/test_enh_threads.sh` (ctest `enh_threads`, label slow), SR2's `SR_ENH_TRACE`, and docs. This plan does not redo it. It audits it against the map below and closes the gaps the map shows: no unit test of the band machinery, a check that can pass vacuously or hide first-use bugs, missing race kinds and band counts, unguarded per-frame inputs, no timing split, and timings taken on a loaded machine.
- The pre-threads commit is `153e779` (the code-check base and the baseline of the timings).
- Verified while planning: `enh_threads` passes at `1fbc94f` (133 s); `tools/sr1-codecheck.sh 153e779 1fbc94f --allow host.c.o` differs only in `enh_bands.c.o` (new), `enh_raster.c.o`, `enh_road.c.o`, `enh_sprite.c.o`, `enhanced.c.o` (enhanced view) and `host.c.o` (allowed): no game object changed.

**Tech Stack:** C11, SDL3 (threads, semaphores, atomics), POSIX sh tests, CMake/ctest, Ninja.

**Spec:** none; owner request in conversation, summarised in Global Constraints.

## Global Constraints

- Frames are EXACTLY the frames single-threaded drawing makes: sample buffer (`px`, and `rgb` where `px == ENH_RGB`) and resolved `xrgb`, byte for byte, in both games, with and without art (`--remaster-dir`), in every race kind: SR1's demo race; SR2's drag, cruise (town drive), grudge (drag strip modes 2 and 4), Mulholland (with its mirror passes), the Aqueduct (turned cuts, the King's rotated sprites).
- Bands: one per logical core by default (`SDL_GetNumLogicalCPUCores`, at most 16), the calling thread takes the first band; bands never share a sample row (split on output rows, `aa` sample rows each); no locks.
- Workers stop before `SDL_Quit` (`host_set_shutdown_handler`) and at exit (`atexit`); a stop runs any recorded commands first.
- A full command list warns once on stderr and is run mid-frame; a worker that cannot start leaves fewer bands; none leaves direct drawing.
- No cache or table may free or rewrite what a recorded command still reads: `enh_flush()` first.
- Developer aid `SR_ENH_THREADS=N` (1: direct, single-threaded drawing) stays; `SR_ENH_THREADS_CHECK=1` stays the identity check.
- SR1's game code does not change: `SRE_PREFIX=$PWD/build-deps/prefix tools/sr1-codecheck.sh 153e779 HEAD --allow host.c.o enhanced.c.o enh_road.c.o enh_raster.c.o enh_sprite.c.o enh_bands.c.o` reports `0 differ`. Any other differing object is a ruling for the owner, written into this plan's ledger (end of file), not silently allowed.
- Commands are run from the repo root. Build: `cmake --build build-mac`. Quick tests: `ctest --test-dir build-mac -LE slow`. Slow tests (`sr2_enh_compare`, `sr2_smoke`, `sr2_identity`, `sr2_marker`, `enh_threads`) at the end of every task that touches drawing.
- Commits: identity already configured; one short imperative subject under about 60 characters, no body, no trailers, no AI attribution. ASCII hyphen-minus only in anything written. No macOS username, real name, or scratchpad path in any repo file. Work on `master`, never push, never touch `.worktrees/`.
- Docs: ENHANCED.md (both games' sections; no "worker threads" left in a not-done list), CHANGELOG `Unreleased` credits bsvdoom's change to kylofon/streetrod-enhanced, README developer aids; ENGINE2.md only if a game hook changes (none planned).

## The map (done before this plan; the tasks rely on it)

**Writes to `enh_buf.px` / `.rgb`** - all inside command bodies, run only by `run_band` (bands) or `enh_submit`'s direct path:

| Body | File | Writes | Public call |
|---|---|---|---|
| `fill_rows_run` | `common/enhanced/enh_raster.c` | px | `enh_fill_rows` |
| `fill_poly_run` | `common/enhanced/enh_raster.c` | px | `enh_fill_poly`, `enh_fill_rect` |
| `draw_image_run` | `common/enhanced/enh_sprite.c` | px, rgb (art) | `enh_draw_image` |
| `sheet_row_run` | `src/enhanced/enh_road.c` | px, rgb (art); whole row, ignores the clip, kept to `by0..by1` | `sheet_row` (static) |
| `sheet_row_run` | `src2/enhanced/enh_road2.c` | same | `enh_sheet_row` |
| `poly_incl_run` | `src2/enhanced/enh_poly2.c` | px | `enh_fill_poly_incl`, `enh_fill_poly_edge` |
| `line1_run` | `src2/enhanced/enh_poly2.c` | px | `enh_line1` |
| `draw_xf_run` | `src2/enhanced/enh_xf2.c` | px, rgb (art) | `enh_draw_image_xf` |

`enh_hills2.c`, `enh_mirror2.c`, `enh_aq2.c`, SR2 `enhanced.c` (cuts) and SR1 `enh_mirror.c` write nothing directly. `enh_buf_setup` reallocates (flushes first); the buffer is never cleared per frame, so stale samples are the same both ways.

**Reads of the samples while drawing:** none by any drawing call (every body only overwrites; no blend, no read-modify-write), so command order within a band is enough. Readers: `enh_resolve_rows` (own band's rows, after its commands); SR2 `compare_frame` (`SR_ENH_COMPARE`, flushes first, `src2/enhanced/enhanced.c:756`); `enh_bands_check`; the unit tests `compose`, `side_mirror`, `tags`, `sr2_enh`, `sr2_tags` (run with `SR_ENH_THREADS=1`). SR1's side mirror reads `xrgb` after the resolve.

**Clip and palette:** `enh_buf.cx0..cy1` (set by `enh_clip` / `enh_clip_view` on the main thread) and `enh_cur_rgb` are copied into each command at `enh_submit`; the band clamps `cy` to its rows. SR2's geometry clips (`enh_clip_poly_i/f`, `enh_clip_line_i`, the Aqueduct's clip rectangle) run at record time. The palette array passed to `enh_compose` lives until the resolve returns.

**What bodies read besides their arguments, and its lifetime:**
- `enh_sprite.c` image cache (48 entries): freed by `enh_images_reset` (cache full mid-frame, race start in both games) - flushes first. Commands copy the `EnhImage` header by value (SR1's stack `strip` image at `enh_road.c:656` relies on it), pointers `px` / `rm` must stay valid.
- SR1 `shp[]` shape art: allocated once, never freed.
- SR2 `cuts[].img`: `SDL_realloc` / `SDL_free` in `capture_objects` (`enh_frame_presented`), between composes. Safe today only because `enh_compose` always resolves; **unguarded**.
- Sheet tables `src_rows` / `src_tags`: SR1 `read_sheets` (inside `draw_sky`, mid-drawing), SR2 `enh_sheets_read` (`enh_frame_presented`). **Unguarded.**
- Remaster pictures: `rm_pic` loads lazily (PNG decode) on its first call. Every tag that reaches a body was first looked up by `rm_pic` on the main thread (`attach_art`, both `read_sheets`, `capture_objects`, `shape_art`), so bodies only read loaded pictures; slots are never freed. `rm_texel`'s palette log and the self-test counters write shared state, hence `serial()` (direct drawing) when either is on. **This invariant is undocumented and the check hides a break of it** (it always draws directly first, which loads).
- The resolve reads `ega_written`, `ega_fg`, the planes: the game is stopped while the main thread waits.

## Baseline timings (measured for this plan)

Median (p90) ms per composed frame, drawing + resolve, from `SR_ENH_TRACE` (SR2's trace patched into a scratch build of `153e779`), 1280 x 800 (`--res-scale 4 --aa 2`), `SDL_VIDEO_DRIVER=dummy`, M1 Pro (8P + 2E = 10 logical cores), identity art packs (`rm.py identity-pack`). SR2 runs: `SR_SEED=2` (Mulholland 4) `SR_DEBUG_RACE=<race> SR_KEYS=1:03 --run-ms 30000 demo v`; SR1: `SR_SEED=3 demo`, 70 s.

**Warning: the machine was heavily loaded by other work during every run (load average 64 to 115 on 10 cores).** The single-thread numbers are inflated and noisy, the threaded ones much more so (a band preempted stalls the frame). Task 5 measures again on a quiet machine; these numbers are only the order of magnitude.

| Run | before (`153e779`) | `1fbc94f`, `SR_ENH_THREADS=1` | `1fbc94f`, 10 bands |
|---|---|---|---|
| SR2 drag | 4.21 (4.95) | 4.43 (12.14) | 4.13 (15.48) |
| SR2 drag, art | 17.63 (21.82) | 18.91 (20.81) | 18.51 (46.07) |
| SR2 cruise | 7.52 (13.55) | 4.94 (32.18) | 3.26 (6.69) |
| SR2 cruise, art | 18.50 (25.24) | 17.75 (20.25) | 7.51 (13.79) |
| SR2 Aqueduct | 4.70 (5.22) | 4.58 (7.33) | 3.29 (5.69) |
| SR2 Aqueduct, art | 5.50 (5.79) | 6.43 (31.88) | 3.09 (9.72) |
| SR2 Mulholland | 5.20 (5.51) | 5.27 (7.68) | 2.45 (5.79) |
| SR2 Mulholland, art | 58.46 (88.35) | 32.45 (59.86) | 9.02 (13.37) |
| SR1 demo | 4.17 (5.26) | 4.48 (9.23) | 4.55 (11.34) |
| SR1 demo, art | 25.20 (66.01) | 34.89 (67.85) | 8.26 (12.29) |

`1fbc94f`'s own figures in ENHANCED.md (presumably a quieter machine): SR1 4.2 / 0.9 ms, art 19.3 / 4.4; SR2 Mulholland art 31.9 / 5.6. The drag with art scaling least (19.0 / 12.7 there, none here) is the open question Task 4's split answers.

## Review Focus

1. A picture's first appearance in banded drawing (art loaded lazily, any static initialised on first use): the check draws directly first, so a first use always happens on the main thread and a race would never be seen. Expected: identical frames whatever order. (Task 2 alternates the order; Task 3 documents the `rm_pic` invariant and tests the producers.)
2. A check run that compares nothing: bands fell to 1 (self-test or palette log on, threads failed) or no art sample was drawn, and the test still says ok. Expected: the test fails. (Task 2 `check` asserts `bands > 1`, `art > 0` with a pack, `art 0` without.)
3. A command list that fills mid-pass (mid-mirror, mid-Aqueduct turn) or a cache reset mid-frame: the flush must keep order and clip. Expected: identical frames. (Task 1 `bands_full_list`, `bands_arena_full`, `bands_cache_overflow`; Task 2 the `SR_ENH_CMDS=50` runs.)
4. Band counts and scales whose rows do not split evenly (3, 7, 16 bands; res scale 1 and 3, aa 1 and 3). Expected: identical frames. (Task 1 `bands_identity_counts`; Task 2 the `b3` and `b7` runs.)
5. A table rewritten between recording and running (SR2's cuts and sheets at `enh_frame_presented`, SR1's sheets mid-draw), or the workers stopped with commands pending. Expected: the commands see what they saw when recorded, and none is lost. (Task 1 `bands_shutdown_runs_pending`; Task 3 `sr2_enh_sheets_read_flushes`.)

---

### Task 1: The band machinery under test

**Files:**
- Modify: `common/enhanced/enh_bands.c`, `common/enhanced/enh_bands.h`
- Create: `tests/test_enh_bands.c`
- Modify: `tests/CMakeLists.txt` (add `test_enh_bands`, linked to `sre_core`, one ctest per case: `enh_bands_<case>`)

**Interfaces:**
- Consumes: `enh_submit`, `enh_flush`, `enh_bands`, `enh_bands_shutdown`, `enh_resolve`, `enh_fill_rows`, `enh_fill_poly`, `enh_fill_rect`, `enh_draw_image`, `enh_image_vram`, `enh_images_reset`, `enh_clip`, `enh_clip_view`, `enh_cur_rgb` (all from `1fbc94f`); `rm_pack_open`, `rm_slot_register_raw`, `rm_pic`, `rm_tag` for a tiny pack as in `tests/test_compose.c`.
- Produces (in `enh_bands.h`):
  ```c
  typedef struct {
      int bands;          /* bands the last frame was drawn in (1: directly) */
      int cmds;           /* commands recorded in the last frame */
      int flushes;        /* runs before the last frame's resolve (full list, cache reset, buffer change) */
      long full;          /* times the list was full since the start */
      double band_ms_max, band_ms_mean;   /* last frame: slowest band and mean band (run + resolve), ms */
  } EnhBandStats;
  void enh_bands_stats(EnhBandStats *out);
  /* Test and developer hook: stops the workers (running anything recorded) and makes the next submit start
   * `bands` bands (0: SR_ENH_THREADS or one per core) with a list of cmd_max commands and arena bytes (0: the
   * defaults 32768 and 4 MiB); fail_after >= 0 makes thread creation fail after that many workers (-1: never). */
  void enh_bands_config(int bands, int cmd_max, size_t arena, int fail_after);
  ```
  `enh_bands_shutdown` runs pending commands (`enh_flush`) before stopping. Stats are the last frame's, frozen at the end of `enh_bands_run`.

- [ ] **Step 1: Write the failing tests** in `tests/test_enh_bands.c` (`main(argc, argv)` runs the case named by `argv[1]`, prints `ok <case>` or `FAIL ...`, exit 1 on failure). A helper `draw_scene(int variant)` draws, in order: sky rows (`enh_fill_rows`), 200 seeded-random `enh_fill_poly` (3 to 8 vertices, partly off-screen), `enh_fill_rect`, `enh_draw_image` of plain and art-tagged images (normal and mirrored, `x1 < x0`), the clip switched between `enh_clip(0xF0, 0x12, 0x140, 0x2B)` and `enh_clip_view()` several times, and `enh_cur_rgb` switched between two palettes between commands. A helper `frame(bands, cmd_max, arena, fail_after, S, aa, out)` calls `enh_bands_config`, sets `enh_opt.res_scale/aa`, `enh_buf_setup()`, sets `px` to 0xFD and `rgb` to 0, draws (after `ega_init()` once, and page-0 VRAM filled with a seeded pattern for `enh_image_vram`), `enh_resolve(xrgb, pal, 16, 0, ENH_VIEW_Y1)`, and copies `px`, `rgb` (where `px == ENH_RGB`) and `xrgb`. `same(a, b)` compares all three byte for byte.
  - `bands_identity_counts`: for bands 2, 3, 7, 16 and (S, aa) in (1, 1), (3, 3), (4, 2): `CHECK(same(frame(1, ...), frame(n, ...)))`, `CHECK(stats.bands == n)`.
  - `bands_full_list`: cmd_max 5: identical to bands 1; `CHECK(stats.full > 0 && stats.flushes > 0)`.
  - `bands_arena_full`: arena 2048: identical; `CHECK(stats.flushes > 0)`.
  - `bands_thread_fail`: bands 8, fail_after 2: identical; `CHECK(stats.bands == 3)`. fail_after 0: identical; `CHECK(stats.bands == 1)`.
  - `bands_cache_overflow`: one frame drawing 50 distinct `enh_image_vram` rectangles (the cache holds 48): identical; `CHECK(stats.flushes >= 1)`.
  - `bands_shutdown_runs_pending`: bands 4, submit `enh_fill_rows(0x20, 0x30, 9)`, `enh_bands_shutdown()`: `CHECK(px[row of 0x28] == 9)`; a second `enh_bands_shutdown()` is harmless; the next frame `CHECK(stats.bands == 1)` and is identical to bands 1.
  - `bands_art_drawn`: with the pack, the bands-4 frame has at least one `ENH_RGB` sample and `CHECK(same(...))` with bands 1.

- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build build-mac && ctest --test-dir build-mac -R enh_bands_ --output-on-failure`
Expected: build error `implicit declaration of function 'enh_bands_config'` (or undefined `enh_bands_stats`).

- [ ] **Step 3: Implement `enh_bands_config` and `enh_bands_stats` in `common/enhanced/enh_bands.c`**; make `CMD_MAX` / `ARENA` runtime limits (allocated in `start`); count `flushes` in `enh_flush` when it runs commands outside `enh_bands_run`; time each band with `SDL_GetTicksNS` in `run_band` into a per-band slot the main thread reads after the done semaphores; `enh_bands_shutdown` calls `enh_flush()` first. `fail_after` only stops `SDL_CreateThread` calls (the same path a real failure takes: the warning line, fewer bands).

- [ ] **Step 4: Run them to see them pass**

Run: `cmake --build build-mac && ctest --test-dir build-mac -R enh_bands_ --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 7`

- [ ] **Step 5: Quick suite**

Run: `ctest --test-dir build-mac -LE slow`
Expected: `100% tests passed`

- [ ] **Step 6: Commit**

```bash
git add common/enhanced/enh_bands.c common/enhanced/enh_bands.h tests/test_enh_bands.c tests/CMakeLists.txt
git commit -m "Test the enhanced view's bands on their own"
```

### Task 2: A real-frame check that cannot pass by accident

**Files:**
- Modify: `common/enhanced/enh_bands.c` (`enh_bands_check`, `start`)
- Modify: `tests/test_enh_threads.sh`, `tests/CMakeLists.txt` (`enh_threads` TIMEOUT 900)

**Interfaces:**
- Consumes: Task 1's `enh_bands_config` limits (the env aid sets the same limit).
- Produces: the check line `enh threads check: frames F bands B differing D art A (...)` where `A` counts compared frames whose banded pass drew at least one `ENH_RGB` sample; developer aid `SR_ENH_CMDS=N` (command list size, at least 1; default 32768).

- [ ] **Step 1: Make the test stricter (it fails now)** in `tests/test_enh_threads.sh`: `check NAME MINART` additionally requires `bands` > 1 and, for art runs, `art` > 0, for runs without art `art` = 0. Runs (each `none` and `art` unless noted, `--res-scale 4 --aa 2` unless noted): SR2 `drag`, `cruise`, `grudge` (`--run-ms 200000`), `aqueduct`, `aqking` (aqueduct, `SR_DEBUG_OPP=21`), `mulholland` (`SR_SEED=4`); SR2 extra art runs: `drag_b3` (`SR_ENH_THREADS=3 --res-scale 3 --aa 3`), `aqueduct_b7` (`SR_ENH_THREADS=7 --res-scale 1 --aa 1`), `mulholland_cmds` (`SR_ENH_CMDS=50`, `SR_SEED=4`); SR1 `demo` none and art, plus SR1 art `demo_b3` (`SR_ENH_THREADS=3 --res-scale 3 --aa 3`) and `demo_cmds` (`SR_ENH_CMDS=50`). Run at most 6 games at once.

- [ ] **Step 2: Run it to see it fail**

Run: `ctest --test-dir build-mac -R '^enh_threads$' --output-on-failure`
Expected: `FAIL` lines naming the check line without an `art` field (and the `_cmds` runs unchanged by the unknown env).

- [ ] **Step 3: Implement** in `enh_bands_check`: odd frames draw the banded pass first and the direct one second (xrgb still ends as the banded frame, restored from the copy), report `art`; in `start`: read `SR_ENH_CMDS`.

- [ ] **Step 4: Run it to see it pass**

Run: `ctest --test-dir build-mac -R '^enh_threads$' --output-on-failure`
Expected: an `ok` line for each of the 20 runs, `differing 0`, then `100% tests passed`.

- [ ] **Step 5: Quick suite and SR2's slow drawing tests**

Run: `ctest --test-dir build-mac -LE slow && ctest --test-dir build-mac -R 'sr2_enh_compare|sr2_identity|sr2_marker|sr2_smoke'`
Expected: `100% tests passed` twice.

- [ ] **Step 6: Commit**

```bash
git add common/enhanced/enh_bands.c tests/test_enh_threads.sh tests/CMakeLists.txt
git commit -m "Check banded frames in both orders and every race"
```

### Task 3: Nothing rewritten under a recorded command

**Files:**
- Modify: `src2/enhanced/enh_road2.c` (`enh_sheets_read`), `src2/enhanced/enhanced.c` (`capture_objects`), `src/enhanced/enh_road.c` (`read_sheets`), `common/enhanced/enh_draw.h` (the contract comment)
- Test: `tests/test_sr2_enh.c` (new case), `tests/CMakeLists.txt` if cases are listed there

**Interfaces:**
- Consumes: Task 1's `enh_bands_config`, `enh_bands_stats`.
- Produces: `enh_flush()` at the top of `enh_sheets_read`, `read_sheets` and `capture_objects`; in `enh_draw.h`, the contract line: "a tag reaching a command body must have been looked up with `rm_pic` on the main thread (it loads lazily); bodies only read loaded pictures".

- [ ] **Step 1: Write the failing test** `sr2_enh_sheets_read_flushes` in `tests/test_sr2_enh.c`: `enh_init(true, 1, 2, 100, true)`, `ega_init()`, `enh_bands_config(4, 0, 0, -1)`, write a known pattern to page 1 rows 66h..70h through `ega_plane(k)`, `enh_sheets_read()`, record `enh_sheet_row(j, 0x66 + j / 2.0, 0)` for every sample row, then overwrite the pattern with another and call `enh_sheets_read()` again, `enh_flush()`. Expected `px`: the same calls with `enh_bands_config(1, 0, 0, -1)` (drawn at once, before the overwrite). `CHECK(memcmp(px_banded, px_direct, n) == 0)`. Finish with `enh_bands_config(1, 0, 0, -1)` so later cases draw directly.

- [ ] **Step 2: Run it to see it fail**

Run: `cmake --build build-mac && ctest --test-dir build-mac -R '^sr2_enh$' --output-on-failure`
Expected: `FAIL ... memcmp(px_banded, px_direct, n) == 0`

- [ ] **Step 3: Add the three `enh_flush()` guards and the `enh_draw.h` contract line.**

- [ ] **Step 4: Run it to see it pass**

Run: `cmake --build build-mac && ctest --test-dir build-mac -R '^sr2_enh$' --output-on-failure`
Expected: `100% tests passed`

- [ ] **Step 5: SR1 unchanged, quick and slow suites**

Run: `SRE_PREFIX=$PWD/build-deps/prefix tools/sr1-codecheck.sh 153e779 HEAD --allow host.c.o enhanced.c.o enh_road.c.o enh_raster.c.o enh_sprite.c.o enh_bands.c.o | tail -1` then `ctest --test-dir build-mac -LE slow && ctest --test-dir build-mac -L slow`
Expected: `... 0 differ (allowed: ...)`, then `100% tests passed` twice.

- [ ] **Step 6: Commit**

```bash
git add src2/enhanced/enh_road2.c src2/enhanced/enhanced.c src/enhanced/enh_road.c common/enhanced/enh_draw.h tests/test_sr2_enh.c tests/CMakeLists.txt
git commit -m "Flush recorded commands before rewriting their tables"
```

### Task 4: Where a frame's time goes

**Files:**
- Modify: `src/enhanced/enhanced.c`, `src2/enhanced/enhanced.c` (trace lines)
- Create: `tools/enh-timing.sh`
- Modify: `README.md` (developer aids)

**Interfaces:**
- Consumes: Task 1's `enh_bands_stats`.
- Produces: trace fields appended to both games' `SR_ENH_TRACE` line after the existing `ms`: `draw D resolve R cmds C flushes F bands B bmax X bmean Y` (ms, from `SDL_GetTicksNS` around the drawing and around `enh_resolve`). `tools/enh-timing.sh <streetrod-enhanced> <SR1 dir> <streetrod2-enhanced> <SR2 dir> [SR_ENH_THREADS value]`: builds identity packs, runs the ten runs of the baseline table one at a time, prints `<game> <race> <none|art> n N median M p90 P bmax X bmean Y`, and first prints the 1-minute load average with `warning: load L on C cores, timings unreliable` when L > C / 2.

- [ ] **Step 1: Write the failing check** - a ctest `enh_trace_fields` (`sh -c` over a short SR2 drag run, `--run-ms 15000`, `SR_ENH_TRACE` to a temp file) that greps a trace line for ` draw [0-9.]* resolve [0-9.]* cmds [0-9]* flushes [0-9]* bands [0-9]* bmax [0-9.]* bmean [0-9.]*`.

- [ ] **Step 2: Run it to see it fail**

Run: `ctest --test-dir build-mac -R enh_trace_fields --output-on-failure`
Expected: `FAIL` (no `draw` field).

- [ ] **Step 3: Add the fields to both traces, write `tools/enh-timing.sh`, and the README line:** "`tools/enh-timing.sh` measures the enhanced view's frame time in every race kind (one run at a time; on a quiet machine)."

- [ ] **Step 4: Run it to see it pass, and the script once**

Run: `ctest --test-dir build-mac -R enh_trace_fields --output-on-failure && sh tools/enh-timing.sh build-mac/streetrod-enhanced <SR1 dir> build-mac/streetrod2-enhanced <SR2 dir> | tail -3`
Expected: `100% tests passed`; three result lines.

- [ ] **Step 5: Decide the drag** - read `bmax / bmean` for SR2 drag with art. If it is above 1.5, a single band holds the work: write the finding (which rows, which command kind) under the ledger at the end of this plan for the owner; the bands stay one per core (the owner's design). If `draw` dominates and `bands` work is small, the cost is recording (image decoding, `attach_art`): note it likewise. No code change in this step.

- [ ] **Step 6: Commit**

```bash
git add src/enhanced/enhanced.c src2/enhanced/enhanced.c tools/enh-timing.sh README.md tests/CMakeLists.txt docs/superpowers/plans/2026-11-12-enhanced-threads.md
git commit -m "Split the enhanced view's frame time in the trace"
```

### Task 5: Measure, document, final check

**Files:**
- Modify: `ENHANCED.md` (SR1 section "Worker threads" bullet, SR2 section's timing sentence), `CHANGELOG.md` (`Unreleased`), `README.md` (aids: `SR_ENH_THREADS`, `SR_ENH_THREADS_CHECK`, `SR_ENH_CMDS`, `SR_ENH_TRACE` fields), this plan (results)

**Interfaces:**
- Consumes: Tasks 1 to 4.
- Produces: final timings in the docs.

- [ ] **Step 1: Measure on a quiet machine** (load average under 2): `sh tools/enh-timing.sh build-mac/streetrod-enhanced <SR1 dir> build-mac/streetrod2-enhanced <SR2 dir> 1` and again without the last argument. Also build `153e779` in a temporary folder outside the repo (`git archive 153e779`) and run the same script against it for the "before" column (its SR2 has no trace: add the one-line trace there only, never commit it). If the machine cannot be made quiet, say so in the docs next to the numbers.

- [ ] **Step 2: Write the numbers** - replace the 1fbc94f figures in ENHANCED.md (SR1 bullet and SR2 paragraph) and in CHANGELOG (keep the credit: "bsvdoom's change to kylofon/streetrod-enhanced") with the measured before / one band / all bands medians for SR1 and SR2 with and without art; mention `SR_ENH_CMDS` and the alternating check; confirm `grep -n -i "worker threads" ENHANCED.md` shows no not-done list entry.

- [ ] **Step 3: ENGINE2.md** - `git diff 153e779 -- src2/game | grep '^[-+]' | grep -v '^[-+][-+]'`
Expected: no output (no game hook changed, so ENGINE2.md stays as it is).

- [ ] **Step 4: Final checks**

Run: `SRE_PREFIX=$PWD/build-deps/prefix tools/sr1-codecheck.sh 153e779 HEAD --allow host.c.o enhanced.c.o enh_road.c.o enh_raster.c.o enh_sprite.c.o enh_bands.c.o | tail -1 && ctest --test-dir build-mac -LE slow && ctest --test-dir build-mac -L slow`
Expected: `... 0 differ ...`, `100% tests passed` twice.

- [ ] **Step 5: Commit**

```bash
git add ENHANCED.md CHANGELOG.md README.md docs/superpowers/plans/2026-11-12-enhanced-threads.md
git commit -m "Document the enhanced view's measured frame times"
```

## Ledger (rulings and findings for the owner)

- `1fbc94f` landed before this plan existed; the plan treats it as the first cut and keeps its design.
- Timings above were taken under load average 64 to 115; Task 5 replaces them.
