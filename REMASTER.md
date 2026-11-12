# HD remaster

Street Rod Enhanced can show every picture of the game redrawn as modern pixel art at four times the original
resolution, and play the background tune with a small synthesised band instead of the PC speaker. Both are
options; the original pictures and the original speaker tune stay one click away.

Design: `docs/superpowers/specs/2026-11-10-hd-remaster-design.md`. Pipeline details: `tools/remaster/README.md`.

## The art pack

The pack lives in its own repository,
[street-rod-remastered-art](https://github.com/007hacky007/street-rod-remastered-art), and is not part of
this repository, its builds or its downloads. A tag there publishes a release with the zip and a manifest
(`Street-Rod-Remastered-art.txt`: version, pack format, size and SHA-256 of the zip); the launcher downloads
the latest release and offers an update when a newer one is out (`launcher/artpack.cpp`). The README's
screenshots of the remastered game are kept there too. `remaster-art/`, where the art is made, is ignored by
git. The code, the tools and the catalogue (sizes, ids and descriptions, no pixels) are committed here. The pictures are redrawn from
Street Rod's art, which belongs to its copyright owner. Without a pack the game runs exactly as before.

## Using it

* Launcher: on its first start it offers to download the pack (**Download Pictures...** beside **Art** does
  it later) into `Remaster` in its settings folder (`%APPDATA%\Street Rod Enhanced` on Windows,
  `~/Library/Application Support/Street Rod Enhanced` on a Mac). A pack put by hand in `Remaster` beside the
  launcher is used when there is none in the settings folder. **Art: Remastered** is the default once a pack
  is there.
  **Race view** (Enhanced / Original) is a separate choice. **Music** in the main window's Options picks the
  remastered or the original tune (default Remastered).
* Game: `--remaster-dir DIR` shows the pack in DIR; `--music remastered` plays the band (default original).
  `streetrod2-enhanced` takes `--remaster-dir DIR` too, with the same pack.

## How the game shows the art

The pack is a folder: `pics/<sha1>.png` (RGBA, exactly 4x the original picture), `pics/<sha1>.idx.png` (the
original palette index of every texel) and `pack.json` (format version and the palette each picture was drawn
in). Pictures are keyed by the SHA-1 of their packed bytes in the game's picture library, so the main game,
the data disk and Street Rod 2 share them.

Every pixel of video memory and of the game's RAM bitmaps carries a tag: which picture it came from and where in
it. The tags travel with the pixels through every copy, blit and sprite helper; any other write clears them.
When a frame is shown, a tagged pixel is replaced by its block of remastered art only if the screen still has
the original's colour index there, so a missed tag can only fall back to the original pixel. Texels are coloured
relative to the live palette, so painting a car, fades and colour cycling recolour the remastered art too. A
picture missing from the pack is simply shown as the original.

The race view samples the remastered pictures and the 20 remastered road-side shapes when Art is Remastered.

## Street Rod 2

Street Rod 2 shows the same pack through the same engine core: the tags, the art pack and the palette-relative
colour (`rm_tags.c`, `rm_pack.c`, `rm_color.c`) live in `common/remaster/` and are built into each game's core
with that game's headers. Street Rod 2's VGA model and blit helpers carry the same tag hooks as Street Rod 1's,
and `src2/remaster/rm_dump.c` dumps its pictures (`--dump-pictures DIR`: `LIB1`, `LIB2`, `LIB3`, in Street Rod
1's dump format). Details: `ENGINE2.md`.

* **The in-place decode.** Street Rod 2 reads a picture's packed bytes into the tail of the bitmap it decodes
  into and unpacks over them, so `src2/remaster/rm_lib.c` takes the SHA-1 before the unpack (a picture parked
  in video memory is read straight from its plane, with no side effect on the latches) and registers the
  picture after it.
* **The race view.** Street Rod 2's enhanced race view does not draw road-side pictures from their source
  bitmaps as Street Rod 1's does: it captures what the game drew into the page. So the art comes through the
  tags of the captured pixels: the sky and horizon strips, and the objects drawn at their picture's own size
  (location signs, billboards, the start props, the nearest opponent and police views). Pixels of the shrunk
  and rotated race sprites carry no tags and stay the original until the race view draws them from their
  pictures (its later parts). `SR2_SHP`, the road-side line shapes, are not pictures and are a later step.
* **A third source.** The catalogue takes Street Rod 2's dump as a third game, `sr2`, beside `main` and
  `datadisk` (`rm.py catalog ... sr2=remaster-art/dump/sr2`). Its 299 pictures are 297 distinct ones; 48 of
  them are byte-identical to Street Rod 1 pictures (all in the main game, and their dumped bitmaps are
  identical too: the two unpackers differ, but not on these). The catalogue now holds 652 pictures.
* **What already has art.** 40 of the 48 shared pictures show Street Rod 1's art with no extra work (the
  other 8 were kept original in Street Rod 1: the night sky strip, the car shadow strip, the small red frame,
  two tiny pieces and three race splatters). The other 249 Street Rod 2 pictures show the original until they
  are redrawn through the same pipeline, with their palettes logged from Street Rod 2's own runs
  (`palette-log-sr2-*.jsonl`, `tools/remaster/README.md` step 4): 13 logs (seven demo runs and one run per
  race kind) saw 159 of its pictures; 132 of the 249 new ones have their palette, the other 117 take the
  `default` until a log or a `pal_like` gives them theirs.

## The music

The band plays the game's own tune from the program image at run time, note for note, driven by the game's own
sequencer: the notes are split back into a bass and an upper voice, held as an instrument would hold them, and
played by a plucked bass, a twangy guitar, an electric piano or vibraphone, with light drums on the original's
note onsets, in stereo with a short reverb. Sound effects stay PC speaker. Nothing derived from the tune is
committed or shipped.

## Rebuilding the pack or redoing a picture

Needs Python 3 with Pillow and numpy, the game files and a ChatGPT account (the art was made in the browser,
with Claude in Chrome, in a ChatGPT project).

1. Dump the pictures and build the catalogue (`tools/remaster/README.md`, steps 1 and 2). The committed
   `catalog.json` already holds the batch, sheet, description and notes of every picture.
2. Log the palettes the pictures are shown with (step 4), so each picture is drawn in its real colours.
3. Generate: `rm.py jobs N`, then for each `rm.py next`: `rm.py job NAME` writes `input.png` and
   `prompt.txt`; send both to ChatGPT, then `rm.py ingest NAME RESULT.png` and look at
   `remaster-art/review/NAME.png`.
4. Settle what the checks flag: `accept SHA1 NOTE`, `reject SHA1 NOTE` (generated again), `original SHA1 NOTE`
   (kept original), `repair NAME` (hue lock), `splice` (opaque animation frames over their scene).
5. `rm.py pack remaster-art/pack` and `rm.py install remaster-art/pack DEST`.

To redo one picture: `reject SHA1 NOTE` it (its job goes back to failed), adjust its `desc` or `keep` in the
catalogue if the drawing went wrong, and run its job again from step 3.

## What is kept original

Of Street Rod 1's 403 pictures and shapes, 388 are remastered. Kept original: the paragraph of ending text (for a later text
stage), two flat one-colour masks, the night sky strip, the car shadow strip, three tiny pieces, a small red
frame and an empty black part, the skull sticker (lettering garbled), three race splatters (not registered) and
the driver licence (ChatGPT does not draw ID documents).

Text, lines and the road surface the game draws with code (car names, dialogs, gauge needles, frames, the race
road) are not pictures and stay original; they are planned as a later stage.
