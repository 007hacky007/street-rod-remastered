# Street Rod Enhanced — SDL3

An enhanced native port of California Dreams' *Street Rod* (1989). Not an emulator. The original game files are
not included; the game is free from the copyright owner: https://www.streetrodonline.com/downloads/.

## Features

* Smooth road view in races and drives through town, at your display's refresh rate.
* High resolution (up to 2560 × 1600) with anti-aliasing.
* Separate music and sound effects volumes, and the tune remastered for a small band (bass, guitar or
  piano, drums), note for note from the original.
* The game is otherwise the original: garage, opponents, physics, police and screens.
* The Street Rod SE data disk's 25 cars.
* A launcher for cars, switches, picture and sound options.
* A Mac app for Apple Silicon that downloads the game for you.

## How to play (Windows)

1. Put your original game files in a folder named `Game` (for the data disk cars, Street Rod SE in `Game\datadisk`).
2. Place the program files next to it:

   ```text
   Street Rod Enhanced\
   ├── Game\                        <- your game files (SR.EXE, LIB1, LIB2, HOT_DATA, datadisk\ ...)
   ├── Street Rod Enhanced.exe      <- the launcher
   ├── streetrod-enhanced.exe       <- the game
   └── SDL3.dll and the other DLLs
   ```

3. Run `Street Rod Enhanced.exe`, pick your options and press **Play**.

* The folder must be writable (saved games and the hall of fame are saved in it).
* Alt+Enter: full screen.
* Launcher settings: `%APPDATA%\Street Rod Enhanced\settings.ini`.

## How to play (Mac)

1. Download `Street-Rod-Enhanced.dmg` from the [Releases page](https://github.com/007hacky007/streetrod-enhanced/releases)
   and open it.
2. Drag **Street Rod Enhanced** to **Applications**, then open it from there.
3. The app is free and open source but not signed by Apple, so macOS asks once before it opens:
   * macOS 15 or newer: macOS says it cannot be opened, click **Done**. Open **System Settings** >
     **Privacy & Security**, scroll down and click **Open Anyway**.
   * macOS 14 or older: right-click the app and choose **Open**.
4. The launcher offers to download the game from streetrodonline.com, the official site (Street Rod and the
   Street Rod SE data disk, about 1 MB). Click **Download**, or **Choose Folder...** if you have the files.
5. Press **Play**.

* Needs a Mac with Apple Silicon (M1 or later) and macOS 12 or newer.
* The game, saved games, the hall of fame and the launcher settings are in
  `~/Library/Application Support/Street Rod Enhanced`.
* Ctrl+Cmd+F, the green window button or Option+Return: full screen. Cmd+Q quits.
* Mac keyboards have no Ins or keypad: Space clicks and shifts gear. On a Mac, Ctrl turns the sound on or off
  when you let go of it.

## Requirements

* Game files: `SR.EXE` (or `SRSE.EXE`), `LIB1`, `LIB2`, `HOT_DATA`.
* One CPU core draws the road view (about 8.5 ms a frame at the default); lower the resolution or anti-aliasing
  on slow machines.
* To build: CMake 3.24+, C11, SDL 3; the launcher needs C++17 and wxWidgets 3.2.

## Build

In Git Bash or an MSYS2 MinGW64 shell:

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

* `-DSRE_LAUNCHER=OFF` builds the game only.
* The build copies the DLLs both programs need beside them.

On a Mac (Apple Silicon, Xcode command line tools, CMake, Ninja and Python 3):

```bash
tools/macos/build-deps.sh                  # SDL3 and wxWidgets, static, into build-deps/prefix
tools/macos/package.sh --version 0.3.0     # tests, then dist/Street Rod Enhanced.app
tools/macos/make-dmg.sh --version 0.3.0    # dist/Street-Rod-Enhanced.dmg
```

* `-DSRE_TESTS=ON` builds the unit tests (`ctest --test-dir build-mac`); `package.sh` turns it on.
* `tests/test_download` checks the download for real: `test_download <dir> ok` fetches the game into `<dir>`.

## Run

```bash
./build/streetrod-enhanced.exe --game-dir Game
```

| Option | Meaning |
|---|---|
| `--game-dir DIR` | Game files folder (default `Game`) |
| `--scale N` | Window size, × 320×240 (default 3) |
| `--fullscreen` | Start in full screen |
| `--res-scale N` | Resolution, × 320×200 (default 4, 2–8) |
| `--aa N` | Anti-aliasing, N×N samples (default 2, 1 = off) |
| `--motion-delay P` | Smooth view delay, % of a game frame (default 100) |
| `--town on\|off` | Enhanced drives through town (default on) |
| `--music-volume P` | Music volume, % (default 100) |
| `--sfx-volume P` | Sound effects volume, % (default 100) |
| `--music original\|remastered` | The tune on the PC speaker or played by a small band (default original; the launcher defaults to remastered) |
| `--classic` | Original picture only |
| `--remaster-dir DIR` | Remastered pictures from the art pack in DIR (see `tools/remaster/README.md`) |
| `--check` | Check that `SR.EXE` loads, then exit |
| `demo`, `auto`, `nomouse` | The original's switches: self-playing demo, races drive themselves, no mouse |

## Controls

* Mouse, or arrows / keypad to move the pointer; Space, Ins or keypad 0 to click.
* Races: Up gas, Down brake, Left / Right steer, Space / Ins shift, A / Z gear up / down.
* Esc skips the drive through town and ends the demo. M music, Ctrl sound on / off.

## Layout

* `src/` — the game (`ENGINE.md`).
* `src/enhanced/` — the enhanced road view (`ENHANCED.md`).
* `launcher/` — the launcher (`launcher/README.md`).

## License

MIT (`LICENSE`). *Street Rod* is © California Dreams / Logical Design Works; its files are not included.
