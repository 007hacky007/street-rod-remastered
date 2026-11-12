# Shared by both games

The host (window, sound, input, files, timer), the Mac key handling and `types.h`: code that
Street Rod 1 (`src/`) and Street Rod 2 (`src2/`) both build on. Nothing here includes either game's
`symbols.h`, `mem.h` or `platform/` headers.

`band/` is the remastered music's band: the synthesiser (`synth.c`: plucked bass and guitar, electric piano,
vibraphone, drums, stereo reverb) and the score reader and arranger of the PC speaker tunes (`arrange.c`). It
is not part of the host library: each game builds it into its own core, because `arrange.c` reads the tune
from the game's memory through that game's `mem.h`.

`enhanced/` is the enhanced race view's drawing core: the sample buffer, polygon fill, clipping and the
linear-light resolve (`enh_raster.c`), and the picture decoding and scaled drawing (`enh_sprite.c`), declared
in `enh_draw.h`. Like the band it is built into each game's core, because it reads pictures through that
game's `mem.h` and `platform/video.h` and the remaster through its `remaster/` headers. What each game draws
with it (the road from its own race data) stays in `src/enhanced` and `src2/enhanced`.

`remaster/` is the remastered pictures' core: the per-pixel tags (`rm_tags.c`), the art pack and the art
sampling (`rm_pack.c`) and the palette-relative colour (`rm_color.c`). Like `enhanced/` it is built into each
game's core, because it includes that game's `mem.h`, `platform/vga.h`, `platform/ega.h` and
`remaster/rm_dump.h` (for `rm_sha1_hex`). Each game's picture dump (`rm_dump.c`) stays in its own
`remaster/` folder, and so does Street Rod 2's `rm_lib.c`, which keys a picture before the game unpacks it
over its own packed bytes.
