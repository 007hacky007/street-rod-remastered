# HD Remaster Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show every Street Rod picture as redrawn modern pixel art (an optional, local art pack), with a launcher choice between Original and Remastered art, and produce that pack with ChatGPT.

**Architecture:** Per-pixel tags (picture slot, u, v) follow picture data through the VGA model and the RAM bitmap helpers; `compose` replaces verified tagged pixels with 4x art, recoloured relative to the live palette, and the enhanced race view samples the same art through the tags of its source bitmaps. A Python pipeline extracts the originals (through the engine), builds prompts, cleans up ChatGPT's results onto the exact 4x grid, checks them and writes the pack.

**Tech Stack:** C11 + SDL3 (engine), vendored `stb_image.h` (PNG decode), C++17 + wxWidgets 3.2 (launcher), Python 3 + Pillow + numpy + pytest (pipeline; no scipy), ChatGPT through Claude in Chrome (art).

**Spec:** `docs/superpowers/specs/2026-11-10-hd-remaster-design.md`

## Global Constraints

* Game behaviour, timing and hotspots never change; with no `--remaster-dir` the game output is byte-identical to today.
* Art pack format: `pics/<sha1>.png` (RGBA, exactly 4x the original w x h), `pics/<sha1>.idx.png` (8-bit, original palette index per texel), `pack.json` (`{"format": 1, "pictures": {"<sha1>": {"ref": [16 x "RRGGBB"]}}, "shapes": {...}}`). sha1 = SHA-1 of the picture's packed bytes as stored in the library (without the trailing BCh).
* Shapes: key `shape-<type>-<sha1 of the shape's data>`, same files and fields.
* `remaster-art/` is git-ignored; no art is committed, attached to a release or uploaded anywhere except to ChatGPT.
* Tools in `tools/remaster/`, catalogue `tools/remaster/catalog.json` (committed, no art).
* Style reference: `remaster-art/spike/garage-pixel.png`. Failures to avoid: the radio turned into a rack, the wrench pointer painted onto the wall.
* No Unicode dashes in any written file; commits as 007hacky007, short imperative subjects, no AI attribution.
* Owner judges finished screens in batches; single pictures are checked by the tools and by the builder.

## Review Focus

1. A screen where a tagged pixel shows art from the wrong picture (stale tags in reused arena memory, pre-shifted copies, the pointer save-under): the identity-pack test in Task 4 must cover the demo, garage, shops, newspaper, race and results.
2. Palette tricks: garage paint (regs 6/7, preview reg 12), opponent colours, `pal_black`/`pal_normal`, the blinds wipe, colour cycling: Task 4's colour tests plus the identity-pack frames during these effects.
3. Sprite edges over backgrounds (cars over the garage floor, pointer over anything): no hard 4x staircase and no halo; Task 4 under-tag blending, checked on the marker pack.
4. The race view with the enhanced view on and off, and the split screen dashboard: Task 5.
5. ChatGPT results that move or resize objects locally: Task 8 checks must reject them, tested with a synthetic moved object.

---

### Task 1: Picture dump through the engine

**Files:**
- Modify: `src/main.c` (option `--dump-pictures DIR`, like `--check`: no window)
- Create: `src/remaster/rm_dump.c`, `src/remaster/rm_dump.h`
- Test: `tests/test_dump.sh` (ctest, needs `SRE_TEST_GAME_DIR`)

**Interfaces:**
- Produces: `int rm_dump_pictures(const char *dir)`: for every directory index of LIB1 and LIB2 of the loaded game, writes `<dir>/<sha1>.bmp` (8-bit indexed, `ega_palette` order, w x h), `<dir>/<sha1>.mask.bmp` for masked pictures (255 = transparent), and appends a line to `<dir>/pictures.jsonl`: `{"sha1","lib","index","id","w","h","masked"}` (id = 1..9 for LIB1, 1000.. for LIB2). Returns the number written, -1 on error.
- Produces: `--dump-pictures DIR` exits 0 after dumping.

- [ ] **Step 1: Write the failing test** `tests/test_dump.sh`: runs `streetrod-enhanced --game-dir "$SRE_TEST_GAME_DIR" --dump-pictures "$tmp"`, then asserts `wc -l pictures.jsonl` = 277, that the record with id 2 has w 24 h 64 masked 1, and that every sha1 in the file has a `.bmp`. Register it in `tests/CMakeLists.txt` only when `SRE_TEST_GAME_DIR` is set.
- [ ] **Step 2: Run it, expect FAIL** (`unknown option`).
- [ ] **Step 3: Implement** with the engine's own loaders: `lib_open_all`, `lib_read_dir(1/2)`, `pic_get` per id, plane bits to indices as in `enh_image_bitmap` (`enh_sprite.c:41`); the packed bytes for the SHA-1 come from the directory entry's offset/size (`PE_OFFSET`, `PE_SIZE`, `lib.c:17`). SHA-1: a small public-domain implementation in `rm_dump.c`. BMP via `SDL_SaveBMP` on an 8-bit surface.
- [ ] **Step 4: Run, expect PASS** for the main game; run once by hand with the data disk (`--game-dir Game/datadisk`) and check 277 records.
- [ ] **Step 5: Verify against the screen:** the garage background picture (the 320x174 or 320x190 one matching `remaster-art/spike/garage-original.png`) equals the snapshot's pixels outside the sprites. Write the id in the task notes.
- [ ] **Step 6: Commit** `git commit -m "Dump the game's pictures for the remaster"`

### Task 2: Pipeline core: catalogue, packs for testing

**Files:**
- Create: `tools/remaster/rmlib/{__init__,images,catalog,pack}.py`, `tools/remaster/rm.py` (CLI), `tools/remaster/tests/test_pack.py`, `tools/remaster/tests/test_catalog.py`, `tools/remaster/README.md`
- Create: `tools/remaster/catalog.json` (initially from the dumps)

**Interfaces:**
- Consumes: Task 1's dump layout.
- Produces CLI: `rm.py catalog DUMP...` (merge dumps of both games into `catalog.json`: per sha1 `ids`, `games`, `w`, `h`, `masked`, `group`, `batch`, `desc`, `keep`, `sheet`, `status`; existing human fields preserved), `rm.py identity-pack DUMP OUT` (4x nearest copies, idx = original indices, ref = `ega_palette`), `rm.py marker-pack DUMP OUT` (as identity, but each picture tinted with a colour derived from its sha1 and a 1-texel checker so coverage and seams are visible), `rm.py install PACK DEST`.
- Produces: `rmlib.pack.write_entry(out, sha1, rgba: np.ndarray[h4,w4,4], idx: np.ndarray[h4,w4], ref: list[int])`.

- [ ] **Step 1: Tests:** `test_identity_pack_is_4x_nearest` (a 3x2 synthetic dump picture becomes a 12x8 RGBA whose 4x4 blocks equal the palette colours, idx equals the indices, masked pixels alpha 0); `test_catalog_merge_keeps_human_fields` (re-running `catalog` keeps `desc`/`keep`/`batch` and adds the second game's ids).
- [ ] **Step 2: Run `python3 -m pytest tools/remaster/tests -q`, expect FAIL.**
- [ ] **Step 3: Implement.** Group guess from size and ids (full screen >= 300 wide: `screen`; others `sprite`); batch left empty for the builder.
- [ ] **Step 4: Run, expect PASS.**
- [ ] **Step 5: Generate the catalogue** from both dumps; expect 383 entries.
- [ ] **Step 6: Commit** `git commit -m "Add the remaster pipeline core"`

### Task 3: Tags through the VGA model and the bitmap helpers

**Files:**
- Create: `src/remaster/rm_tags.c`, `src/remaster/rm_tags.h`
- Modify: `src/platform/vga.c` (`vga_rd`, `vga_wr`, `vmovedata`), `src/platform/vga.h` (`vrd`/`vwr` for RAM), `src/platform/lib.c` (`pic_decode` seeds), `src/platform/blit.c` (`rows_composite`, `rows_mask_or`, `rows_mask_blend`, `ega_latch_copy`, `ega_draw_cursor`, the unpacker clears), `src/platform/gfx.c` (`bitmap_make_shifted`, `bitmap_composite_behind`, `bitmap_blit_masked_shift`), `src/platform/drv_ega.c` (`shift_row` / `ega_put_shifted_row` / `ega_save_rect`), `src/game/race_road.c` (`pic_shrink_planar` clears)
- Test: `tests/test_tags.c` (links `vga.c`, `ega.c`, `rm_tags.c`, `mem.c` with stubs for host and enhanced)

**Interfaces:**
- Produces (`rm_tags.h`):
  - `typedef u32 RmTag;` bit 31 valid, bits 18-26 slot (1..511), bits 9-17 u, bits 0-8 v. `RmTag rm_tag(int slot, int u, int v)`, `int rm_tag_slot(RmTag)`, `int rm_tag_u(RmTag)`, `int rm_tag_v(RmTag)`.
  - `bool rm_tags_on(void)`; `void rm_tags_enable(void)` (allocates VRAM tags 64K x 8 and RAM tags MEM_SIZE x 8, top and under; nothing allocated or done when off).
  - `RmTag rm_vtag(u16 off, int bit)`, `RmTag rm_vunder(u16 off, int bit)`, `RmTag rm_mtag(u32 lin, int bit)`, `RmTag rm_munder(u32 lin, int bit)` (bit 7 = leftmost pixel).
  - Hooks: `rm_on_vram_read(u16 off)`, `rm_on_vram_write(u16 off, u8 bits_written, int mode)`, `rm_on_ram_read(u32 lin)`, `rm_on_ram_write(u32 lin, u8 bits)`, `rm_copy_begin()` / `rm_copy_end()` (while armed, writes take the tags of the last read; otherwise mode 1 takes latch tags, bits outside the bit mask keep the latch tags, every other written bit is cleared), `rm_seed_bitmap(FarPtr planes, FarPtr mask, int w, int h, int stride, int slot)`, `rm_masked_pixel(dst, src, keep_under)` helpers for the CPU compositing code: a pixel taken from the source gets the source tag as top and the old destination top as under.
- Rule: every hook is a no-op when `rm_tags_on()` is false, so the original output is unchanged.

- [ ] **Step 1: Tests in `tests/test_tags.c`:**
  - `seed_then_ram_to_vram_copy` (seed a 16x2 RAM bitmap as slot 5, blit per plane to VRAM through `vmovedata` with map masks 1,2,4,8: every VRAM pixel tag = (5,u,v)).
  - `latch_copy_moves_tags` (write mode 1 VRAM->VRAM copy of a row moves tags to the destination).
  - `text_write_clears` (write mode 2 with bit mask 0x0F over a tagged byte: the 4 right pixels cleared, the 4 left kept).
  - `set_reset_fill_clears`.
  - `masked_or_sets_under` (`rows_mask_or` of slot 7 over slot 5 with a mask: opaque pixels top 7 under 5, transparent pixels top 5).
  - `shift_moves_tags` (`bitmap_make_shifted` by 3: tag of source pixel u at destination u+3).
  - `off_changes_nothing` (tags off: plane bytes after a sequence of writes equal a run without the hooks).
- [ ] **Step 2: Run `ctest -R tags`, expect FAIL** (no module).
- [ ] **Step 3: Implement `rm_tags.c` and the hooks.** Slot = directory index + 1 assigned in `pic_decode` from the entry (`pic_index(id)`), `rm_seed_bitmap` on the decoded planes (both planes and mask region of `pump_overlay`'s quirk: seeding follows `dst`'s planes only).
- [ ] **Step 4: Run, expect PASS.** Also run the existing tests.
- [ ] **Step 5: Commit** `git commit -m "Track which picture pixel each screen pixel shows"`

### Task 4: Pack loading and compose

**Files:**
- Create: `src/remaster/rm_pack.c`, `src/remaster/rm_pack.h`, `src/remaster/rm_color.c`, `src/remaster/rm_color.h`, `src/third_party/stb_image.h` (vendored, version and licence noted in its header)
- Modify: `src/platform/ega.c` (`compose`), `src/main.c` (`--remaster-dir DIR`; with it, `res_scale` defaults to 4 even with `--classic`), `src/platform/lib.c` (slot -> sha1 table filled at `lib_read_dir`)
- Test: `tests/test_color.c`, `tests/test_identity.sh` (needs `SRE_TEST_GAME_DIR`)

**Interfaces:**
- Produces: `bool rm_pack_open(const char *dir)`; `void rm_slot_set(int slot, const char sha1[41], const u8 *orig_idx, int w, int h)`; `const RmPic *rm_pic(int slot)` with `w4, h4, rgba, idx, ref[16]` and the original index map; `u32 rm_shade(u32 texel_rgb, u32 ref_rgb, u32 cur_rgb)`.
- `rm_shade` rule: `cur == ref` -> texel; else `k = (lum(texel)+1)/(lum(ref)+1)` (lum = 0.299r+0.587g+0.114b), each channel `k <= 1 ? cur*k : cur + (255-cur)*(1-1/k)`, clamped.
- Compose rule: for each screen pixel with top tag T valid, `rm_pic(slot)` present and `orig_idx(u,v)` == current pixel index: fill the S x S block from the art (scale 4 copies, other S resample by nearest), each texel through `rm_shade(texel, ref[idx], rgb[idx])`; texels with alpha < 255 blend over the under tag's art (same rules) or, without one, over the original pixel colour. Pixels that fail any check keep the original block.

- [ ] **Step 1: Tests.** `test_color.c`: `shade_same_is_identity`, `shade_black_is_black`, `shade_keeps_highlight_order` (two texels brighter/darker than ref stay in that order after recolour to red). `test_identity.sh`: build the identity pack (Task 2) from the dump, run the demo twice with `SR_SNAPSHOT_DIR` and the same `SR_KEYS` script through intro, garage, shops, newspaper, a race and results, once without and once with `--remaster-dir`, assert every frame pair is identical.
- [ ] **Step 2: Run, expect FAIL.**
- [ ] **Step 3: Implement.** Pack open reads `pack.json` with a minimal JSON scan (only the fields above), loads PNGs lazily on first use of a slot.
- [ ] **Step 4: Run, expect PASS.** Then run the marker pack on the same script and look at the garage, shop and race frames: tint everywhere a picture is shown, no tint on printed text and lines. Note coverage gaps in the task notes and fix them in Task 3's hooks (each with a new test there).
- [ ] **Step 5: Commit** `git commit -m "Show remastered pictures from an art pack"`

### Task 5: Remastered art in the enhanced race view

**Files:**
- Modify: `src/enhanced/enh_sprite.c` (`enh_image_bitmap` records slot and u/v origin from the bitmap's RAM tags when they are consistent), `src/enhanced/enh_raster.c` / `enh_road.c` (sample the art at 4x through `rm_shade` when the image has art)
- Test: extend `tests/test_identity.sh` with a race segment, enhanced view on and off

**Interfaces:**
- Consumes: `rm_mtag`, `rm_pic`, `rm_shade`.
- Produces: `EnhImage` gains `int rm_slot, rm_u0, rm_v0` (0 = none).

- [ ] **Step 1: Test:** identity pack, race frames equal with and without `--remaster-dir` (enhanced on; the enhanced renderer's own resampling of the identity art must match its resampling of the original).
- [ ] **Step 2: Run, expect FAIL** where the race view still reads only original indices (equal frames would mean the art is not used: assert via the marker pack that the road-side signs are tinted).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run, expect PASS.**
- [ ] **Step 5: Commit** `git commit -m "Use the remastered art in the race view"`

### Task 6: Race-view shapes

**Files:**
- Modify: `src/remaster/rm_dump.c` (also dump the 20 shape types of `DS_shape_table`, each rendered at 8x by the enhanced shape code into an indexed BMP with mask, key `shape-<type>-<sha1>`), `src/enhanced/enh_road.c` (`shape()`: with art for the key, draw the art scaled into the shape's screen box instead of the lines)
- Test: `tests/test_dump.sh` (20 shape records), `tests/test_identity.sh` race segment still equal with an identity pack that has no shapes, and the marker pack shows tinted trees

- [ ] **Step 1: Tests** as above. **Step 2: FAIL.** **Step 3: Implement.** **Step 4: PASS.**
- [ ] **Step 5: Commit** `git commit -m "Allow remastered race-view shapes"`

### Task 7: Launcher

**Files:**
- Modify: `launcher/launcher.cpp/.h` (rename Graphics to "Race view"; add "Art: Original / Remastered", enabled when the pack folder holds `pack.json`), `launcher/game.cpp/.h` (`Options.remaster_dir`; pass `--remaster-dir`), `launcher/settings.cpp` (key `art`), `launcher/README.md`, `README.md`
- Test: `tests/test_gamefiles.cpp` style unit test `test_args.cpp` for the argument list

**Interfaces:**
- Default pack folder: `settings::Folder()/Remaster` (Mac: Application Support; Windows: next to the executable).

- [ ] **Step 1: Test** `args_remastered_adds_dir`, `args_original_has_no_dir`. **Step 2: FAIL.** **Step 3: Implement.** **Step 4: PASS**, then run the launcher by hand both ways.
- [ ] **Step 5: Commit** `git commit -m "Add the Art choice to the launcher"`

### Task 8: Cleanup, checks, prompts, sheets, progress

**Files:**
- Create: `tools/remaster/rmlib/{register,cleanup,check,prompt,sheet,progress}.py`, `tools/remaster/style.md` (the style guide), tests `tools/remaster/tests/test_{register,cleanup,check,sheet,progress}.py`
- Modify: `tools/remaster/rm.py` (subcommands `job`, `next`, `ingest`, `pack`, `status`)

**Interfaces:**
- `register(orig_rgb, result_rgb) -> (scale, dx, dy, score)`: search scale and offset (coarse to fine on downscaled edge maps and colours) that best maps the result onto the 4x original.
- `cleanup(orig, mask, result, reg) -> (rgba, idx)`: crop/resample to exactly 4x, alpha = 0 outside the original mask, anti-aliased inside a 1-original-pixel border, colour count reduced to at most 64 per picture without dithering, idx = per texel the original index among the 3x3 neighbourhood whose reference colour is nearest.
- `check(orig, mask, rgba, idx) -> (ok, reasons)`: thresholds tuned on batch 1 and stored in `check.py`; must reject a synthetic object moved by 2 original pixels or recoloured to another palette colour, and a background with a pointer-shaped blob added.
- `job(sha1 or sheet) -> remaster-art/jobs/<name>/{input.png,prompt.txt}`; input = original at 4x nearest on a margin canvas of aspect 3:2, 2:3 or 1:1 (key colour background for masked pictures, transparency requested in the prompt); prompt = `style.md` + the catalogue's `desc`/`keep`.
- `sheet(group, sha1s) -> job` with one cell per sprite and a cell map; `ingest` cuts sheets by the cell map before registration.
- `progress.json` in `remaster-art/`: per job `pending|sent|done|failed|attention`, tries, reasons. `next` prints the next pending job.

- [ ] **Step 1: Tests** with synthetic images for each interface (known scale/offset recovered within 1 texel; moved object rejected; sheet round trip). **Step 2: FAIL.** **Step 3: Implement.** **Step 4: PASS.**
- [ ] **Step 5: Calibrate on the spike:** `register` + `check` on `garage-pixel.png` against the garage background must flag the radio and the wrench pointer.
- [ ] **Step 6: Commit** `git commit -m "Add the remaster cleanup and checks"`

### Task 9: Batch 1 - garage, shops, newspaper, menus, dialogs, pointer

- [ ] **Step 1: Catalogue:** view the batch's pictures in contact sheets, write `desc`, `keep`, `group`, `batch`, `sheet` for each; commit `catalog.json` (`git commit -m "Describe the batch 1 pictures"`).
- [ ] **Step 2: Generate:** in a ChatGPT project "Street Rod Remaster", for each `next` job: new chat, attach `input.png`, send `prompt.txt`, wait, download the result (Downloads, moved to `remaster-art/raw/`), `ingest`. Respect limits (wait and resume); stop and report on login, captcha or anything that needs the owner.
- [ ] **Step 3: Repair** failures in the spec's order; leftovers become `attention` with reasons.
- [ ] **Step 4: Pack and install;** play the batch's screens with snapshots, compare against the originals for layout and style consistency.
- [ ] **Step 5: Hand to the owner:** the screens to look at, how to switch Art in the launcher, the attention list. Feedback goes into `keep` notes and re-runs.

### Task 10: Batch 2 - cars and car parts (both games)

Same steps as Task 9 for the car pictures. Extra check: every car repainted in the garage (all paint colours) looks right through `rm_shade`; the body, bumper, roof, scoop, sticker, wheels and shadow pictures join without seams.

### Task 11: Batch 3 - race view

Same steps for signs, sky, horizon, opponent and police cars, cockpit, dashboard, then the 20 shapes. Extra check: a race with the enhanced view on and off, mirror included.

### Task 12: Batch 4 - everything left, docs

Same steps for the remaining pictures; then `rm.py status` shows no `pending`. Write `REMASTER.md` (how the pack works, how to rebuild or redo a picture, the owner's distribution decision pending) and update CHANGELOG. Commit `git commit -m "Document the remaster"`.
