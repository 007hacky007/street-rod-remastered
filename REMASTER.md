# HD remaster

Street Rod Enhanced can show every picture of the game redrawn as modern pixel art at four times the original
resolution, and play the background tune with a small synthesised band instead of the PC speaker. Both are
options; the original pictures and the original speaker tune stay one click away.

Design: `docs/superpowers/specs/2026-11-10-hd-remaster-design.md`. Pipeline details: `tools/remaster/README.md`.

## The art pack

The pack is a separate download from the Releases page, `Street-Rod-Enhanced-remaster-pack.zip`; the README
says where to put it. It is not kept in the repository: `remaster-art/`, where it is made, is ignored by git,
and the only remastered images committed are the README's screenshots in `docs/screenshots/`. The code, the
tools and the catalogue (sizes, ids and descriptions, no pixels) are committed. The pictures are redrawn from
Street Rod's art, which belongs to its copyright owner. Without a pack the game runs exactly as before.

## Using it

* Launcher: **Art: Remastered** (enabled when a pack is found in the launcher's settings folder, `Remaster/`;
  on a Mac `~/Library/Application Support/Street Rod Enhanced/Remaster`, on Windows next to the launcher).
  **Race view** (Enhanced / Original) is a separate choice. **Music** in the main window's Options picks the
  remastered or the original tune (default Remastered).
* Game: `--remaster-dir DIR` shows the pack in DIR; `--music remastered` plays the band (default original).

## How the game shows the art

The pack is a folder: `pics/<sha1>.png` (RGBA, exactly 4x the original picture), `pics/<sha1>.idx.png` (the
original palette index of every texel) and `pack.json` (format version and the palette each picture was drawn
in). Pictures are keyed by the SHA-1 of their packed bytes in the game's picture library, so the main game and
the data disk share them.

Every pixel of video memory and of the game's RAM bitmaps carries a tag: which picture it came from and where in
it. The tags travel with the pixels through every copy, blit and sprite helper; any other write clears them.
When a frame is shown, a tagged pixel is replaced by its block of remastered art only if the screen still has
the original's colour index there, so a missed tag can only fall back to the original pixel. Texels are coloured
relative to the live palette, so painting a car, fades and colour cycling recolour the remastered art too. A
picture missing from the pack is simply shown as the original.

The race view samples the remastered pictures and the 20 remastered road-side shapes when Art is Remastered.

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

Of 403 pictures and shapes, 388 are remastered. Kept original: the paragraph of ending text (for a later text
stage), two flat one-colour masks, the night sky strip, the car shadow strip, three tiny pieces, a small red
frame and an empty black part, the skull sticker (lettering garbled), three race splatters (not registered) and
the driver licence (ChatGPT does not draw ID documents).

Text, lines and the road surface the game draws with code (car names, dialogs, gauge needles, frames, the race
road) are not pictures and stay original; they are planned as a later stage.
