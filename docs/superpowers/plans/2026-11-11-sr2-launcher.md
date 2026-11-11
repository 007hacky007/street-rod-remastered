# Street Rod 2 launcher and packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One launcher with a Game choice starts Street Rod or Street Rod 2, and the Mac app and the Windows zip carry both games.

**Architecture:** The launcher learns a `Game` (SR1, SR2) and a table of what each game supports (`GameTraits`); the command line, the folder check, the downloads and the window all read that table. Plan 1's `streetrod2-enhanced` takes only `--game-dir`, `--scale`, `--fullscreen`, the volumes and SR.EXE's switches, so SR2's traits say no art, no enhanced view and no remastered music until plans 3 to 5 switch them on. The packagers copy both programs and a new check fails a package that lacks one.

**Tech Stack:** C++17, wxWidgets 3.2 (wxBase for the tests), CMake/ctest, POSIX sh.

**Spec:** `docs/superpowers/specs/2026-11-11-street-rod-2-design.md` (sections Launcher; Packaging, CI, docs, release). Plan 2 of 5 (1 engine, done in 1fb3a77..ddf265a).

## Global Constraints

- Street Rod 1 does not change: same command line for the same settings, same settings keys with the same meaning, same folder and download behaviour. `tools/sr1-codecheck.sh ddf265a HEAD` reports no differing object (the launcher is not in it).
- App names stay "Street Rod Enhanced"; the settings folder and saved games do not move.
- SR2's folder: `Game2` beside the launcher on Windows; `~/Library/Application Support/Street Rod Enhanced/Game2` on a Mac. Recognised by `SR.EXE` plus `LIB3` and `SR2_DATA`; saved games counted from `HOTROD*.SAV`.
- SR2 download: `https://www.streetrodonline.com/wp-content/uploads/2017/05/sr2pc.zip`, SHA-256 `e2d503bf5e8438d85d3ebb297121b783dbbd44f62424ac8ecfd5361e4fd0eaac`, 716452 bytes, files at the top of the zip, `__MACOSX/` skipped, unpacked to `Game2.partial` first, a non-empty folder never written to.
- The launcher always passes SR2 `v` and `adlib` or `speaker`.
- Cars and the data disk are SR1 only; Sound AdLib/PC speaker is SR2 only.
- Mac app and Windows zip carry both executables and no art. The Windows zip gets an empty `Game2\`; `Read me.txt` says where `sr2pc.zip`'s files go.
- No macOS username, no real name or email, no AI attribution, no Unicode dashes in anything committed.
- Commit messages: one short imperative line, no body.

## Review Focus

1. An options set with art, the enhanced view or remastered music chosen must never put `--remaster-dir`, `--classic`, `--res-scale`, `--aa`, `--town`, `--music` or `auto` on SR2's command line: `streetrod2-enhanced` prints its usage and exits on any unknown `--` option. (Task 1, `args_sr2_minimal`.)
2. Switching the game must not write one game's folder or program into the other game's keys, and an old `settings.ini` without `[Launcher]` must open on Street Rod with every control as before. (Task 4, checked by the settings files after the run.)
3. A Street Rod folder chosen for Street Rod 2, or the other way round, says so and keeps Play greyed. (Task 2, `sr1_folder_as_sr2`, `sr2_folder_as_sr1`.)
4. The Game choice is greyed while a game download runs, so the download lands in the folder it started for. (Task 4.)
5. A zip entry such as `../x` in a top-level zip is refused just as under `SR1/`. (Task 3, `rejects_zip_slip_top_level`.)

---

### Task 1: The games and their command lines

**Files:**
- Modify: `launcher/gameargs.h`, `launcher/gameargs.cpp`
- Test: `tests/test_args.cpp`

**Interfaces:**
- Produces (in `gameargs.h`):
  - `enum class Game { SR1 = 1, SR2 = 2 };`
  - `Game GameFromSetting(int value);` 2 gives SR2, anything else SR1.
  - `struct GameTraits { const char* name; const char* program; const char* folder; const char* section; bool cars, autoDrive, soundDevice, enhanced, art, remasteredMusic, gearKeys; };`
    name "Street Rod" / "Street Rod 2"; program "streetrod-enhanced" / "streetrod2-enhanced" (no .exe); folder "Game" / "Game2"; section (settings section of the per-game keys) "Game" / "Game2".
  - `const GameTraits& Traits(Game game);`
  - `GameOptions` gains `Game game = Game::SR1;` and `bool speaker = false;` (SR2: the PC speaker instead of AdLib).
  - `GameArgs` leaves out every option the game's traits lack; for SR2 it appends `v` and `adlib` or `speaker` before `nomouse` and `demo`.

- [ ] **Step 1: Pin Street Rod 1's default command line**

Add `args_sr1_default_pinned`: a default `GameOptions` with program `/p/streetrod-enhanced` and gameDir `/g` gives exactly `{"/p/streetrod-enhanced", "--game-dir", "/g", "--scale", "3", "--res-scale", "4", "--aa", "2", "--town", "on", "--music", "remastered"}`. Run `ctest --test-dir build-mac -R '^args$' --output-on-failure`. Expected: PASS (it pins today's behaviour before the change).

- [ ] **Step 2: Write the failing tests**

- `traits_table`: every field of both rows as listed under Interfaces; SR1 has cars, autoDrive, enhanced, art, remasteredMusic and gearKeys true and soundDevice false; SR2 the opposite (soundDevice true, the other six false).
- `game_from_setting`: 2 gives SR2; 1, 0, 3 and -1 give SR1.
- `args_sr2_minimal`: game SR2, program `/p/streetrod2-enhanced`, gameDir `/g2`, remasterDir `/art`, autoDrive true, classic false, remasteredMusic true gives exactly `{"/p/streetrod2-enhanced", "--game-dir", "/g2", "--scale", "3", "v", "adlib"}`.
- `args_sr2_speaker_switches_volumes`: SR2 with speaker, demo, noMouse, fullscreen, music 40, effects 0 contains `--fullscreen`, `--music-volume` `40`, `--sfx-volume` `0`, `speaker`, `nomouse`, `demo`, and neither `adlib` nor `auto`; `v` comes before `speaker`.

Run the same ctest. Expected: build FAIL (`Game`, `Traits`, `GameOptions::game` not declared).

- [ ] **Step 3: Implement** the Interfaces above in `gameargs.h` / `gameargs.cpp`; SR1's argument order stays as it is.

- [ ] **Step 4: Run** the same ctest. Expected: PASS, all cases.

- [ ] **Step 5: Commit** `git add launcher/gameargs.h launcher/gameargs.cpp tests/test_args.cpp` and `git commit -m "Teach the launcher's command line about Street Rod 2"`.

### Task 2: Recognising each game's folder

**Files:**
- Create: `launcher/gamefolder.h`, `launcher/gamefolder.cpp` (wxBase only: `ReadGameFolder` and its file checks move here from `game.cpp`)
- Modify: `launcher/game.h`, `launcher/game.cpp`, `launcher/CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/test_gamefolder.cpp` (new ctest `gamefolder`, wxBase, like `test_args`)

**Interfaces:**
- Consumes: `Game`, `Traits` (Task 1).
- Produces:
  - `struct GameFolder { wxString missing; bool se = false; int saves = 0; wxString dataDisk; bool otherGame = false; };` (moved to `gamefolder.h`; `otherGame`: the folder holds the other game).
  - `GameFolder ReadGameFolder(const wxString& dir, Game game);`
    SR1 exactly as today (SR.EXE or SRSE.EXE, LIB1, LIB2, HOT_DATA; HOTROD1..15.SAV; datadisk); `otherGame` when SR2_DATA is there.
    SR2: the first missing of SR.EXE, LIB1, LIB2, LIB3, SR2_DATA, SR2_END, SR2_SHP, MSR.SND; saves HOTROD0..14.SAV (SR2's slots, `save_slots_scan`); no datadisk; `otherGame` when HOT_DATA is there and LIB3 isn't.
  - `wxString DefaultGameDir(Game game);` and `wxString DefaultProgram(Game game);` in `game.h`, from `Traits(game).folder` and `.program` (".exe" on Windows).

- [ ] **Step 1: Write the failing tests** (each in a fresh temp folder, files holding one byte):
  - `sr1_folder_unchanged`: SR.EXE, LIB1, LIB2, HOT_DATA, HOTROD1.SAV, HOTROD15.SAV and a complete `datadisk` (SRSE.EXE, LIB1, LIB2, HOT_DATA): missing empty, saves 2, dataDisk set, !se, !otherGame.
  - `sr2_folder_complete`: the eight SR2 files, HOTROD0.SAV, HOTROD14.SAV, HOTROD15.SAV: missing empty, saves 2, dataDisk empty, !otherGame.
  - `sr2_missing_lib3`: the eight minus LIB3: missing "LIB3".
  - `sr2_lowercase_names`: the eight in lower case: missing empty.
  - `sr2_empty_folder`: missing "SR.EXE"; and `ReadGameFolder("", Game::SR2)` too.
  - `sr1_folder_as_sr2`: the SR1 files read as SR2: otherGame, missing "LIB3".
  - `sr2_folder_as_sr1`: the SR2 files read as SR1: otherGame, missing "HOT_DATA".

Run `cmake --build build-mac && ctest --test-dir build-mac -R gamefolder --output-on-failure`. Expected: build FAIL (`gamefolder.h` missing).

- [ ] **Step 2: Implement** the Interfaces; `game.cpp` keeps `LauncherDir`, `IsDosProgram`, the art dirs and `LaunchGame`, whose DOS-program message names `Traits(o.game).program`. Callers in `launcher.cpp` pass `Game::SR1` for now so the launcher still builds.

- [ ] **Step 3: Run** the same command. Expected: PASS; `ctest --test-dir build-mac -LE slow` all pass.

- [ ] **Step 4: Commit** `git add launcher/gamefolder.h launcher/gamefolder.cpp launcher/game.h launcher/game.cpp launcher/launcher.cpp launcher/CMakeLists.txt tests/CMakeLists.txt tests/test_gamefolder.cpp` and `git commit -m "Recognise Street Rod 2's game folder in the launcher"`.

### Task 3: Downloading Street Rod 2

**Files:**
- Modify: `launcher/gamefiles.h`, `launcher/gamefiles.cpp`, `launcher/download.h`, `launcher/download.cpp`, `tests/test_download.cpp` (its call of `Start`)
- Test: `tests/test_gamefiles.cpp`

**Interfaces:**
- Consumes: `Game` (Task 1).
- Produces:
  - `const std::vector<Archive>& Official(Game game);` SR1: the two archives as today. SR2: one, `{"Street Rod 2", "sr2pc.zip", <URL>, <SHA-256>, 716452, "", ""}` with the values of Global Constraints.
  - `Archive::top` may be empty: the zip's files are at its top. `Unzip` then takes every file entry outside `__MACOSX/`, without `.DS_Store`, with the same refusal of absolute paths, drive letters and `..`.
  - `void GameDownload::Start(Game game, Progress progress, Done done);` downloads `Official(game)`.

- [ ] **Step 1: Write the failing tests**
  - `official_table`: calls `Official(Game::SR1)` (values unchanged) and checks `Official(Game::SR2)`: size 1 and every field.
  - `url_override_sr2`: with `SRE_DOWNLOAD_BASE=http://127.0.0.1:8765`, `Url(Official(Game::SR2)[0])` is `http://127.0.0.1:8765/sr2pc.zip`.
  - `installs_top_level`: a zip of `SR.EXE`, `LIB3`, `.DS_Store`, `__MACOSX/`, `__MACOSX/._SR.EXE` installed with top "" and subdir "": `game/SR.EXE` and `game/LIB3` hold their bytes; no `.DS_Store`, no `__MACOSX`, no `game.partial`.
  - `rejects_zip_slip_top_level`: a top-level zip with `../evil`: Install fails, error says "outside its folder", no `game`, no `evil` beside it.

Run `cmake --build build-mac && ctest --test-dir build-mac -R gamefiles --output-on-failure`. Expected: build FAIL (`Official` takes no `Game`).

- [ ] **Step 2: Implement** the Interfaces; `launcher.cpp` passes `Game::SR1` to `Start` for now.

- [ ] **Step 3: Run** the same command. Expected: PASS. Then by hand, `build-mac/tests/test_download` as its file describes, if it can fetch SR2, against the real site once. Expected: the SHA-256 above.

- [ ] **Step 4: Commit** `git add launcher/gamefiles.h launcher/gamefiles.cpp launcher/download.h launcher/download.cpp launcher/launcher.cpp tests/test_gamefiles.cpp tests/test_download.cpp` and `git commit -m "Let the launcher download Street Rod 2"`.

### Task 4: The Game choice in the window

**Files:**
- Modify: `launcher/launcher.h`, `launcher/launcher.cpp`, `launcher/README.md`

**Interfaces:**
- Consumes: Tasks 1 to 3.
- Settings (Builder's call under "Settings keys for SR2"):
  - `[Launcher] Game` = 1 or 2, read with `GameFromSetting`, default 1.
  - Per game, in `Traits(game).section`: `GameFolder`, `Program` (empty = the default, as today), `Demo`, `NoMouse`; SR1 also `Cars`, `Auto`, `Art`; SR2 also `Sound` (0 AdLib, 1 PC speaker). SR1's keys are today's keys in `[Game]`.
  - Shared, in `[Game]` as today: `Scale`, `Fullscreen`, `Classic`, `ResScale`, `AA`, `Town`; `[Sound]` the volumes, and `Music` for SR1 only. A key a game's traits lack is neither read nor written for it.

- [ ] **Step 1: The window.**
  - A Game row at the top of the main page: a `wxChoice` with "Street Rod (1989)" and "Street Rod 2: The Next Generation (1991)".
  - Changing it saves the current game's per-game keys, writes `[Launcher] Game`, loads the other game's keys into the same controls and calls `UpdateState`. On a Mac, the first change in a run to a game whose folder is free offers its download as at start.
  - `UpdateState` follows `Traits`:
    - Hidden when the trait is false: the Cars row and its note (`cars`), Races drive themselves (`autoDrive`), the A / Z key row (`gearKeys`).
    - A Sound row with "AdLib: music and effects (default)" and "PC speaker", shown with `soundDevice`.
    - Art greyed and Original unless `art`; Race view, Resolution, Anti-aliasing and Town greyed unless `enhanced`; Music greyed on Original unless `remasteredMusic`. The Music choice's second item reads "Original: as chosen under Sound" for SR2.
    - A grey note in Picture for SR2: "Street Rod 2 shows its original picture for now."
    - The status note names the game ("Found Street Rod 2, 1 saved game."). With `otherGame` it says "This folder holds Street Rod 2, not Street Rod." or the other way round. The DOS-program note names `Traits(game).program`.
  - The Game choice is greyed while a game download runs. The download button's tooltip, the offer's text and `Play` use the chosen game (`options.game`, `options.speaker`). The About blurb names both games.

- [ ] **Step 2: Build and test.** Run `cmake --build build-mac && ctest --test-dir build-mac -LE slow`. Expected: build without new warnings, all pass.

- [ ] **Step 3: Look at it.**
  - Copy the real `settings.ini` to the scratchpad first and put it back after. Without the copy, do not run the launcher.
  - Run the launcher three times and take each screenshot by window ID (`screencapture -x -o -l<id>`):
    - (a) the copied settings as they are: Street Rod, every control as before;
    - (b) `[Launcher] Game=2` with `[Game2] GameFolder` at the scratchpad's `sr2game`: Street Rod 2, found, no Cars, Sound row shown, picture options greyed;
    - (c) `[Game2] GameFolder` at the scratchpad's `Game` (SR1's files): the other-game note, Play greyed.
  - After each run, read `settings.ini`. Expected: `[Game] GameFolder` and `Program` unchanged by the SR2 runs, `[Game2]` holds only SR2's keys.

- [ ] **Step 4: Docs.** `launcher/README.md`: the Game choice, the per-game and shared settings, SR2's folder and download, the keys above.

- [ ] **Step 5: Commit** `git add launcher/launcher.h launcher/launcher.cpp launcher/README.md` and `git commit -m "Add the Game choice to the launcher"`.

### Task 5: Both games in the packages

**Files:**
- Create: `tools/check-package.sh`
- Modify: `tools/macos/package.sh`, `tools/windows/package.sh`, `tools/windows/Read me.txt`, `README.md`, `CHANGELOG.md`

**Interfaces:**
- Produces: `tools/check-package.sh <folder with the programs> [--windows]`. It prints `missing: <name>` for each of `Street Rod Enhanced`, `streetrod-enhanced` and `streetrod2-enhanced` (each with `.exe` under `--windows`) that is not an executable file there. Under `--windows` it also checks for the `Game` and `Game2` folders. It exits 1 on any miss, else prints `package ok: <folder>`.

- [ ] **Step 1: Write the check and run it on the current app**

Run: `sh tools/check-package.sh "dist/Street Rod Enhanced.app/Contents/MacOS"` (after a `tools/macos/package.sh --version 0.4.2` from master if `dist` is gone).
Expected: FAIL, `missing: streetrod2-enhanced`.

- [ ] **Step 2: Package both.**
  - Mac `package.sh`: copy, `strip -x` and codesign `streetrod2-enhanced` as `streetrod-enhanced`, then call the check on `Contents/MacOS`.
  - Windows `package.sh`: copy `streetrod2-enhanced.exe`, `mkdir "$out/Game2"`, then call the check with `--windows`.
  - `Read me.txt`: a step for Street Rod 2 (`sr2pc.zip` from the same page, its files straight into `Game2`, choose Street Rod 2 under Game).

- [ ] **Step 3: Run** `tools/macos/package.sh --version 0.4.2`. Expected: every test passes, `notices ok`, `package ok`, `codesign --verify` silent.

- [ ] **Step 4: Docs.** `README.md`: the players' part covers both games (where SR2's files go, the Game choice); remove "not in the downloads yet". `CHANGELOG.md` Unreleased: the Game choice, the SR2 download on a Mac, both games in the downloads.

- [ ] **Step 5: Street Rod 1's code.** Run `tools/sr1-codecheck.sh ddf265a HEAD`. Expected: no differing object.

- [ ] **Step 6: Commit** `git add tools/check-package.sh tools/macos/package.sh tools/windows/package.sh "tools/windows/Read me.txt" README.md CHANGELOG.md` and `git commit -m "Ship Street Rod 2 in the app and the zip"`.
