# Remaster pipeline

Tools for the HD remaster art pack (design: `docs/superpowers/specs/2026-11-10-hd-remaster-design.md`).
The art itself lives in `remaster-art/`, which git ignores; nothing generated from the game's pictures is
committed. `rm.py` finds that folder next to the repository (`<repo>/remaster-art`), or wherever the
environment variable `SRE_REMASTER_ART` points (a git worktree has no `remaster-art` of its own).

Needs Python 3 with Pillow and numpy (tests: pytest).

1. Dump the original pictures through the engine: Street Rod 1 and its data disk, and Street Rod 2:

       streetrod-enhanced --game-dir Game --dump-pictures remaster-art/dump/main
       streetrod-enhanced --game-dir Game/datadisk --dump-pictures remaster-art/dump/datadisk
       streetrod2-enhanced --game-dir Game2 --dump-pictures remaster-art/dump/sr2

   A picture is keyed by the SHA-1 of its packed bytes, so one pack holds the art of both games, and a
   Street Rod 2 picture byte-identical to a Street Rod 1 one shows the same art.

2. Merge them into the catalogue (`catalog.json`, committed: sizes, ids, descriptions, no pixels):

       tools/remaster/rm.py catalog main=remaster-art/dump/main datadisk=remaster-art/dump/datadisk \
           sr2=remaster-art/dump/sr2

   It replaces the ids of the games named and keeps the others' (`catalog sr2=...` alone leaves the Street
   Rod 1 ids), and prints per game how many of its pictures are also in another game.

3. Packs for testing the engine: `identity-pack OUT DUMP...` (the originals at 4x: the game must look
   exactly as without a pack) and `marker-pack OUT DUMP...` (tinted: shows where remastered art is used).
   `install PACK DEST` copies a pack to where the game or the launcher looks for it.

4. Palettes: run the game with `SR_REMASTER_PALETTE_LOG=remaster-art/palette-log-N.jsonl` and an art pack
   (the identity pack will do) through the screens to be remastered, then
   `palettes remaster-art/palette-log-*.jsonl` (`remaster-art/palettes.json`: the colours each picture is
   shown with; `default` for pictures never seen). For Street Rod 2 the same with `streetrod2-enhanced`, an
   identity pack of `dump/sr2` and logs `palette-log-sr2-*.jsonl`: demo runs with a few `SR_SEED`s and one
   `SR_DEBUG_RACE` run per race (they end when the race is over), always with `demo v adlib`. A picture
   never seen takes the palette of its catalogue `pal_like` (an id in the same game).

5. Generation, batch by batch (`batch` and `sheet` set in the catalogue, with `desc` and `keep`):

       rm.py jobs 1        # one job per picture, or per sheet name for small pictures drawn together
       rm.py next          # the next job to generate (pending, or failed with tries left)
       rm.py job NAME      # remaster-art/jobs/NAME/{input.png, prompt.txt, job.json}

   Send `input.png` with `prompt.txt` to ChatGPT, download the result, then

       rm.py ingest NAME RESULT.png

   It keeps the result in `remaster-art/raw/`, registers each picture in it, cleans it up onto the 4x grid
   (`remaster-art/art/pics/`), checks it against the original (colour per tile, outlines) and writes
   `remaster-art/review/NAME.png` (original beside the art). A failed job is retried up to three times, then
   listed under `attention` by `status`. The checks cannot tell what an object is (the radio drawn as a rack
   passes them), so every review sheet is also looked at: `reject SHA1 NOTE` sends a picture back,
   `accept SHA1 NOTE` takes one the checks flagged. `retry NAME` puts a job listed under `attention` back in
   the queue for one more generation; `ingest NAME RAW --again` checks a result kept in `raw/` again
   without counting a try (after a fix to the tools, say).

   Repairs: `repair NAME` (hue lock of the tiles that failed for colour; `--all` for UI chrome that lines up
   exactly) and `splice SHA1 SCENE_SHA1 --cell W --common --margin 3` for opaque animation frames that carry
   their own copy of the scene (the attendant at Gus'): each frame takes the scene's art wherever it shows
   the scene, so no rectangle shows around the figure. Run it again after a new ingest of the frames.

6. `pack OUT` collects the art that passed (and was not rejected) with each picture's palette; `install` it
   where the launcher looks (`~/Library/Application Support/Street Rod Enhanced/Remaster` on a Mac).
   To publish it, `pack DIR` into a clone of
   [street-rod-remastered-art](https://github.com/007hacky007/street-rod-remastered-art) (it adds and
   overwrites, it never deletes: a picture taken out of the pack is removed there with `git rm`), commit, and
   push a tag `vN`: its workflow publishes the release the launcher downloads.

In the game, `SR_REMASTER_SELFTEST=2` tints every remastered pixel, and `SR_REMASTER_TAGMAP=DIR` writes
every 25th frame with a map of which picture each pixel shows (`tagsNNNN.bmp`: R + 256 G = the picture's
slot, B = the slot under it, 0 = none).

Tests: `python3 -m pytest tools/remaster/tests -q`.
