# Street Rod 2 Remastered Pictures Engine (plan 5a) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Street Rod 2 gets the remastered picture engine Street Rod 1 has: its pictures can be dumped, catalogued as a third source `sr2` and shown from the art pack (classic view and enhanced race view), the launcher's Art choice works for it, and the SR2 dump, catalogue merge and palette logs are made.

**Architecture:** The game-independent remaster core (`rm_tags`, `rm_pack`, `rm_color`) moves unchanged from `src/remaster/` to `common/remaster/` and is built into each game's core with that game's `mem.h`, `platform/` and `remaster/rm_dump.h`, as `common/enhanced` is. Street Rod 2 gets the same `ENH:` hooks Street Rod 1 has in its VGA model and blit helpers (its platform code is the same transcription without them), its own dump (`src2/remaster/rm_dump.c`: LIB1/LIB2/LIB3, SR2's record format) and a small `src2/remaster/rm_lib.c` that hashes a picture's packed bytes before SR2's in-place unpack overwrites them. The enhanced race view samples the art through the VRAM tags of what it captures (the sky and horizon sheets as SR1's does, and the cut-out objects).

**Tech Stack:** C11 + SDL3 (engine), C++17 + wxWidgets 3.2 (launcher), POSIX sh + Python 3 (tests), Python 3 + Pillow + numpy + pytest (`tools/remaster`).

**Spec:** `docs/superpowers/specs/2026-11-11-street-rod-2-design.md` (section "Remastered pictures"), with `docs/superpowers/specs/2026-11-10-hd-remaster-design.md` (how the engine shows the art) and Street Rod 1's plan `docs/superpowers/plans/2026-11-10-hd-remaster.md`.

## Paths used in the commands

The absolute paths are the session's and are not written here (no file may hold the macOS username):

```
R=<the main checkout: the folder that holds .worktrees/ and remaster-art/>
W=$R/.worktrees/sr2-art          # the worktree (branch sr2-art)
B=$W/build-art                   # its build dir
ART=$R/remaster-art              # shared art folder (git-ignored)
G1=<Street Rod 1's game folder (SR.EXE, LIB1, LIB2, datadisk/), the session's SRE_TEST_GAME_DIR>
G2=<Street Rod 2's game folder (SR.EXE, LIB1..LIB3, SR2_*), the session's SRE_TEST_GAME2_DIR>
```

Configure once:

```
cmake -S $W -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_PREFIX_PATH=$R/build-deps/prefix "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local" \
  -DSRE_LAUNCHER=ON -DSRE_TESTS=ON -DSRE_TEST_GAME_DIR=$G1 -DSRE_TEST_GAME2_DIR=$G2
```

Build and quick tests: `cmake --build $B && ctest --test-dir $B -LE slow`. Slow tests (`sr2_smoke` about 7 min, `sr2_enh_compare`, and this plan's `sr2_identity`, `sr2_marker`): `ctest --test-dir $B -L slow`, at the end of Tasks 4, 5 and 8. Python: `python3 -m pytest tools/remaster/tests -q` (the system `python3` has Pillow, numpy and pytest; 26 tests pass before this plan).

## Global Constraints

- Work only in `$W` on branch `sr2-art`; never edit the main checkout `$R` itself (another track works on master there); never push.
- Street Rod 1 does not change. After every task's commit, `tools/sr1-codecheck.sh 50331e9 HEAD` (50331e9 = the branch point) ends with `0 differ (allowed: )`. This plan needs no `--allow`: the moved files keep their names and their code. Any SR1 object that would differ is a plan bug: stop, give the reason in the ledger, and change the approach before allowing it.
- Pack format unchanged (`pack.json` `{"format": 1, "pictures": {...}, "shapes": {...}}`, `pics/<sha1>.png` RGBA exactly 4x, `pics/<sha1>.idx.png`), keyed by the SHA-1 of the picture's packed bytes from the leading BCh (`PE_SIZE` bytes). One pack holds both games' art; an SR2 picture byte-identical to an SR1 one shows SR1's art with no extra work.
- A picture without art shows the original. Without `--remaster-dir`, Street Rod 2 runs exactly as now: every hook is a no-op while `rm_tags_on()` / `rm_pack_active()` are false.
- No pictures or anything derived from the game's pixels are committed. `tools/remaster/catalog.json` (sizes, ids, descriptions) is committed. SR2 dumps go to `$ART/dump/sr2`; SR2 packs for testing to `$ART/identity-sr2` and `$ART/marker-sr2`. Do not touch SR1's art in `$ART` (`art/`, `pack/`, `identity/`, `dump/main`, `dump/datadisk`).
- Out of scope: `SR2_SHP` road-side shapes (need parts B/C of the race view), generating art.
- Street Rod 2 runs in tests and logs always get `demo v adlib` (or `v adlib`) and the SDL dummy drivers (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`), with `--run-ms N` to end.
- Markers: our deliberate changes in either engine say `ENH:`.
- Commits: one short imperative subject under about 60 chars, no body, no trailers, no AI attribution. Never write the macOS username or a real name into any file. Only ASCII hyphen-minus, no Unicode dashes, in anything written.

## Review Focus

1. **SR2 decodes pictures in place** (`pic_decode` reads the packed data into the tail of the destination bitmap, then unpacks over it): the SHA-1 must be taken before `pic_unpack`, or every picture gets a wrong key and silently shows the original. Pinned by `sr2_decode_hash_in_place` (Task 4) and by the identity run's palette log, whose SHA-1s must all be in the dump (Task 4).
2. **Pictures parked in VRAM** (`pic_park_list`, read back through the read map select): the hash reads the parked plane directly (`ega_plane`), not through `vga_rd`, so it has no side effects on the latches or the tags (`pic_unpack` reloads the latches itself right after, so this is hygiene, not a visible bug). Pinned by `sr2_hash_parked_keeps_latches` (Task 4).
3. **A pack with art for only some SR2 pictures** (every player's case: today's pack has SR1's art only) and a missing pack folder: the game must look exactly as without a pack and keep running. Pinned by the half-pack run and the missing-folder run of `test_sr2_identity.sh` (Task 4).
4. **Stale tags in reused arena memory** under the shrunk and rotated race sprites (`pic_shrink_planar`, `pic_rotate_planar` write planes with `wr8`, which moves no tags): their destinations must be cleared by `bitmap_clear` first (`pic_alloc_shrunk`, `rot_alloc` do), or old art shows on a car. Pinned by `sr2_bitmap_clear_clears_tags` (Task 3). The identity runs cannot catch it (a stale tag that passes the colour check shows the identity art's same colour); the marker drag run (Task 5) and the marker snapshots are where it would show.
5. **The compare tool and the enhanced view with a pack**: art samples (`ENH_RGB`) must not reach the compare tool (`compare_frame` runs from `enh_frame_presented`, outside `enh_compose`, with the last frame's `enh_cur_rgb` still set), and the enhanced view must still match the original. The compare counts mismatches only below the horizon strip, so the sky's art samples would not show in its numbers: pinned by the `drag_art` run of `test_sr2_enh_compare.sh` checking its `SR_ENH_COMPARE_DUMP` images for the magenta it paints for a non-palette sample, and by the enhanced identity runs (Task 5).
6. **Byte-identical is not pixel-identical by itself**: SR2's unpacker differs from SR1's (`00`/`FF` followed by `00` is one escape byte `tokens[0]`/`tokens[1]` in SR2, one `00`/`FF` in SR1) and reads its own run tables (`DS_run_counts` 4B72h, SR1's 4EEEh). The 48 shared pictures decode the same only because they have the same 16 tokens, contain no such escape pair, and the two run tables are equal (checked at review time). Pinned by the SR1-against-SR2 dump comparison of `test_sr2_dump.sh` (Task 2) and of Task 8 Step 2.

---

### Task 1: The remaster core moves into common

**Files:**
- Move (git mv): `src/remaster/rm_tags.c`, `rm_tags.h`, `rm_pack.c`, `rm_pack.h`, `rm_color.c`, `rm_color.h` to `common/remaster/`
- Stay: `src/remaster/rm_dump.c`, `rm_dump.h` (they read SR1's `symbols.h`)
- Modify: `CMakeLists.txt` (glob `common/remaster/*.c` as `SR_RM_CORE_SOURCES` into `sre_core`; not into `SR_ENHANCED_SOURCES`, so the compile flags stay as they are), `common/README.md` (a `remaster/` paragraph: built into each game's core like `enhanced/`, includes that game's `mem.h`, `platform/vga.h`, `platform/ega.h` and `remaster/rm_dump.h` for `rm_sha1_hex`), `README.md:224` (the path), the first comment lines of `tests/test_tags.c`, `tests/test_color.c`, `tools/remaster/rmlib/repair.py` (the path)

**Interfaces:**
- Produces: `common/remaster/rm_tags.h`, `rm_pack.h`, `rm_color.h`, reachable as `"remaster/rm_*.h"` from either game (the quoted include falls through to `common/`, on the include path of `sre_host`). No file content changes beyond the comments named above.

- [ ] **Step 1: Move and build**

Run: `cd $W && git mv src/remaster/rm_tags.c src/remaster/rm_tags.h src/remaster/rm_pack.c src/remaster/rm_pack.h src/remaster/rm_color.c src/remaster/rm_color.h common/remaster/` (create `common/remaster` first), edit `CMakeLists.txt`, then `cmake --build $B && ctest --test-dir $B -LE slow`
Expected: build ok; `100% tests passed` (including `tags`, `color`, `compose`, `dump`, `identity`).

- [ ] **Step 2: Commit and check SR1's code**

```bash
git add -A CMakeLists.txt common src README.md tests tools
git commit -m "Move the remaster core into common"
tools/sr1-codecheck.sh 50331e9 HEAD
```
Expected: every object `same`, last line ends `0 differ (allowed: )`.

### Task 2: Street Rod 2's picture dump

**Files:**
- Delete: `src2/remaster/rm_pack.h`, `src2/remaster/rm_tags.h` (the stand-ins; the real headers in `common/remaster` take their place)
- Create: `src2/remaster/rm_dump.c`, `src2/remaster/rm_dump.h`
- Modify: `CMakeLists.txt` (`SR_RM_CORE_SOURCES` into `sr2_core` too), `src2/main.c` (`--dump-pictures DIR`, usage and header comment), `ENGINE2.md:37` (no stand-ins: the remaster core from `common/remaster`)
- Test: `tests/test_sr2_dump.sh` (ctest `sr2_dump` under `SRE_TEST_GAME2_DIR`, arguments `$<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR}`, plus `$<TARGET_FILE:streetrod-enhanced> ${SRE_TEST_GAME_DIR}` when `SRE_TEST_GAME_DIR` is set, the two forms as `sr2_check` registers them)

**Interfaces:**
- Produces (`src2/remaster/rm_dump.h`):
  - `int rm_dump_pictures(const char *game_dir, const char *out);` every picture of LIB1, LIB2, LIB3 in SR1's dump format: `<out>/<sha1>.bmp` (8-bit, `ega_palette` order), `<out>/<sha1>.mask.bmp` for masked pictures (255 = transparent), one line per picture in `<out>/pictures.jsonl`: `{"sha1":..,"lib":L,"index":i,"id":ID,"w":..,"h":..,"masked":m}` with ID = `i + 1` (LIB1), `1000 + i` (LIB2), `2000 + i` (LIB3). No `shapes.jsonl`. Returns the number of records, -1 on error (message on stderr). Needs the program image loaded (`mem_load_exe`: the unpacker's run tables `DS_run_counts`).
  - `void rm_sha1_hex(const unsigned char *p, unsigned long n, char hex[41]);` (used by `common/remaster/rm_pack.c`).
- Produces: `streetrod2-enhanced --game-dir DIR --dump-pictures OUT` prints `N pictures written to OUT` and exits 0 after `mem_load_exe` (no window, like `--check`).
- SR2's library format (FORMATS.md, `src2/platform/lib.c:17-27`): u16 count, then `count` 28-byte records: w `+0`, h `+2`, depth `+4`, masked `+5` (bit 1: mask only), 16 tokens `+6`, u32 offset `+16h` (from `2 + count * 28`), u16 packed size `+1Ah`. Counts: LIB1 4, LIB2 212, LIB3 83. Unpack with SR2's `void pic_unpack(FarPtr src, FarPtr dst, u16 n, const u8 *tokens, u16 ntok)` (`src2/platform/blit.c:61`) on the bytes after the BCh, `n = size - 1`, `tokens = record + 6`, `ntok = 0x10`.
- The SHA-1 is a copy of SR1's (`src/remaster/rm_dump.c:20-55`), not a move: moving it would change SR1's `rm_dump.c.o`.

- [ ] **Step 1: Write the failing test** `tests/test_sr2_dump.sh <streetrod2-enhanced> <SR2 game dir> [<streetrod-enhanced> <SR1 game dir>]`, each check printing `ok   NAME` or `FAIL NAME`:
  - the dump's stdout is `299 pictures written to $out`;
  - `pictures.jsonl` has 299 lines, 297 distinct sha1s; `grep -c '"lib":1,'` = 4, `'"lib":2,'` = 212, `'"lib":3,'` = 83; the record `"lib":3,"index":0` has `"id":2000`;
  - every sha1 has `<sha1>.bmp`, every record with `"masked"` not 0 has `<sha1>.mask.bmp`; there is no `shapes.jsonl`;
  - an independent reader (`python3 -I -c` with `hashlib`, `struct`, `PIL.Image`) recomputes each record's SHA-1 from the library file (the bytes at its offset, `size` of them, the first one BCh) and finds each BMP is w x h;
  - a second dump into another folder is identical (`diff -r` prints nothing);
  - with the SR1 arguments (`shared`): SR1's dump of its main game into a third folder; the sha1s in both `pictures.jsonl` are 48, and for each of them `cmp` finds `<sha1>.bmp` (and `<sha1>.mask.bmp` where SR1 has one) identical in the two dumps (Review Focus 6: the same key must mean the same pixels). Without them the check prints `skip shared`.

  Register it in `tests/CMakeLists.txt` inside the `SRE_TEST_GAME2_DIR` block.

- [ ] **Step 2: Run it to see it fail**

Run: `cmake --build $B && ctest --test-dir $B -R sr2_dump --output-on-failure`
Expected: FAIL (the program prints its usage for `--dump-pictures`, exit 2).

- [ ] **Step 3: Implement** `src2/remaster/rm_dump.c` from SR1's (`src/remaster/rm_dump.c`) with SR2's records (28 bytes; tokens at `+6`, not SR1's `+7`; offset `+16h`, size `+1Ah`), three libraries and no shapes; `save_indexed` copied unchanged, so a shared picture's BMP is byte-identical to SR1's; scratch segments `0x8000` (packed) and `0x9000` (planes), above SR2's `HEAP_BOTTOM` (5E08h) and unused before the game starts (the largest SR2 picture is 320 x 200: 32000 plane bytes). `src2/main.c`: parse `--dump-pictures DIR` as SR1's `src/main.c:88,113-118` does.

- [ ] **Step 4: Run the tests**

Run: `cmake --build $B && ctest --test-dir $B -LE slow --output-on-failure`
Expected: `100% tests passed`, `sr2_dump` among them.

- [ ] **Step 5: Commit and check SR1's code**

```bash
git add -A CMakeLists.txt src2 tests ENGINE2.md
git commit -m "Dump Street Rod 2's pictures for the remaster"
tools/sr1-codecheck.sh 50331e9 HEAD
```
Expected: last line ends `0 differ (allowed: )`.

### Task 3: Tags through Street Rod 2's VGA model and bitmap helpers

**Files:**
- Modify (each change marked `ENH:`, ported line for line from Street Rod 1, whose functions are the same transcriptions):

| Street Rod 2 | as Street Rod 1 |
|---|---|
| `src2/platform/vga.c`: `vga_rd` (120), `vga_wr` (135, after `ega_mark_written`), `vmovedata` (177), `#include "remaster/rm_tags.h"` | `src/platform/vga.c:8,124,175-180,187,194` |
| `src2/platform/vga.h`: `vrd` / `vwr` RAM hooks (66-71) | `src/platform/vga.h:66-82` |
| `src2/platform/blit.c`: `ega_draw_cursor` (20; the bank's plane stride stays `0x480`), `rows_composite` (146), `rows_mask_or` (171), `rows_mask_blend` (197) | `src/platform/blit.c:13,30,38,41,56,159-182,189-211,228-260` |
| `src2/platform/drv_ega.c`: `ega_sprite_over_bg` (393: `bo0`, `rm_tail_holes` after `ega_latch_copy_to40`), `ega_save_rect` (474), `ega_blit_shifted` (553: suspend around SR2's `shift_row` loop and `ega_put_shifted_row`, then the per-pixel copy) | `src/platform/drv_ega.c:17,402,416,457,462,560-567` |
| `src2/platform/gfx.c`: `bitmap_make_shifted` (520), `bitmap_composite_behind` (640) | `src/platform/gfx.c:15,512-514,532-544,563,668-673` |
| `src2/game/garage_draw.c`: `ega_vram_cache_strips` fill (61-63): `rm_holes` | `src/game/garage_draw.c:20,71` |
| `src2/platform/lib.c` `pic_decode` (222): after the unpack, when `data` is not null, `rm_seed_bitmap(data, w, h, stride, pic_index(id) + 1)` | `src/platform/lib.c:255-256` |

- Not changed: `pic_shrink_planar` (`src2/game/race_gfx.c:580`) and `pic_rotate_planar` (`src2/game/race_aq.c:789`). They write planes with `wr8` into bitmaps that `bitmap_clear` (`src2/platform/gfx.c:449`, `far_memset` through `vwr`) has just cleared (in `pic_alloc_shrunk`, `race_gfx.c:563`, and `rot_alloc`, `race_aq.c:742`, also on its `DS_rot_into_scratch` path), so their output has no tags and shows the original, as SR1's shrunk sprites with the enhanced view off. SR1 needed an explicit clear in its shrink (`src/game/race_road.c:534`) because its destination was not cleared.
- Test: `tests/test_sr2_tags.c` (ctest `sr2_tags`, in the `SRE_TEST_GAME2_DIR` block, links `sr2_core sre_host`, argument `${SRE_TEST_GAME2_DIR}`: it loads SR2's `SR.EXE` with `mem_load_exe` so `pic_decode` in Task 4 finds its DGROUP)

**Interfaces:**
- Consumes: `common/remaster/rm_tags.h` (all of it), SR2's `video.h` / `platform.h` (`bitmap_init`, `bitmap_clear`, `bitmap_make_shifted`, `bitmap_composite_behind`, `ega_draw_cursor`, `ega_blit_shifted`, `ega_save_rect`, `ega_sprite_over_bg`, `Tail`, `rows_mask_or`).
- Produces: nothing new; the rule is SR1's (rm_tags.h header comment).

- [ ] **Step 1: Write the failing tests** in `tests/test_sr2_tags.c` (`RUN` macro and `CHECK` as `tests/test_sr2_enh.c:17-25`; `setup()` clears segments 8000h-9FFFh, the planes and the tags, sets `DS_g_nplanes` 4, `DS_g_px_shift` 3, `DS_g_px_factor` 1 and `DS_g_lmask` 00 80 C0 E0 F0 F8 FC FE FF, and never clears the program image; `main` loads `<dir>/SR.EXE`, runs `sr2_off_changes_nothing` first with the tags off, then `rm_tags_enable()` and the rest):
  - `sr2_off_changes_nothing`: SR1's `run_sequence` (`tests/test_tags.c:181`) twice, tags off then on: the planes are equal.
  - `sr2_seed_copy_latch_text`: SR1's `seed_then_ram_to_vram_copy`, `latch_copy_moves_tags`, `text_write_clears` (`tests/test_tags.c:50-89`) with SR2's engine, same assertions.
  - `sr2_masked_or_sets_under`: SR1's `masked_or_sets_under` (`tests/test_tags.c:106`), same assertions.
  - `sr2_make_shifted_moves_tags`: SR1's `shift_moves_tags` (`tests/test_tags.c:129`), same assertions.
  - `sr2_masked_blend_sets_under`: the same through `ega_blit_masked` mode 0 (`rows_mask_blend`), same assertions.
  - `sr2_cursor_moves_tags` (the tags use plane 0 of the bank only, so this does not pin the 0x480 stride; the pixels already do): a 24 x 384 pointer bank at 8400:0 (stride 3, planes 0x480 apart) seeded as slot 2, its mask byte at 8400:2000h = F0h (the pointer's bit mask: the left 4 pixels drawn), VRAM byte 0 seeded with slot 5 (`rm_set_at`); `ega_draw_cursor(far_make(0x8400,0), far_make(0x8400,0x2000), far_make(0x8600,0), far_make(0xA000,0), 0, 0, 1, 1)`:
    `rm_vtag(0, 7) == rm_tag(2, 0, 0)`, `rm_vunder(0, 7) == rm_tag(5, 0, 0)`, `rm_vtag(0, 3) == rm_tag(5, 4, 0)`, `rm_mtag(lin(0x8600, 0), 7) == rm_tag(5, 0, 0)`.
  - `sr2_blit_shifted_moves_tags`: a 16 x 1 RAM bitmap seeded as slot 9, `ega_blit_shifted` to a 320 x 200 descriptor over A000:0 with `r = {w 16, h 1, sx 0, sy 0, dx 3, dy 0}`: `rm_vtag(0, 7 - 3) == rm_tag(9, 0, 0)`, `rm_vtag(1, 7 - 3) == rm_tag(9, 8, 0)`, `rm_vtag(0, 7) == 0` (setup cleared the tags); and with `w 8, sx 5, dx 1` (a left shift; `w 8` keeps the source inside the 16-pixel row): `rm_vtag(0, 6) == rm_tag(9, 5, 0)`.
  - `sr2_save_rect_to_ram`: VRAM bytes 0..1 seeded as slot 5, `ega_save_rect(far_make(0xA000,0), 2, 1, far_make(0x8800,0), 0x28)`: `rm_mtag(lin(0x8800,1), 0) == rm_tag(5, 15, 0)`.
  - `sr2_tail_holes`: SR1's `tail_holes_take_background` (`tests/test_tags.c:165`) through `ega_sprite_over_bg`: a 16 x 2 masked sprite (slot 7) over a 16 x 2 background bitmap (slot 5), destination a screen descriptor (type FEh), a `Tail` of 1 row whose source bytes at VRAM 1000h are `rm_holes`: after the call the destination's tail-row pixels that came from holes carry the background's tags (`rm_vtag == rm_tag(5, x, 1)`).
  - `sr2_composite_behind_shows_behind`: an 8 x 1 masked RAM bitmap s seeded as slot 7 with mask byte 00h (all opaque), an 8 x 1 masked bitmap d seeded as slot 5 with mask byte F0h (its left 4 pixels transparent), `bitmap_composite_behind(s, d, 0, 0)`: in d's plane 0 `rm_mtag(.., 7) == rm_tag(7, 0, 0)` (s shows where d was clear) and `rm_mtag(.., 3) == rm_tag(5, 4, 0)` (d's own pixel kept; an unhooked `vwrp` would clear it).
  - `sr2_bitmap_clear_clears_tags`: a 16 x 2 RAM bitmap seeded as slot 5, `bitmap_clear` on its descriptor: every plane pixel `rm_mtag == 0`.

- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build $B && ctest --test-dir $B -R sr2_tags --output-on-failure`
Expected: FAIL lines for every test except `sr2_off_changes_nothing` (no hooks yet: copies move no tags and `bitmap_clear` clears none).

- [ ] **Step 3: Implement** the table above.

- [ ] **Step 4: Run the tests and compare the hooks with SR1's**

Run: `cmake --build $B && ctest --test-dir $B -LE slow --output-on-failure`
Expected: `100% tests passed`.
Run: `cd $W && for f in vga.c vga.h blit.c gfx.c drv_ega.c; do diff <(grep -oE '(^|[^a-z_])rm_[a-z_]*' src/platform/$f | sed 's/^[^r]*//') <(grep -oE '(^|[^a-z_])rm_[a-z_]*' src2/platform/$f | sed 's/^[^r]*//') >/dev/null || echo "differs: $f"; done; grep -c rm_holes src2/game/garage_draw.c` (the leading non-identifier character keeps words like `platform_exit` out)
Expected: no `differs:` line; `1`.

- [ ] **Step 5: Commit and check SR1's code**

```bash
git add src2 tests
git commit -m "Track Street Rod 2's picture pixels for the remaster"
tools/sr1-codecheck.sh 50331e9 HEAD
```
Expected: last line ends `0 differ (allowed: )`.

### Task 4: Street Rod 2 shows pictures from an art pack

**Files:**
- Create: `src2/remaster/rm_lib.c`, `src2/remaster/rm_lib.h`
- Modify: `src2/platform/lib.c` `pic_decode` (the hash before `src.off++` and the unpack, the registration after the seed), `src2/platform/ega.c` `ega_compose` (the `rm_compose` call before `enh_compose`, as `src/platform/ega.c:7,52-55`), `src2/main.c` (`--remaster-dir DIR`; `rm_pack_open` before `enh_init`; with a pack, `--res-scale` defaults to 4 even with `--classic`; `atexit(rm_pack_report)` before `game_main`; as `src/main.c:9,89,122-123,131`), `common/remaster/rm_pack.h` (comment of `rm_slot_register_raw`: also Street Rod 2's library pictures, whose packed bytes are gone after the unpack; comment only), `ENGINE2.md` (a remaster bullet: hooks, `rm_lib.c`, options, developer aids)
- Test: `tests/test_sr2_tags.c` (two more tests), `tests/test_sr2_identity.sh`, `tests/test_sr2_marker.sh` (ctest `sr2_identity` and `sr2_marker`, `SRE_TEST_GAME2_DIR` block, arguments `<exe> <SR2 dir> ${CMAKE_SOURCE_DIR}`, `LABELS slow`, `TIMEOUT 900`)

**Interfaces:**
- Consumes: `rm_pack_active`, `rm_slot_register_raw(int slot, const char sha1[41], const u8 *orig, int w, int h)`, `rm_pic`, `rm_compose`, `rm_pack_open`, `rm_pack_report` (`common/remaster/rm_pack.h`); `rm_sha1_hex` (Task 2).
- Produces (`src2/remaster/rm_lib.h`), all no-ops unless `rm_pack_active()`, slot 1..`RM_MAX_SLOTS - 1`:
  - `void rm2_pic_hash(int slot, FarPtr packed, u16 psize, u8 plane);` the SHA-1 of the `psize` packed bytes from the leading BCh, once per slot, read directly: `ega_plane(plane & 3)[vram_off(seg, off + i)]` when `plane != 0xFF` (parked in VRAM), else `rd8`. Never `vrd`/`vga_rd` (no latch or tag side effects).
  - `void rm2_pic_register(int slot, FarPtr planes, int w, int h, int stride);` once per slot after the unpack: the w x h original indices read with `rd8` from the 4 planes (plane k at `k * stride * h`), then `rm_slot_register_raw(slot, sha1, orig, w, h)`. Skipped when the slot was never hashed or w, h are outside 1..640 x 1..400.
  - `const char *rm2_slot_sha1(int slot);` the hash taken, NULL if none (tests).
- `pic_decode` order (SR2 `src2/platform/lib.c:222-262`): `int slot = pic_index(id) + 1;` then, right after `FarPtr src = far_rd(e, PE_PACKED)`, `rm2_pic_hash(slot, src, size, rd8(e, PE_PLANE))`; after the unpack, when `data` is not null, `rm_seed_bitmap(...)` (Task 3) and `rm2_pic_register(slot, data, w, h, stride)`. Slots: LIB1 1..4, LIB2 5..216, LIB3 217..299, all below `RM_SHAPE_SLOT0` (400).
- Produces: `streetrod2-enhanced --remaster-dir DIR` (a folder without `pack.json`: `remaster: cannot read DIR/pack.json` on stderr and the game runs as without it), and with it the developer aids of `rm_pack.h`: `SR_REMASTER_SELFTEST`, `SR_REMASTER_PALETTE_LOG`, `SR_REMASTER_TAGMAP` (+ `SR_REMASTER_TAGMAP_N`).

- [ ] **Step 1: Write the failing unit tests** in `tests/test_sr2_tags.c` (they open, once and after the Task 3 tests, a pack written by the test into a temp folder in the working directory, as `tests/test_compose.c:34` does: `pack.json` with one picture entry, the SHA-1 of the decode test's packed bytes, and its 32 x 4 `.png` and `.idx.png`; every hook of `rm_lib.c` is a no-op until `rm_pack_open` succeeds):
  - `sr2_hash_parked_keeps_latches`: 5 packed bytes `BC 11 22 33 44` in plane 2 at VRAM 7D00h; planes 0..3 at 100h hold 4 other distinct non-zero bytes (so a latch load from 7D00h..7D04h would show); `vga_rd(0x100)` loads the latches; `rm2_pic_hash(30, far_make(0xA7D0, 0), 5, 2)`; then a write-mode-1 `vga_wr(0x200, 0)`:
    `strcmp(rm2_slot_sha1(30), <rm_sha1_hex of those 5 bytes>) == 0`, and planes 0..3 at 200h equal planes 0..3 at 100h.
  - `sr2_decode_hash_in_place`: a fake directory at `DS_picdir_seg` with entry 0 (id 1): w 8, h 1, masked 0, tokens all 0, `PE_SIZE` 5, `PE_PLANE` FFh, and `PE_PACKED` pointing at the tail of an 8 x 1 destination bitmap exactly where `pic_decode` would load it (`data + desc_size - size + 1` = `data + 0` here, so the packed bytes overlap the planes they decode into), filled with `BC` and 4 literal bytes, one per plane (none 00h or FFh), that decode to a known 8-pixel row; `DS_driver_id` = -2; `DSB(DS_cursor_visible)` = 0 (so `cursor_ctl(-4)` / `(-2)` return at once, no driver vectors needed). `pic_decode(1, desc)`:
    `strcmp(rm2_slot_sha1(1), want) == 0`, `rm_pic(1) != NULL`, `rm_pic(1)->orig[x]` equals the known row for x = 0..7, `rm_mtag(far_lin(data), 7) == rm_tag(1, 0, 0)`.
- [ ] **Step 2: Write the failing run tests.** `tests/test_sr2_identity.sh <exe> <SR2 dir> <repo>`: dump to `$tmp/dump`, `python3 $repo/tools/remaster/rm.py identity-pack $tmp/pack $tmp/dump`, a half pack `$tmp/half` (a copy whose `pack.json` keeps only the sha1s starting with 0-7, rewritten by `python3 -I -c`), each game folder a copy of `<SR2 dir>`; the runs in parallel, each `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_REMASTER_SELFTEST=1 <exe> --game-dir <copy> --remaster-dir <pack> --run-ms <ms> ...`:
  - `demo`: `SR_SEED=11 SR_REMASTER_PALETTE_LOG=$tmp/pal.jsonl`, 150000 ms, `demo v adlib`;
  - `drag_classic`: `SR_DEBUG_RACE=drag`, `--classic`, 90000 ms, `demo v adlib`;
  - `aqueduct_classic`: `SR_DEBUG_RACE=aqueduct`, `--classic`, 90000 ms, `demo v adlib`;
  - `half`: `SR_SEED=3`, the half pack, 90000 ms, `demo v adlib`;
  - `nopack`: `--remaster-dir $tmp/none`, 5000 ms, `v adlib`.

  Checks, each `ok   NAME` / `FAIL NAME`: for the first four the last `remaster selftest:` line of stderr has frames > 100, replaced > 0, mismatched 0, and stderr has no `art for` line; `nopack` exits 0 and its stderr has `remaster: cannot read`; the palette log has at least 40 distinct sha1s and every one is in `$tmp/dump/pictures.jsonl`.

  `tests/test_sr2_marker.sh <exe> <SR2 dir> <repo>`: `rm.py marker-pack`, one run `SR_SEED=11 SR_REMASTER_SELFTEST=1 SR_REMASTER_TAGMAP=$tmp/map SR_REMASTER_TAGMAP_N=100 --res-scale 1 --classic --run-ms 180000 demo v adlib` (dummy drivers); then `python3 -I -c` over `$tmp/map/tags*.bmp` (slot = R + 256 G): checks `ok   coverage` when the largest tagged share of one frame is at least 0.90 (a full-screen picture) and at least 20 distinct slots are seen over the run, and `ok   selftest` when mismatched > 0 (the tint is shown). These are floors: below them, look at `frame*.bmp` beside `tags*.bmp` for pictures shown untinted and fix the hook (with a test in `test_sr2_tags.c`); if the run simply reaches fewer screens (`SR_TRACE=1`, `ui_push screen` lines), lengthen it, never lower the floor.
- [ ] **Step 3: Run them to see them fail**

Run: `cmake --build $B && ctest --test-dir $B -R "sr2_tags|sr2_identity|sr2_marker" --output-on-failure`
Expected: build FAIL (`remaster/rm_lib.h` not found). With an empty `rm_lib.h`/`rm_lib.c` in place the shell tests FAIL (`--remaster-dir` is an unknown option: usage, exit 2).

- [ ] **Step 4: Implement** `rm_lib.c`, the `pic_decode`, `ega_compose` and `main.c` changes.

- [ ] **Step 5: Run the tests**

Run: `cmake --build $B && ctest --test-dir $B --output-on-failure` (all, slow included)
Expected: `100% tests passed`. Note the selftest lines of `sr2_identity` and the coverage numbers of `sr2_marker` (`ctest -V -R ...`) in the ledger, and look at a few `frame*.bmp` of the marker run (garage, newspaper, title, a race): tint on every picture, none on printed text, lines and the race road. Every gap found gets a hook fix and a test in `test_sr2_tags.c` before the commit.

- [ ] **Step 6: Commit and check SR1's code**

```bash
git add src2 common/remaster/rm_pack.h tests ENGINE2.md
git commit -m "Show remastered pictures in Street Rod 2"
tools/sr1-codecheck.sh 50331e9 HEAD
```
Expected: last line ends `0 differ (allowed: )`.

### Task 5: Remastered art in Street Rod 2's enhanced race view

**Ruling (builder's call, as the task allowed):** Street Rod 1's enhanced view draws road-side pictures and opponents from their source bitmaps; Street Rod 2's captures what the game drew into the page (the sky and horizon sheets on page 1, and per row the object layer's pixels, `capture_objects`). The art fits that approach wherever the captured pixels carry verified tags: the sheets (exactly as SR1's `read_sheets`/`sheet_row`) and the cut objects drawn at their picture's own size (location signs, billboards, the start props, the nearest opponent and police views). Pixels from shrunk or rotated bitmaps carry no tags (Task 3) and stay original, as in SR1 with the enhanced view off; drawing them from their source pictures belongs to the race view's parts B/C.

**Files:**
- Modify: `src2/enhanced/enh_road2.c` (`src_tags` beside `src_rows`, filled and verified in `read_sheets`; `sheet_row` samples the art as `src/enhanced/enh_road.c:121-165`, its signature unchanged), `src2/enhanced/enhanced.c` (`capture_objects`: `c->img.rm` = the verified VRAM tags of the cut pixels, freed and rebuilt each capture, NULL when none; `compare_frame`: `enh_cur_rgb` NULL while it draws, restored after, so the compare tool sees palette indices only), `ENHANCED.md` section 9 (the ruling)
- Test: `tests/test_sr2_identity.sh` and `tests/test_sr2_marker.sh` (more runs), `tests/test_sr2_enh_compare.sh` (a `drag_art` run; its CMake line passes `${CMAKE_SOURCE_DIR}` as a third argument)

**Interfaces:**
- Consumes: `rm_vtag`, `rm_tag_slot/u/v`, `rm_pic`, `rm_texel`, `rm_selftest`, `rm_selftest_race`, `RM_ART_SCALE`; `EnhImage.rm`, `enh_buf.rgb`, `enh_cur_rgb`, `ENH_RGB` (`common/enhanced/enh_draw.h`).
- Verification rule for a captured pixel (as `attach_art`, `common/enhanced/enh_sprite.c:46`): keep tag t only when `rm_pic(slot)` exists, `u < w`, `v < h` and `orig[v * w + u]` equals the captured colour.

- [ ] **Step 1: Write the failing runs.** `test_sr2_identity.sh` gets `drag_enh` (`SR_DEBUG_RACE=drag`, enhanced, 90000 ms) and `cruise_enh` (`SR_DEBUG_RACE=cruise`, 90000 ms), both also requiring `race` > 0 in the selftest line; `test_sr2_marker.sh` gets `drag_marker` (`SR_DEBUG_RACE=drag`, enhanced, `SR_REMASTER_SELFTEST=1`, 90000 ms): `race` > 0 and mismatched > 0. `test_sr2_enh_compare.sh` gets `drag_art`: the same drag run (its `run` function, `SR_KEYS=1:03 ... demo v`) with `--remaster-dir` an identity pack built from a dump (as in `test_sr2_identity.sh`) and `SR_ENH_COMPARE_DUMP=$tmp/art_dump`, checked with the `drag` thresholds (frames >= 20, worst edge <= 1, median mismatch <= 10 permille, mirror coverage 0, shown >= frames) and `ok   drag_art indices` when at least one `cmp*.bmp` was written and no pixel of their right halves is FF00FFh (`python3 -I -c` with `PIL.Image`; the compare dump paints a non-palette sample magenta, and `ega_palette` has no FF00FFh).
- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build $B && ctest --test-dir $B -R "sr2_identity|sr2_marker|sr2_enh_compare" --output-on-failure`
Expected: FAIL on `drag_enh`, `cruise_enh`, `drag_marker` (`race 0`). `drag_art` passes already (no art samples yet): it guards Step 3's `compare_frame` change, and fails if `enh_cur_rgb` is left set while it draws.

- [ ] **Step 3: Implement** the two files.

- [ ] **Step 4: Run all tests**

Run: `cmake --build $B && ctest --test-dir $B --output-on-failure`
Expected: `100% tests passed`. Look at two snapshots of a marker drag with the enhanced view (`SR_SNAPSHOT_DIR`, `SR_SNAPSHOT_MS=4000`, dummy drivers): the sky, the horizon and a near sign tinted, the road untinted; keep them out of the repo.

- [ ] **Step 5: Commit and check SR1's code**

```bash
git add src2/enhanced tests ENHANCED.md
git commit -m "Use the remastered art in Street Rod 2's race view"
tools/sr1-codecheck.sh 50331e9 HEAD
```
Expected: last line ends `0 differ (allowed: )`.

### Task 6: The launcher's Art choice for Street Rod 2

Only after Task 4: before it, Street Rod 2 stops at `--remaster-dir` with its usage.

**Files:**
- Modify: `launcher/gameargs.cpp:9-10` (SR2's `art` true), `launcher/launcher.cpp:380-381` (the note), `launcher/README.md` (Picture: Art for both games, one pack), `tests/test_args.cpp`

**Interfaces:**
- Consumes: `GameTraits::art`, `GameArgs` (unchanged code paths: `--remaster-dir` goes right after `--scale` / `--fullscreen`, before the race-view options; the settings key is `Art` in SR2's section `Game2`, default Remastered when a pack is found, `launcher.cpp:655,677`).
- Note text (exact): `"Street Rod 2: pictures not redrawn yet show the original; the race view of the\nGrudge drag strip, Mulholland and the Aqueduct is original for now."`

- [ ] **Step 1: Change the tests** in `tests/test_args.cpp`: `traits_table` checks `b.art`; `args_sr2_minimal` wants `{"/p/streetrod2-enhanced", "--game-dir", "/g2", "--scale", "3", "--remaster-dir", "/art", "--res-scale", "4", "--aa", "2", "--town", "on", "--music", "remastered", "v", "adlib"}`; `args_sr2_classic` checks `has(a, "--classic") && !has(a, "--res-scale") && followed_by(a, "--remaster-dir", "/art")`; new `args_sr2_no_pack_no_dir` (empty `remasterDir`: no `--remaster-dir`), added to `main`.
- [ ] **Step 2: Run to see them fail**

Run: `cmake --build $B && ctest --test-dir $B -R args --output-on-failure`
Expected: `FAIL traits_table`, `FAIL args_sr2_minimal`, `FAIL args_sr2_classic`; `args_sr2_no_pack_no_dir` passes already (a guard for the empty folder once the trait is on).

- [ ] **Step 3: Implement** the trait, the note and the README.

- [ ] **Step 4: Run the tests**

Run: `cmake --build $B && ctest --test-dir $B -LE slow --output-on-failure`
Expected: `100% tests passed`. Start the launcher from `$B` with Street Rod 2 chosen and a pack installed: Art is enabled and the note shows the new text (one screenshot by window ID for the ledger, not committed).

- [ ] **Step 5: Commit and check SR1's code**

```bash
git add launcher tests
git commit -m "Offer the remastered art for Street Rod 2"
tools/sr1-codecheck.sh 50331e9 HEAD
```
Expected: last line ends `0 differ (allowed: )`.

### Task 7: The pipeline takes Street Rod 2 as a third source

**Files:**
- Modify: `tools/remaster/rm.py` (`ART` from the environment `SRE_REMASTER_ART` when set, else `<repo>/remaster-art`: a worktree has no `remaster-art` of its own; `DUMPS` gets `dump/sr2`; `catalog` prints the overlap per game; `cmd_palettes` uses `palettes.fill_like`), `tools/remaster/rmlib/catalog.py`, `tools/remaster/rmlib/palettes.py`, `tools/remaster/README.md` (step 1: the SR2 dump command; step 2: `sr2=`; step 4: SR2's palette runs; the environment variable)
- Test: `tools/remaster/tests/test_catalog.py`, `tools/remaster/tests/test_palettes.py`, `tools/remaster/tests/test_rmpy.py` (new)

**Interfaces:**
- `catalog.merge(path, dumps)` (unchanged signature): first every entry loses the ids (and the `games` item) of each game named in `dumps`, then the dumps add theirs back; the other games' ids stay. Today's code resets `ids`/`games` of every entry it sees, so `merge(sr2)` alone would wipe the `main` ids of the 48 shared entries. Human fields (`group`, `batch`, `desc`, `keep`, `sheet`, `status`, `pal_like` and any other) are kept.
- `catalog.overlap(cat: dict, game: str) -> tuple[int, int, int]`: (entries with `game` in `games`, those also in another game, those only in `game`).
- `rm.py catalog GAME=DIR ...` prints `N pictures`, then per game named, in order: `GAME: T pictures, S also in other games, O only in GAME`.
- `palettes.fill_like(out: dict, cat: dict) -> None`: for an entry with `pal_like` = id and no palette, the palette of the entry with that id in the same game (`'main'` when the entry has main ids, else its first game).

- [ ] **Step 1: Write the failing tests**
  - `test_catalog_merge_third_source`: main has `c`*40 (id 1003); sr2 has `c`*40 (id 1050) and `e`*40 (id 2000). After `merge(main, sr2)`: `cat['c'*40]['ids'] == {'main': [1003], 'sr2': [1050]}`, `cat['c'*40]['games'] == ['main', 'sr2']`, `cat['e'*40]['games'] == ['sr2']`, human fields set before are kept.
  - `test_catalog_merge_one_game_keeps_the_others`: after `merge(main, datadisk)`, a second `merge(sr2)` alone leaves `ids['main']` and `ids['datadisk']` as they were and adds `ids['sr2']`.
  - `test_catalog_overlap_counts`: for the catalogue of the first test, `overlap(cat, 'sr2') == (2, 1, 1)`.
  - `test_pal_like_in_another_game` (test_palettes.py): an sr2-only entry `x` with `pal_like` 2001 and an sr2 entry `y` with id 2001 and a palette: after `fill_like`, `out['x'] == out['y']`.
  - `test_art_folder_from_env` (test_rmpy.py): with `SRE_REMASTER_ART` set to `tmp_path` (`monkeypatch.setenv`), `importlib.reload` of `rm` gives `rm.ART == str(tmp_path)` and `rm.DUMPS[2] == str(tmp_path / 'dump' / 'sr2')`; afterwards `monkeypatch.delenv` and reload again, so other tests see the default.
- [ ] **Step 2: Run to see them fail**

Run: `cd $W && python3 -m pytest tools/remaster/tests -q`
Expected: 4 failed, 27 passed. `test_catalog_merge_third_source` passes already (one call with both games works today); it guards the new merge.

- [ ] **Step 3: Implement.**

- [ ] **Step 4: Run the tests**

Run: `cd $W && python3 -m pytest tools/remaster/tests -q`
Expected: `31 passed`.

- [ ] **Step 5: Commit**

```bash
git add tools/remaster
git commit -m "Take Street Rod 2 into the remaster catalogue"
```

### Task 8: Dump, catalogue, palette logs, docs

**Files:**
- Modify: `tools/remaster/catalog.json` (the merge), `REMASTER.md` (a Street Rod 2 section: the engine core shared from `common/remaster`, the in-place decode, the race-view ruling, the third source, the counts measured below), `README.md` (players: Street Rod 2's pictures it shares with Street Rod 1 are shown remastered, the rest original until redrawn; the folder list: `common/remaster/`), `CHANGELOG.md` (Unreleased: one bullet with `--remaster-dir`, the launcher's Art for Street Rod 2, the counts), `common/README.md` if Task 1's paragraph needs the SR2 side
- Not committed: `$ART/dump/sr2/`, `$ART/identity-sr2/`, `$ART/collect-sr2.sh`, `$ART/palette-log-sr2-*.jsonl`, `$ART/palettes.json`, `$ART/palettes.before-sr2.json`

- [ ] **Step 1: Dump.** Run: `mkdir -p $ART/dump/sr2 && $B/streetrod2-enhanced --game-dir $G2 --dump-pictures $ART/dump/sr2`
Expected: `299 pictures written to .../dump/sr2`.

- [ ] **Step 2: Merge the catalogue.** Run: `cd $W && SRE_REMASTER_ART=$ART python3 tools/remaster/rm.py catalog main=$ART/dump/main datadisk=$ART/dump/datadisk sr2=$ART/dump/sr2`
Expected (counted from the library files when this plan was written; the engine's dump must agree): `652 pictures`, then `main: ...`, `datadisk: ...`, and `sr2: 297 pictures, 48 also in other games, 249 only in sr2`. Check with `git diff --stat tools/remaster/catalog.json` and a `python3 -I -c` comparison against `git show HEAD:tools/remaster/catalog.json` that every one of the 403 old entries is unchanged except for an added `sr2` in `ids`/`games` (48 of them; their `w`, `h` and `masked` agree with SR1's, checked at review time). Then, for each of the 48, `cmp $ART/dump/sr2/<sha1>.bmp $ART/dump/main/<sha1>.bmp` (and the `.mask.bmp`): all identical (Review Focus 6; all 48 are in SR1's main game). A difference means the shared key would show SR1's art over different pixels: stop and report it.

- [ ] **Step 3: Count what already has art.** Run: `python3 -I -c` reading the catalogue and `$ART/pack/pack.json`: the sr2 entries whose sha1 is in the pack's `pictures`.
Expected: `40` (the other 8 shared pictures have no art in the pack). Write the numbers (48 shared, 40 with art now, 249 new) in the ledger, `REMASTER.md` and `CHANGELOG.md`.

- [ ] **Step 4: Palette logs.** `rm.py identity-pack $ART/identity-sr2 $ART/dump/sr2`, then write `$ART/collect-sr2.sh` (as `$ART/collect.sh` for SR1): for each seed 1 2 4 5 6 7 11 a demo run (name = the seed), and for each of `drag cruise grudge mulholland aqueduct bob` an `SR_DEBUG_RACE` run with `SR_SEED=1` (name = the race), each on its own copy of `$G2`, in parallel, `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_SEED=<seed> SR_REMASTER_PALETTE_LOG=$ART/palette-log-sr2-<name>.jsonl $B/streetrod2-enhanced --game-dir <copy> --remaster-dir $ART/identity-sr2 --run-ms 300000 demo v adlib`. A debug race run exits when its race is over (race_debug.c), so 300 s is an upper bound there. Run it.
Expected: 13 non-empty `palette-log-sr2-*.jsonl`.

- [ ] **Step 5: Merge the palettes.** Run: `cp $ART/palettes.json $ART/palettes.before-sr2.json && cd $W && SRE_REMASTER_ART=$ART python3 tools/remaster/rm.py palettes $ART/palette-log-*.jsonl`
Expected: `N pictures with a palette`, N larger than before by the SR2 pictures seen. A `python3 -I -c` comparison with `palettes.before-sr2.json` shows every SR1 entry unchanged; whether `default` changed goes in the ledger. List how many of the 249 new pictures have a palette and how many fall back to `default`.

- [ ] **Step 6: Docs.** Write the files listed above.

- [ ] **Step 7: Everything once more**

Run: `cmake --build $B && ctest --test-dir $B --output-on-failure && cd $W && python3 -m pytest tools/remaster/tests -q && python3 -I -c "import sys; print([f for f in sys.argv[1:] if any(0x2010 <= ord(c) <= 0x2015 for c in open(f).read())])" REMASTER.md README.md CHANGELOG.md ENGINE2.md ENHANCED.md launcher/README.md tools/remaster/README.md common/README.md docs/superpowers/plans/2026-11-12-sr2-remaster-engine.md`
Expected: `100% tests passed`, `31 passed`, and `[]` (no Unicode dashes).

- [ ] **Step 8: Commit and check SR1's code**

```bash
git add tools/remaster/catalog.json REMASTER.md README.md CHANGELOG.md common/README.md
git commit -m "Catalogue Street Rod 2's pictures"
tools/sr1-codecheck.sh 50331e9 HEAD
```
Expected: last line ends `0 differ (allowed: )`. The final report: the task list, the counts of Steps 2-5, the selftest and coverage numbers of Tasks 4-5, and the codecheck line.
