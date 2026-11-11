# Street Rod Enhanced

*Street Rod* (California Dreams, 1989) on today's computers: buy an old car in 1963 California, fix it up in
your garage and race it for pink slips. This is the original game, rebuilt to run natively on Windows and on
Apple Silicon Macs, with a smooth high-resolution road, separate music and sound volumes, the tune played by
a small band, and the option to show every picture redrawn at four times the resolution.

The game itself is free from its copyright owner and is not included here: the Mac app downloads it for you,
and on Windows you unzip it into a folder (see below).

![The garage, remastered](docs/screenshots/garage.png)

## What you get

* **The original game.** The garage, the parts, the opponents, the races, the police and the endings are all
  the same, played from the original game files.
* **A smooth road.** Races and drives through town are drawn at your display's refresh rate, up to
  2560 × 1600, with anti-aliasing.
* **Remastered pictures** (on by default, one click to the original). Every picture of the game redrawn as modern pixel art at four times the
  resolution. Painting your car, fades and the flashing signs still work on the new art.
* **Remastered music** (on by default). The game's own tune, note for note, played by a small band (bass, guitar
  or piano, drums) in stereo instead of the PC speaker.
* **Separate music and sound effects volumes.**
* **The Street Rod SE data disk's 25 cars**, one click away.
* **A launcher** for all of it, and a Mac app that downloads the game for you.

![Original and remastered, side by side](docs/screenshots/original-vs-remastered.png)

*The same car in the original (left) and remastered (right) pictures.*

## Download and play

### Windows

1. Download `Street-Rod-Enhanced-windows.zip` from the
   [Releases page](https://github.com/007hacky007/streetrod-enhanced/releases) and unzip it somewhere you can
   write to, for example your Documents folder (not Program Files: saved games are kept next to the game).
2. Download the game from the official site, <https://www.streetrodonline.com/downloads/>:
   * **Street Rod** (`sr1pc.zip`): copy the files from its `SR1` folder into the `Game` folder of Street Rod
     Enhanced, so that `SR.EXE`, `LIB1`, `LIB2` and `HOT_DATA` are directly inside `Game`.
   * **Street Rod SE** (`srsepc.zip`, optional, for the 25 data disk cars): copy the files from its `SRSE`
     folder into a new folder `Game\datadisk`.
3. Run `Street Rod Enhanced.exe`, pick your options and press **Play**.

```text
Street Rod Enhanced\
├── Game\                        <- SR.EXE, LIB1, LIB2, HOT_DATA (and datadisk\ for Street Rod SE)
├── Remaster\                    <- the remastered pictures
├── Street Rod Enhanced.exe      <- the launcher: start here
├── streetrod-enhanced.exe       <- the game
└── the DLLs
```

### Mac

1. Download `Street-Rod-Enhanced.dmg` from the
   [Releases page](https://github.com/007hacky007/streetrod-enhanced/releases) and open it.
2. Drag **Street Rod Enhanced** to **Applications**, then open it from there.
3. The app is free and open source but not signed by Apple, so macOS asks once before it opens it:
   * macOS 15 or newer: macOS says it cannot be opened, click **Done**. Open **System Settings** >
     **Privacy & Security**, scroll down and click **Open Anyway**.
   * macOS 14 or older: right-click the app and choose **Open**.
4. The launcher offers to download the game from streetrodonline.com, the official site (Street Rod and the
   Street Rod SE data disk, about 1 MB). Click **Download**, or **Choose Folder...** if you already have the
   files.
5. Press **Play**.

Needs a Mac with Apple Silicon (M1 or later) and macOS 12 or newer. The game, your saved games and the
launcher's settings are kept in `~/Library/Application Support/Street Rod Enhanced`.

## The launcher

![The launcher](docs/screenshots/launcher.png)

* **Game files**: where the game is. The line below says whether it was found and how many saved games there
  are; **Play** stays greyed out until the game is there.
* **Start**: **Cars** picks the original cars or the Street Rod SE data disk's 25 cars. The game's own
  switches: a self-playing demo, races that drive themselves, no mouse.
* **Options**: the window size, the **Music** (remastered or the original PC speaker) and full screen.
* **Picture**: **Art** original or remastered, **Race view** enhanced or original, the resolution and the
  anti-aliasing of the road.
* **Game settings > Sounds** (in the menu): the music and sound effects volumes.

Every choice is remembered. More in [launcher/README.md](launcher/README.md).

### The remastered pictures

The redrawn pictures come with the download (the `Remaster` folder next to the launcher on Windows, inside
the app on a Mac) and are on by default. **Art: Original** brings back the game's own pictures. How the
pictures were made: [REMASTER.md](REMASTER.md).

## Screenshots

All with the remastered pictures.

| | |
|---|---|
| ![At the gas station](docs/screenshots/gas-station.png) | ![Driving through town](docs/screenshots/drive.png) |
| At Gus's gas station | Cruising through town on the smooth road |
| ![The newspaper](docs/screenshots/newspaper.png) | ![Buying a car](docs/screenshots/classifieds.png) |
| The newspaper | The classifieds: buy your first car |

## Playing

* **Mouse**: point and click; the game draws its own pointer. Arrows or the keypad move the pointer too, and
  Space, Ins or keypad 0 click.
* **Races**: Up gas, Down brake, Left / Right steer, Space or Ins shift (down when braking, else up), A / Z
  gear up / down.
* **Esc** skips the drive through town and ends the demo. **M** music on / off, **Ctrl** sound on / off.
* **Full screen**: Alt+Enter. On a Mac also Ctrl+Cmd+F, Option+Return or the green window button.
* **On a Mac**: Cmd+Q quits. Mac keyboards have no Ins or keypad: Space clicks and shifts gear. Ctrl turns
  the sound on or off when you let go of it.
* One CPU core draws the road (about 8.5 ms a frame at the default). On a slow computer, lower the
  **Resolution** or the **Anti-aliasing** in the launcher.

## For developers

### Build

Windows, in an MSYS2 UCRT64 shell with `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,sdl3,wxwidgets3.2-msw}` and
`zip`:

```bash
tools/windows/package.sh     # tests, then dist/Street-Rod-Enhanced-windows.zip
```

Or by hand:

```bash
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Mac (Apple Silicon, Xcode command line tools, CMake, Ninja and Python 3):

```bash
tools/macos/build-deps.sh                  # SDL3 and wxWidgets, static, into build-deps/prefix
tools/macos/package.sh --version 0.4.0     # tests, then dist/Street Rod Enhanced.app
tools/macos/make-dmg.sh --version 0.4.0    # dist/Street-Rod-Enhanced.dmg
```

* Needs CMake 3.24+, a C11 compiler and SDL 3; the launcher needs C++17 and wxWidgets 3.2.
* `-DSRE_LAUNCHER=OFF` builds the game only. On Windows the build copies the DLLs both programs need beside
  them.
* `-DSRE_TESTS=ON` builds the unit tests (`ctest --test-dir build`). With `-DSRE_TEST_GAME_DIR=<folder>` the
  remaster's picture tests run against the original game files too.
* `tests/test_download` checks the download for real: `test_download <dir> ok` fetches the game into `<dir>`.

### Continuous integration

* **CI** (`.github/workflows/ci.yml`), on every push to master and every pull request: builds and tests the
  Mac app and the Windows zip, and runs the remaster tools' tests. Both packages are kept as artifacts of the
  run.
* **Release** (`.github/workflows/release.yml`), on a `v*` tag: builds the disk image and the Windows zip and
  attaches them to a draft release.

### Run the game directly

```bash
./build/streetrod-enhanced --game-dir Game
```

| Option | Meaning |
|---|---|
| `--game-dir DIR` | Game files folder (default `Game`) |
| `--scale N` | Window size, × 320×240 (default 3) |
| `--fullscreen` | Start in full screen |
| `--res-scale N` | Resolution, × 320×200 (default 4, 2 to 8) |
| `--aa N` | Anti-aliasing, N×N samples (default 2, 1 = off) |
| `--motion-delay P` | Smooth view delay, % of a game frame (default 100) |
| `--town on\|off` | Enhanced drives through town (default on) |
| `--music-volume P` | Music volume, % (default 100) |
| `--sfx-volume P` | Sound effects volume, % (default 100) |
| `--music original\|remastered` | The tune on the PC speaker or played by a small band (default original; the launcher defaults to remastered) |
| `--classic` | Original picture only |
| `--remaster-dir DIR` | Remastered pictures from the art pack in DIR (see [REMASTER.md](REMASTER.md)) |
| `--check` | Check that `SR.EXE` loads, then exit |
| `demo`, `auto`, `nomouse` | The original's switches: self-playing demo, races drive themselves, no mouse |

### Layout

* `src/`: the game ([ENGINE.md](ENGINE.md)).
* `src/enhanced/`: the enhanced road view ([ENHANCED.md](ENHANCED.md)).
* `src/remaster/`, `tools/remaster/`: the remastered pictures and the tools that build the art pack
  ([REMASTER.md](REMASTER.md)).
* `launcher/`: the launcher ([launcher/README.md](launcher/README.md)).
* `tools/macos/`, `tools/windows/`: packaging.

## License

MIT ([LICENSE](LICENSE)). *Street Rod* is © California Dreams / Logical Design Works; its files are not
included.
