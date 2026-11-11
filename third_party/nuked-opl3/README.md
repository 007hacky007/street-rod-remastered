# Nuked-OPL3

**Modified for Street Rod Enhanced:** `opl3.c` and `opl3.h` add a per-channel gain (`OPL3_SetChannelGain`,
every change marked `MODIFIED (Street Rod Enhanced)`), so Street Rod 2's music and sound effects can have
separate volumes. Everything else is as upstream.

OPL3 (YMF262) emulator by Nuke.YKT: the AdLib (OPL2) chip of Street Rod 2's music and effects
(`common/host.c`). The chip starts in OPL2-compatible mode, which is all the game uses.

* Source: https://github.com/nukeykt/Nuked-OPL3, commit `765ec962e473aeb767e4cba74ffdc8f588ffbfe8`
  (fetched 2026-09-23 for td3port, copied from `TestDrive3/td3port/third_party`), files `opl3.c`,
  `opl3.h`, `LICENSE`; copied here from `kylofon/street-rod-2-sdl3` at `9a3d488`.
* Licence: GNU LGPL 2.1 or later (`LICENSE`). It is built as a separate static library
  (`CMakeLists.txt`, target `nuked_opl3`); the rest of the project is MIT. Releases ship this directory's
  `LICENSE` and a pointer to the source.
