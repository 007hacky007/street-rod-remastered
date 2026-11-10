# Remaster pipeline

Tools for the HD remaster art pack (design: `docs/superpowers/specs/2026-11-10-hd-remaster-design.md`).
The art itself lives in `remaster-art/`, which git ignores; nothing generated from the game's pictures is
committed.

Needs Python 3 with Pillow and numpy (tests: pytest).

1. Dump the original pictures of both games through the engine:

       streetrod-enhanced --game-dir Game --dump-pictures remaster-art/dump/main
       streetrod-enhanced --game-dir Game/datadisk --dump-pictures remaster-art/dump/datadisk

2. Merge them into the catalogue (`catalog.json`, committed: sizes, ids, descriptions, no pixels):

       tools/remaster/rm.py catalog main=remaster-art/dump/main datadisk=remaster-art/dump/datadisk

3. Packs for testing the engine: `identity-pack OUT DUMP...` (the originals at 4x: the game must look
   exactly as without a pack) and `marker-pack OUT DUMP...` (tinted: shows where remastered art is used).
   `install PACK DEST` copies a pack to where the game or the launcher looks for it.

Tests: `python3 -m pytest tools/remaster/tests -q`.
