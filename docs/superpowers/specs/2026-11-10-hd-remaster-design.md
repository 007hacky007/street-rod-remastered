# HD remaster - design

## Intent

Street Rod Enhanced already plays the 1989 game natively on macOS and Windows, with a high-resolution race view.
Every other picture is still the original 320x200, 16-colour art, enlarged 4x with hard pixels. This phase adds
a second set of graphics: every picture in the game redrawn as modern pixel art, so the game looks like a careful
modern remaster while playing exactly like the original. The original graphics stay, and the launcher lets the
player choose between them.

## Decisions made by the owner

* **Style: modern pixel art**, as in the garage test (`remaster-art/spike/garage-pixel.png`): the same composition and
  colour scheme as 1989, at about 4x the detail, with richer shading, highlights on paint and chrome, textured
  walls and floors.
* **Nothing moves.** Every object keeps its position, size and outline, so hotspots and game logic are untouched.
* **Nothing is reinvented or added.** Each object stays the same kind of object (the test turned the radio on the
  garage shelf into a rack of tubes; that is a failure). No new objects, no text baked into pictures.
* **Backgrounds are drawn alone.** The mouse pointer, cars, parts, dialogs and text that the game draws on top are
  not painted into a background (the test painted the wrench pointer onto the wall; that is a failure). Each of
  those pictures is redrawn separately.
* **Scope of the first release: everything** - every picture of the main game and of the 25-car data disk.
* **Original graphics stay** and the launcher offers Original or Remastered.
* **Art is made with ChatGPT** through Claude in Chrome, in the owner's logged-in browser (no API key).
* **Review:** the owner judges finished screens in the game, in batches, and gives feedback; repairs follow from
  the feedback. Single pictures are checked by the builder, not by the owner.

## Goals

1. A remaster art pack covering all unique pictures of LIB1/LIB2 (main game and data disk).
2. Engine support that shows the pack in every screen, including the enhanced race view, with exactly the same
   game behaviour, and falls back to the original pixels wherever a picture has no remastered version.
3. A repeatable pipeline (extract, generate, clean up, check, pack) so a single picture can be redone from
   feedback without touching the rest.
4. A launcher choice between Original and Remastered graphics.

## Non-goals

* Widescreen, new animation frames, new screens, changed gameplay, a new font.
* Remastering the CGA/Tandy libraries (`LIB1C`, `LIB2C`); the port uses the VGA path only.
* Redrawing things the game draws with code rather than pictures (printed text, lines such as gauge needles, the
  road surface), except the race-view shapes (see Calls made by the builder). They keep the original pixels, which suit the pixel-art look. Revisit only
  if the owner's feedback asks for it.

## Anti-goals (failures even if it technically works)

* Anything that shifts a hotspot, changes timing or changes what the player can do.
* A remastered picture that changes what an object is, adds or drops objects, or bakes in the pointer, a car or
  text.
* Pictures that clash in style from screen to screen.
* Seams: a remastered sprite that shows a hard 4x-pixel staircase or a halo where it meets the background.
* Publishing the remastered art before the owner decides to (see Distribution).

## Inventory

Counted from the game files (VGA libraries only, unique by packed content):

* 383 unique pictures: LIB1 (9) and LIB2 (268) of the main game, plus the data disk's LIB1/LIB2 where they differ
  (mostly car pictures).
* 25 large (full screens and big panels), about 107 medium (car side views, panels, road-side objects),
  about 251 small (parts, icons, gauges, pointer shapes). 327 are masked (drawn with transparency).
* The mouse pointer is LIB1 picture id 2 (four 24x16 shapes).

## How the engine shows remastered art

Facts from the code (see ENGINE.md, and `src/platform/*`):

* The picture id is known only when a picture is decoded (`pic_decode`, `lib.c`). After that, pictures travel as
  planar RAM bitmaps and VRAM rectangles through many helpers: plain and masked blits, latch copies, page copies,
  the blinds wipe, sprite animation, slides, pre-shifted copies (pointer bank, car animation frames) and RAM
  compositing (garage cars are built from several pictures in RAM).
* `vga_wr` (`vga.c`) is the single VRAM write path; RAM bitmaps go through `vrd`/`vwr`.
* Paint colours, fades and colour cycling are done with the 16 EGA palette registers, not by changing pixels.
* `compose` (`ega.c`) builds the output frame (320S x 200S, S = `--res-scale`, default 4), then `enh_compose`
  overlays the enhanced race view.

Design:

1. **Per-pixel tags follow the picture data.** Every pixel of VRAM and of the RAM bitmap area carries a tag: which
   picture it came from and which pixel of that picture (u, v), or none. `pic_decode` seeds the tags. The VGA model
   carries them like the hardware carries data: latch loads and latch writes move tags, plain CPU copies move them,
   and every other write (text, lines, fills, set/reset, logic functions) clears them. Helpers that combine or
   shift bits on the CPU (masked compositing, pre-shifting, the shifted blit, the planar shrink) move tags per pixel
   explicitly. Masked writes also record the tag that was under the written pixel, so a sprite's remastered edge
   can blend over the remastered background below it.
2. **Verify at present time.** A tagged pixel shows remastered art only if the screen pixel still has the colour
   index the original picture has at (u, v). A missed tag clear can then never show wrong art; it only falls back
   to the original pixel.
3. **Compose.** In `compose`, after the original frame is filled and before `enh_compose`, each verified tagged
   pixel is replaced by its S x S block from the remastered picture (the pack is at 4x; other scales resample).
   Sprite edges use the art's alpha over the "under" tag's art. Everything else stays original.
4. **Palette-relative colour.** Each remastered texel stores its colour and the original palette index it belongs
   to. When that palette register has its reference colour, the texel is shown as drawn. When the register differs
   (car paint, fades to black, colour cycling, alternative tables), the texel is recoloured to the register's
   current colour, keeping its shading. So painting a car in the garage repaints the remastered car.
5. **Race view.** The enhanced renderer (`src/enhanced/`) draws road-side objects, signs, sky, horizon and the
   opponent cars from the original pictures; with Remastered on it samples the remastered pictures instead (with
   the same palette-relative colour). The cockpit and dashboard are normal pictures and go through steps 1-4.
   With the enhanced view off, the shrunk race sprites stay original.
6. **The pack.** One folder: `pics/<sha1 of the packed picture>.png` (RGBA, exactly 4x the original size) and
   `pics/<sha1>.idx.png` (8-bit original index per texel), plus `pack.json` (format version, reference palette).
   Keying by content lets the main game and the data disk share pictures and keeps the pack valid for either game.
   A missing picture simply falls back to the original. PNGs are decoded with a vendored single-file decoder.
7. **Options.** The game takes `--remaster-dir DIR`; without it, it runs as today. The launcher gets a Graphics
   choice (Original / Remastered) that is enabled only when a pack is found (default folder: the launcher's
   settings folder `Remaster/`; on Windows next to the executable).

## The art pipeline

All tools live in `tools/remaster/` and are committed. The generated art lives in `remaster-art/` (git-ignored).

1. **Extract** (C tool built from the engine's own decoder): every unique picture as an indexed PNG with the
   reference palette, its mask, and a catalogue entry (sha1, ids, library, size, masked).
2. **Catalogue** (`tools/remaster/catalog.json`, committed, no art): per picture a short description of what it
   shows, what must stay as it is, its group (screen, car, part, race, ui, pointer) and its batch. Small pictures
   of one group are assigned to sheets of 12-16.
3. **Prompts.** One fixed style guide (written from the garage test) plus a per-picture template built from the
   catalogue: keep composition, positions, sizes and colours; same kind of object; nothing added; no pointer, car
   or text; transparent background for masked pictures. The input image is the original enlarged 4x with nearest
   neighbour, on a canvas with margin in an aspect ChatGPT can produce. Sheets put each sprite in its own cell.
4. **Generate** in ChatGPT through Claude in Chrome: one picture or sheet per request, results downloaded and moved
   into `remaster-art/raw/`. ChatGPT's image limits spread this over several days; the run is resumable from a
   progress file. The owner agreed that generation may run unattended while Chrome is open, that the results are
   downloaded (about 250 PNGs of 2-3 MB through the Downloads folder) and that the chats go into the owner's ChatGPT
   history; they are grouped in one ChatGPT project, "Street Rod Remaster". The daily image budget is not known in
   advance: when ChatGPT reports a limit, the run waits and resumes. A login prompt, captcha or anything else
   needing the owner stops the run and is reported.
5. **Clean up** (Python): register the result to the original (best scale and offset by matching the outline and
   colours), crop to exactly 4x size, cut sheets into sprites, set alpha within the original outline (edges
   anti-aliased, nothing outside it), reduce noise to a pixel-art palette, compute the index map.
6. **Check automatically** before a picture enters the pack: outline overlap with the original, per-region colour
   close to the original's colour, no large areas that differ in content from the original. A picture that fails is
   regenerated (up to three tries), then listed for attention with the reason.
7. **Pack** into `remaster-art/pack/` and install it to the default folder for testing.

## Review batches

Delivered to the owner one batch at a time, each playable in the game:

1. Garage, shops, newspaper, menus, dialogs, pointer.
2. Cars and car parts (main game and data disk).
3. Race view: road-side objects, signs, sky and horizon, opponent and police cars, cockpit and dashboard.
4. Everything left.

Feedback becomes catalogue notes and regenerations of the named pictures.

## Distribution

The remastered pictures are redrawn from Street Rod's copyrighted art. Until the owner decides otherwise, the art
stays local: `remaster-art/` is git-ignored, nothing of it is committed, attached to a release or uploaded anywhere
except to ChatGPT for generation. The code, tools and catalogue are committed as usual; without a pack the game
behaves exactly as today.

## Later stage (owner's request, after this one)

The owner wants the things the game draws with code remastered too, as a separate stage after the picture
remaster: the printed text (a new font for the car names, "Bankroll", dialogs and menus), the lines (gauge
needles, clock hands, frames and boxes) and, as the builder's reading of "everything", the road surface of the
race view. This needs new drawing code in the engine, not more pictures; it is planned on its own when this
stage is done.

## Calls made by the builder (owner may overrule)

* **Lettering inside pictures** (logos, "AD 1989", the newspaper masthead, shop headers, button labels, signs,
  gauge numerals) is part of the picture and is redrawn with it; the prompt quotes the exact text. "No text baked
  in" means text the game prints on top. Lettering that ChatGPT garbles after the retries is replaced in that
  region by the original lettering cleaned up as pixel art at 4x.
* **Race-view shapes are in scope.** Trees, poles, hydrants, banner shapes, posts and town walls are line shapes in
  the program file (`DS_shape_table`), not LIB pictures. They get their own pack entries, keyed
  `shape-<sha1 of the shape data>`, made by rendering each shape large and redrawing it, and the enhanced renderer
  draws the remastered shape instead of the line shape. The road surface stays as drawn (flat spans). This is the
  last part of batch 3, so the rest of the race view can be judged first.
* **Pictures that still fail after three tries** go through repair, in this order: regenerate only the failing
  region (ChatGPT image editing), local alignment (piecewise warp of the result to the original's outlines), or a
  combination of the best parts of several tries. A picture that still fails ships as the original for now and is
  named in the batch notes to the owner, with the reason, so the owner's feedback can decide. Batches are not held
  back by single pictures.
* **Launcher:** two independent choices. The existing Graphics choice (Enhanced / Original, `--classic`) is renamed
  "Race view". The new one is "Art: Original / Remastered", enabled when a pack is found. With Remastered and the
  original race view, the game runs at the high output scale (as `--res-scale 4`) with the original race renderer;
  `--classic` alone keeps its current meaning.
* **The spike** (the garage test images and the comparison page) is kept in `remaster-art/spike/` as the style
  reference.

## Left to the builder

* The tag layout, storage and the exact rules per helper, as long as design points 1-4 hold.
* The registration and check thresholds, tuned on the first batch.
* The prompt wording beyond the decisions above, and sheet sizes.
* Whether the race-view remaster needs mip levels for far objects.
