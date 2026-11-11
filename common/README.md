# Shared by both games

The host (window, sound, input, files, timer), the Mac key handling and `types.h`: code that
Street Rod 1 (`src/`) and Street Rod 2 (`src2/`) both build on. Nothing here includes either game's
`symbols.h`, `mem.h` or `platform/` headers.

`band/` is the remastered music's band: the synthesiser (`synth.c`: plucked bass and guitar, electric piano,
vibraphone, drums, stereo reverb) and the score reader and arranger of the PC speaker tunes (`arrange.c`). It
is not part of the host library: each game builds it into its own core, because `arrange.c` reads the tune
from the game's memory through that game's `mem.h`.
