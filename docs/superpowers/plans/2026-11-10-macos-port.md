# macOS Apple Silicon Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A self-contained `Street Rod Enhanced.app` for Apple Silicon, shipped as a DMG on this fork's GitHub
Releases, whose launcher downloads the official game files on first launch.

**Architecture:** SDL3 and wxWidgets are built from source as static arm64 libraries (deployment target 12.0),
so the bundle holds just two executables and no dylibs. The game gets Retina output and a macOS key filter in
`src/host.c`; the launcher gets macOS paths, Mac menus, a credits rewrite and a downloader/installer
(`launcher/gamefiles.*`, `launcher/download.*`). Shell scripts under `tools/macos/` build the dependencies,
assemble and sign the bundle, and make the DMG; a GitHub Actions workflow runs the same scripts.

**Tech Stack:** C11 + SDL3 3.4 (game), C++17 + wxWidgets 3.2 incl. `net` (launcher), CMake 3.24+, CTest,
dmgbuild, GitHub Actions `macos-15` (arm64).

**Spec:** `docs/superpowers/specs/2026-11-10-macos-port-design.md`

## Global Constraints

- arm64 only; `LSMinimumSystemVersion` and deployment target `12.0`.
- No dylib outside `/usr/lib` and `/System` may be linked by either executable.
- Ad-hoc signing only (`codesign -s -`); no Developer ID.
- Bundle id `io.github.007hacky007.streetrod-enhanced`; version `0.3.0`.
- Author shown and stored everywhere: `007hacky007`. No donation links. `LICENSE` keeps
  `Copyright (c) 2026 Krzysztof Kania` and gains `Copyright (c) 2026 007hacky007` above it.
- User-Agent of every launcher HTTP request: `StreetRodEnhanced/0.3.0` (from `APP_VERSION_TEXT`).
- Game file sources, sizes and SHA-256: exactly the table in the spec's "The game files" section.
- macOS data root: `~/Library/Application Support/Street Rod Enhanced` (settings.ini, `Game/`, `Game/datadisk/`).
- Windows: no behaviour or file-location change; every new platform branch is `__APPLE__` / `__WXOSX__`.
- Commits: git identity is already set in this repo to 007hacky007 (verify with `git config user.name`
  before the first commit). One short imperative subject line, no body, no trailers, no AI attribution.
- Docs and UI text: plain hyphen-minus only, never Unicode dashes.
- Dependency pins: SDL `release-3.4.20`, wxWidgets `v3.2.12`.

## Review Focus

1. Ctrl+Cmd+F, Cmd+Q, Cmd+H or Cmd+Tab pressed in the game must not toggle the sound (the game toggles it on a
   Ctrl make code) or reach the game as keys. Tests: Task 2 `ctrl_then_cmd_f_sends_nothing`,
   `cmd_q_not_sent`, `suppressed_release_after_gui_up`.
2. A game folder that already holds anything (saved games, a partial copy) is never written by the
   installer. Test: Task 5 `refuses_nonfree_folder`, `free_folder_rules`.
3. A failed, truncated or changed download leaves nothing half-installed and says so. Tests: Task 5
   `rejects_checksum_mismatch`; manual checks in Task 6 against a dead and a wrong-file server.
4. The app run straight from the mounted (read-only) DMG, or from any folder, still finds its game program
   and writes only to Application Support. Manual check in Task 7 and Task 8.
5. Mouse pointer position in a Retina window and in full screen matches the cursor. Manual check in Task 2.

---

### Task 1: Static macOS dependencies

**Files:**
- Create: `tools/macos/build-deps.sh`

**Interfaces:**
- Produces: `build-deps/prefix` (already ignored by `build*/`) holding static `libSDL3.a` with its CMake config,
  and static wxWidgets (`core`, `base`, `net`) with its CMake config. Later tasks pass
  `-DCMAKE_PREFIX_PATH=$PWD/build-deps/prefix`.

- [ ] **Step 1: Write `tools/macos/build-deps.sh`**

`set -eu`; sources cloned shallow at the pinned tags into `build-deps/src`; both built with CMake + Ninja,
`-DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
-DCMAKE_INSTALL_PREFIX=build-deps/prefix`. SDL: `-DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF`.
wxWidgets (`--recurse-submodules`): `-DwxBUILD_SHARED=OFF -DwxBUILD_SAMPLES=OFF -DwxBUILD_TESTS=OFF
-DwxBUILD_DEMOS=OFF -DwxUSE_WEBREQUEST=ON`, builtin zlib/png/expat/regex. Skips a library whose install
already exists, so CI can cache the prefix. Must not use Homebrew's SDL3.

- [ ] **Step 2: Run it**

Run: `tools/macos/build-deps.sh`
Expected: exits 0; `build-deps/prefix/lib/libSDL3.a` and `build-deps/prefix/lib/libwx_osx_cocoau_core-3.2.a`
(or the equivalent wx static core library) exist.

- [ ] **Step 3: Check the deployment target**

Run: `otool -l build-deps/prefix/lib/libSDL3.a | grep -A4 LC_BUILD_VERSION | grep minos | sort -u`
Expected: only `minos 12.0`. Same check on the wx core library.

- [ ] **Step 4: Commit**

```bash
git add tools/macos/build-deps.sh
git commit -m "Add macOS dependency build script"
```

### Task 2: The game on macOS (Retina, Mac keys)

**Files:**
- Create: `src/host_mackeys.h`, `src/host_mackeys.c`, `tests/test_mackeys.c`, `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt` (option `SRE_TESTS`, default OFF; `add_subdirectory(tests)` when ON; APPLE needs
  nothing else since SDL3::SDL3 resolves to the static target), `src/host.c:58-59` (window flags),
  `src/host.c:680-700` (key events, focus lost)

**Interfaces:**
- Produces (SDL-header-only, no SDL calls, so the test links `SDL3::Headers` only):

```c
typedef struct { int scancode; bool down; } MacKey;
typedef struct {
    int pending_ctrl;          /* SDL_SCANCODE_LCTRL/RCTRL held but not sent yet, 0 if none */
    int dropped_ctrl;          /* a Ctrl whose make code was dropped (used in a Cmd shortcut), 0 if none */
    bool suppressed[SDL_SCANCODE_COUNT];   /* keys whose make code was not sent, so neither is the break */
} MacKeyFilter;
void mac_keys_reset(MacKeyFilter *f);
/* Returns how many events (0..2) to feed to the game, in order, in out[]. */
int mac_keys_filter(MacKeyFilter *f, int scancode, bool down, bool gui_held, MacKey out[2]);
```

Rules: a Ctrl make is held back. A GUI key, or any key while GUI is held, is suppressed (its break too) and
turns a held Ctrl into a dropped one. Any other key while Ctrl is held flushes the Ctrl make first. A Ctrl
break sends make + break if still held, break if flushed, nothing if dropped. Plain keys pass through.

- [ ] **Step 1: Write the failing tests in `tests/test_mackeys.c`**

A tiny `CHECK(cond)` macro, `main` runs each, nonzero exit on failure. Each test starts from `mac_keys_reset`.
- `ctrl_tap_toggles_on_release`: LCTRL down -> 0; LCTRL up -> 2: `{LCTRL,true},{LCTRL,false}`.
- `ctrl_then_cmd_f_sends_nothing`: LCTRL down, LGUI down (gui), F down (gui), F up (gui), LGUI up, LCTRL up ->
  every call returns 0.
- `cmd_q_not_sent`: LGUI down, Q down, Q up (all gui), LGUI up -> all 0.
- `ctrl_held_with_other_key_flushes`: LCTRL down -> 0; UP down -> 2: `{LCTRL,true},{UP,true}`; UP up -> 1;
  LCTRL up -> 1: `{LCTRL,false}`.
- `suppressed_release_after_gui_up`: LGUI down, F down (gui), LGUI up, F up with `gui_held=false` -> 0.
- `plain_keys_pass`: A down -> 1 `{A,true}`; A up -> 1 `{A,false}`.
- `reset_drops_pending_ctrl`: LCTRL down, `mac_keys_reset`, LCTRL up -> 0.

`tests/CMakeLists.txt`: `add_executable(test_mackeys test_mackeys.c ../src/host_mackeys.c)`, links
`SDL3::Headers`, `add_test(NAME mackeys COMMAND test_mackeys)`.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake -S . -B build-mac -G Ninja -DCMAKE_PREFIX_PATH=$PWD/build-deps/prefix -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_BUILD_TYPE=Release -DSRE_LAUNCHER=OFF -DSRE_TESTS=ON && cmake --build build-mac`
Expected: build fails, `mac_keys_filter` undefined.

- [ ] **Step 3: Implement `mac_keys_reset` / `mac_keys_filter` in `src/host_mackeys.c`**

- [ ] **Step 4: Run the tests**

Run: `cmake --build build-mac && ctest --test-dir build-mac --output-on-failure`
Expected: `mackeys` passes.

- [ ] **Step 5: Wire it into `src/host.c`**

- Window flags `SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY` (harmless on Windows).
- `#ifdef __APPLE__`: key down/up go through a static `MacKeyFilter`, with
  `gui_held = (ev.key.mod & SDL_KMOD_GUI) != 0`, each returned event to `key_event`. The Alt+Enter branch stays
  first. `SDL_EVENT_WINDOW_FOCUS_LOST` also calls `mac_keys_reset`. Mark with `ENH:` comments as in the engine.
- No own Ctrl+Cmd+F or Cmd+Q handling: SDL3's default Cocoa menu already has Quit (sends
  `SDL_EVENT_QUIT`) and Toggle Full Screen. Step 6 confirms it; add a handler only if it doesn't work.

- [ ] **Step 6: Run the game**

Download the two zips from the spec's table to the scratchpad, unzip `SR1/` into a test `Game/` folder and
`SRSE/` into `Game/datadisk/`.
Run: `build-mac/streetrod-enhanced --game-dir <Game> --check`
Expected: `SR.EXE ok: image ...`.
Then run without `--check` (and once with `--game-dir <Game>/datadisk`):
- the demo plays;
- a race and a drive through town show the enhanced view, sharp on a Retina screen;
- the mouse pointer matches the cursor in a window and in full screen;
- the green button, Ctrl+Cmd+F and Alt+Enter toggle full screen, and none of them toggles the sound;
- Ctrl alone still toggles the sound (on release);
- Cmd+Q quits;
- a gamepad drives, if one is at hand.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/host.c src/host_mackeys.c src/host_mackeys.h tests
git commit -m "Build the game on macOS with Retina output and Mac keys"
```

### Task 3: The launcher on macOS (paths, menus)

**Files:**
- Modify: `launcher/CMakeLists.txt` (wx components `core base net` in both `find_package` calls and
  `SRL_WX_LIBRARIES`), `launcher/settings.cpp:28-37`, `launcher/settings.h:1-3` (comment),
  `launcher/game.cpp` (`DefaultGameDir`), `launcher/launcher.cpp:392-421` (`CreateMenus`)

**Interfaces:**
- Produces: `settings::Folder()` on `__WXOSX__` = `wxStandardPaths::Get().GetUserDataDir()`
  (`~/Library/Application Support/Street Rod Enhanced`); `DefaultGameDir()` on `__WXOSX__` =
  `settings::Folder() + "/Game"`; `DefaultProgram()` unchanged (`streetrod-enhanced` beside the launcher,
  which in the bundle is `Contents/MacOS`).

- [ ] **Step 1: Make the changes**

Menus on `__WXOSX__`: no File or About menu. `wxID_ABOUT`, `wxID_PREFERENCES` and `wxID_EXIT` are appended to
the Game settings menu, and wxWidgets moves them into the application menu. Bind `wxID_ABOUT` to `About()`.
Windows and Linux keep today's menus.

- [ ] **Step 2: Build and run**

Run: `cmake -S . -B build-mac -DSRE_LAUNCHER=ON && cmake --build build-mac && open -n "build-mac/Street Rod Enhanced"`
(or run the binary directly).
Expected:
- the app menu holds About, Preferences (Cmd+,) and Quit (Cmd+Q), and the menu bar shows Game settings > Sounds;
- `~/Library/Application Support/Street Rod Enhanced/settings.ini` appears after a change;
- the folder field shows `.../Street Rod Enhanced/Game`;
- with the test `Game/` from Task 2 copied there, Play starts the game.

- [ ] **Step 3: Commit**

```bash
git add launcher
git commit -m "Build the launcher on macOS with Mac paths and menus"
```

### Task 4: Credits and license

**Files:**
- Modify: `LICENSE`, `launcher/app.cpp:11`, `launcher/launcher.cpp` (constants at 39-40, About at 589-636),
  `launcher/app.rc:28`, `launcher/app.manifest:3`, `README.md` (drop the Support section)

- [ ] **Step 1: Make the changes**

- `LICENSE`: add `Copyright (c) 2026 007hacky007` above Kania's line. Replace the last paragraph's
  "Test Drive is (c) 1987 Accolade / Distinctive Software Inc." with
  "Street Rod is (c) 1989 California Dreams.".
- `SetVendorName("007hacky007")`, `CompanyName` `007hacky007`, manifest name `007hacky007.StreetRodEnhanced`.
- About, both variants: `Author: 007hacky007`, `Website:` link text `github.com/007hacky007/streetrod-enhanced`
  to `https://github.com/007hacky007/streetrod-enhanced`. Remove `SUPPORT`, the Support line and link.

- [ ] **Step 2: Verify**

Run: `grep -rn -i "kkania\|buymeacoffee\|Author: Krzysztof" launcher README.md LICENSE`
Expected: no output. Then `grep -n "Krzysztof Kania" LICENSE` prints exactly one line. Rebuild the launcher
and open About: it shows the new author and website, and no Support line.

- [ ] **Step 3: Commit**

```bash
git add LICENSE README.md launcher
git commit -m "Credit 007hacky007 and drop the donation link"
```

### Task 5: Game files: checksum and installer

**Files:**
- Create: `launcher/sha256.h`, `launcher/sha256.cpp`, `launcher/gamefiles.h`, `launcher/gamefiles.cpp`,
  `tests/test_gamefiles.cpp`
- Modify: `launcher/CMakeLists.txt` (add both .cpp files), `tests/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
// sha256.h
std::string Sha256Hex(const void* data, size_t size);   // lowercase hex

// gamefiles.h
namespace gamefiles {
struct Archive {
    const char* label;     // "Street Rod", "Street Rod SE"
    const char* file;      // "sr1pc.zip"
    const char* url;       // the spec's official URL
    const char* sha256;
    unsigned long size;
    const char* top;       // folder inside the zip: "SR1", "SRSE"
    const char* subdir;    // under the game folder: "", "datadisk"
};
struct Download { const Archive* archive; wxMemoryBuffer data; };
const std::vector<Archive>& Official();          // SR1 first, then SRSE
wxString Url(const Archive& a);                  // SRE_DOWNLOAD_BASE + "/" + file when set, else a.url
bool FolderIsFree(const wxString& dir);          // missing, empty, or only .DS_Store
bool Install(const wxString& gameDir, const std::vector<Download>& downloads, wxString& error);
}
```

`Install`: checks every checksum first. Refuses unless `FolderIsFree(gameDir)`. Extracts into
`gameDir + ".partial"` (any stale one removed first): only file entries under `top + "/"`, skipping
`__MACOSX/` and any `.DS_Store`, refusing entries with `..` or absolute paths, `top/` stripped, written under
`subdir`. Then removes the free `gameDir` (and its `.DS_Store`) and renames `.partial` into place. On any
failure it removes `.partial` and leaves `gameDir` as it was. Error texts are plain sentences naming the
archive's `label`, for example "The download of Street Rod SE is damaged or has changed on the website."

- [ ] **Step 1: Write the failing tests in `tests/test_gamefiles.cpp`**

wxBase only (`wxInitializer`), a `CHECK` macro, each test in its own `wxFileName::CreateTempFileName`-based
temp dir. Zips are built in memory with `wxZipOutputStream`, and their `Archive` entries use `Sha256Hex` of
the buffer.
- `sha256_vectors`: `""` -> `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`; `"abc"` ->
  `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`.
- `official_table`: two entries; urls, sizes (415994, 568242) and sha256 exactly as in the spec; tops `SR1`,
  `SRSE`; subdirs `""`, `"datadisk"`.
- `url_override`: with `SRE_DOWNLOAD_BASE=http://127.0.0.1:8765` -> `http://127.0.0.1:8765/sr1pc.zip`;
  unset -> the official URL.
- `free_folder_rules`: missing -> true; empty -> true; only `.DS_Store` -> true; a file `HOTROD1.SAV` -> false;
  a subfolder -> false.
- `installs_layout`: zip A has `SR1/SR.EXE`, `SR1/LIB1`, `SR1/.DS_Store`, `__MACOSX/SR1/._SR.EXE`; zip B has
  `SRSE/SRSE.EXE`. After `Install` -> true: `game/SR.EXE` and `game/LIB1` have the right bytes,
  `game/datadisk/SRSE.EXE` exists, there is no `game/.DS_Store`, no `game/__MACOSX`, and no `game.partial`.
- `rejects_checksum_mismatch`: B's sha wrong -> false, error mentions B's label, `game` and `game.partial`
  don't exist.
- `refuses_nonfree_folder`: `game/HOTROD1.SAV` with content `x` -> false, the file still holds `x`, no
  `game/SR.EXE`.
- `rejects_zip_slip`: entry `SR1/../evil` -> false; no `evil` next to `game`.

Register as `add_test(NAME gamefiles ...)`, linking `wx::wxbase` (or `${wxWidgets_LIBRARIES}`) and the two
sources.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build build-mac`
Expected: fails on the undefined `Sha256Hex` / `gamefiles::*`.

- [ ] **Step 3: Implement `sha256.cpp`** (FIPS 180-4, portable, no platform crypto).

- [ ] **Step 4: Implement `gamefiles.cpp`** with `wxZipInputStream` over a `wxMemoryInputStream`.

- [ ] **Step 5: Run the tests**

Run: `cmake --build build-mac && ctest --test-dir build-mac --output-on-failure`
Expected: `mackeys` and `gamefiles` pass.

- [ ] **Step 6: Commit**

```bash
git add launcher tests
git commit -m "Add the game files installer with checksums"
```

### Task 6: Download in the launcher (macOS)

**Files:**
- Create: `launcher/download.h`, `launcher/download.cpp`
- Modify: `launcher/launcher.h`, `launcher/launcher.cpp` (constructor, `CreateMainPage` status row,
  `UpdateState`), `launcher/CMakeLists.txt`

**Interfaces:**
- Consumes: `gamefiles::Official`, `gamefiles::Url`, `gamefiles::FolderIsFree`, `gamefiles::Install`,
  `gamefiles::Download` (Task 5).
- Produces:

```cpp
// download.h: fetches every gamefiles::Official() archive in turn, in memory, with wxWebRequest.
class GameDownload : public wxEvtHandler {
public:
    using Done = std::function<void(bool ok, std::vector<gamefiles::Download> downloads, wxString error)>;
    using Progress = std::function<void(const wxString& text)>;   // "Downloading Street Rod SE (2 of 2)..."
    void Start(Progress progress, Done done);
    bool Running() const;
};
```

Each request: `wxWebSession::GetDefault().CreateRequest(this, gamefiles::Url(a))`, `SetStorage(Storage_Memory)`,
`SetHeader("User-Agent", "StreetRodEnhanced/" APP_VERSION_TEXT)`. A failed or cancelled request ends with
`ok=false` and an error naming the archive's label and wx's message. The UI parts below are compiled only on
`__WXOSX__`; `download.cpp` and `gamefiles.cpp` build everywhere.

- [ ] **Step 1: Implement `GameDownload`**

- [ ] **Step 2: Wire it into `LauncherFrame` (`__WXOSX__` only)**

- **Button:** a `Download game` button at the end of the status row, shown when
  `gamefiles::FolderIsFree(folder)`. Clicking it starts the download.
- **First-launch prompt:** after the window shows (`CallAfter`), if the folder is free, ask:
  "Street Rod Enhanced needs the original game. Download Street Rod and the Street Rod SE data disk (about
  1 MB) from streetrodonline.com, the official site?" The buttons are **Download**, **Choose Folder...** and
  **Not Now** (`wxMessageDialog` with `wxYES_NO_CANCEL` and `SetYesNoCancelLabels`). Choose Folder... opens
  `BrowseFolder()`.
- **While running:** the status note shows the progress text; Play, the folder field and both Browse buttons
  are disabled.
- **When done:** `gamefiles::Install(folder, downloads, error)`, then `UpdateState()`.
- **On failure** (download or install): ask "<error>" with **Try Again**, **Choose Folder...** and **Cancel**.
- **Never overwrite:** the prompt and the button never appear when the folder isn't free.

- [ ] **Step 3: Fresh-user run**

Run: delete `~/Library/Application Support/Street Rod Enhanced`, then start the launcher.
Expected:
- the prompt appears, and Download installs the files: `Game/SR.EXE` and `Game/datadisk/SRSE.EXE` exist;
- the status reads "Found Street Rod, no saved games." and the Cars choice offers the data disk;
- Play works with both car choices;
- a second start shows no prompt.

- [ ] **Step 4: Failure runs**

- **Dead server:** delete the folder again, then start with `SRE_DOWNLOAD_BASE=http://127.0.0.1:9`. Expected:
  the error prompt names Street Rod; Try Again fails the same way; Cancel leaves no `Game` and no
  `Game.partial`.
- **Wrong files:** serve a folder holding two wrong `sr1pc.zip` / `srsepc.zip` with
  `python3 -m http.server 8765 --directory <dir>` (scratchpad) and set `SRE_DOWNLOAD_BASE` to it. Expected:
  the "damaged or has changed" error, and nothing installed.
- **Existing saves:** a `Game/` holding only `HOTROD1.SAV` gets no prompt and no button.

- [ ] **Step 5: Commit**

```bash
git add launcher
git commit -m "Download the official game files on first launch"
```

### Task 7: The app bundle

**Files:**
- Create: `tools/macos/package.sh`, `launcher/macos/Info.plist.in`, `launcher/macos/AppIcon.icns`
- Modify: `launcher/make_icon.py` (also writes `macos/AppIcon.icns`: the same tile, nearest-neighbour, sizes
  16 to 1024, via Pillow's ICNS writer)

**Interfaces:**
- Consumes: `build-deps/prefix` (Task 1).
- Produces: `tools/macos/package.sh --version V` -> `dist/Street Rod Enhanced.app`, with tests run on the way.

Bundle layout: `Contents/MacOS/Street Rod Enhanced` (launcher, `CFBundleExecutable`),
`Contents/MacOS/streetrod-enhanced` (game), `Contents/Resources/AppIcon.icns`, `Contents/Resources/LICENSE`,
`Contents/Info.plist`.

Info.plist values:
- `CFBundleName` and `CFBundleDisplayName`: `Street Rod Enhanced`
- `CFBundleIdentifier`: `io.github.007hacky007.streetrod-enhanced`
- `CFBundleExecutable`: `Street Rod Enhanced`
- `CFBundleIconFile`: `AppIcon`
- `CFBundleShortVersionString` and `CFBundleVersion`: `@VERSION@`
- `LSMinimumSystemVersion`: `12.0`
- `NSHighResolutionCapable`: true
- `CFBundlePackageType`: `APPL`
- `NSHumanReadableCopyright`: `Copyright (c) 2026 007hacky007. MIT License.`

The game is started from `Contents/MacOS`, so it belongs to the same bundle and shows the app's name and icon
in the Dock.

- [ ] **Step 1: Write the script and the plist template; regenerate the icons**

The script configures `build-mac` with `CMAKE_PREFIX_PATH=build-deps/prefix`, arm64, 12.0, Release,
`SRE_TESTS=ON`, then builds it and runs `ctest` (stops on failure). It assembles the bundle in a fresh `dist/`
(add `/dist/` to `.gitignore`) and fills `@VERSION@`. Signing: `codesign --force -s -` on the game, then on
the bundle, then `codesign --verify --strict`.

- [ ] **Step 2: Build it**

Run: `tools/macos/package.sh --version 0.3.0`
Expected: exits 0 with the tests passed.

- [ ] **Step 3: Check the binaries**

Run: `for b in "dist/Street Rod Enhanced.app/Contents/MacOS/"*; do lipo -archs "$b"; otool -L "$b" | tail -n +2 | grep -v -E "^\s+(/usr/lib|/System)/"; otool -l "$b" | grep -A4 LC_BUILD_VERSION | grep minos; done`
Expected: `arm64` and `minos 12.0` for both, and no other library lines.

- [ ] **Step 4: Run it from elsewhere**

Copy the app to `/Applications` and to a scratchpad folder, and run each. Expected: the launcher's Program
field points inside that copy, Play works, the Dock shows the app's icon for the launcher and the game, and
nothing is written inside the bundle (`codesign --verify --strict` still passes after a game is saved).

- [ ] **Step 5: Commit**

```bash
git add .gitignore tools/macos/package.sh launcher/macos launcher/make_icon.py
git commit -m "Package Street Rod Enhanced.app"
```

### Task 8: DMG, version and docs

**Files:**
- Create: `tools/macos/dmg/settings.py`, `tools/macos/dmg/background.html`, `tools/macos/dmg/background.png`,
  `tools/macos/dmg/background@2x.png`, `tools/macos/dmg/render_background.sh`, `tools/macos/make-dmg.sh`
- Modify: `CMakeLists.txt:2` (VERSION 0.3.0), `launcher/version.h` (0, 3, 0, "0.3.0"), `CHANGELOG.md`,
  `README.md`, `launcher/README.md`, `launcher/settings.h` comment

Adapt `../NFSV-self-contained/builder/dmg` (dmgbuild 1.6.7, HiDPI TIFF background, Chrome-rendered HTML).
Differences from NFSV:
- the window holds the app, an `Applications` symlink (players drag to install) and `LICENSE`;
- the background's notes: drag to Applications; on macOS 15 or newer, open once, click Done, then System
  Settings > Privacy & Security > Open Anyway; on macOS 14 or older, right-click > Open;
- output `dist/Street-Rod-Enhanced.dmg` plus `.sha256` (hyphens, because GitHub turns spaces in asset names
  into dots).

`make-dmg.sh --version V` runs dmgbuild from a private venv in `build-deps/venv`.

- [ ] **Step 1: Write the DMG files and render the background**

Run: `tools/macos/dmg/render_background.sh`
Expected: both PNGs written; check them visually.

- [ ] **Step 2: Bump the version and write the docs**

- `CHANGELOG.md` 0.3.0: macOS Apple Silicon app; first-launch download; Retina; Mac keys; credits.
- `README.md`: a "How to play (Mac)" section: download the DMG from this fork's Releases, drag to
  Applications, first-open steps, the download prompt, where saves live, Space for click and shift (no Ins or
  keypad needed), Ctrl+Cmd+F full screen, Cmd+Q. A "Build (macOS)" section: `tools/macos/build-deps.sh`,
  `tools/macos/package.sh --version 0.3.0`, `tools/macos/make-dmg.sh --version 0.3.0`. Windows sections
  unchanged.
- `launcher/README.md`: the macOS paths and the download.

- [ ] **Step 3: Build and try the DMG**

Run: `tools/macos/package.sh --version 0.3.0 && tools/macos/make-dmg.sh --version 0.3.0 && open dist/Street-Rod-Enhanced.dmg`
Expected:
- the window shows the background, the app and Applications;
- the app started straight from the mounted DMG runs and plays (Review Focus 4);
- About says 0.3.0.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt launcher CHANGELOG.md README.md tools/macos
git commit -m "Add the macOS disk image and docs, version 0.3.0"
```

### Task 9: Release workflow, run for real

**Files:**
- Create: `.github/workflows/release.yml`

Triggers: `push` of tags `v*` and `workflow_dispatch`. Job:
- `runs-on: macos-15`; `permissions: contents: write`;
- `actions/checkout`, `actions/setup-python` (3.12), `actions/cache` for `build-deps/prefix`, keyed on
  `hashFiles('tools/macos/build-deps.sh')`;
- `tools/macos/build-deps.sh`, then `tools/macos/package.sh --version "$VERSION"`, then
  `tools/macos/make-dmg.sh --version "$VERSION"`. `VERSION` is `${GITHUB_REF_NAME#v}` for tags and the
  contents of `launcher/version.h`'s `APP_VERSION_TEXT` for dispatch;
- dispatch: `actions/upload-artifact` with the DMG and its `.sha256`;
- tag: `gh release create "$GITHUB_REF_NAME" <dmg> <sha256> --draft --title "Street Rod Enhanced $VERSION"
  --generate-notes`.

- [ ] **Step 1: Write the workflow, commit and push**

```bash
git add .github/workflows/release.yml
git commit -m "Add the macOS release workflow"
git push origin master
```

- [ ] **Step 2: Dispatch and check**

Run: `gh workflow run release.yml --ref master`, then `gh run watch` (via Monitor) and
`gh run download <id> -D <scratchpad>/ci`.
Expected: the run succeeds. The downloaded DMG's app passes the Task 7 Step 3 checks and runs on this Mac
through the fresh-user flow of Task 6 Step 3.

- [ ] **Step 3: Tag 0.3.0**

Run: `git tag v0.3.0 && git push origin v0.3.0`, wait for the run, then `gh release view v0.3.0 --json isDraft,assets`.
Expected: `isDraft: true`, with `Street-Rod-Enhanced.dmg` and `Street-Rod-Enhanced.dmg.sha256` attached.
If the run fails: fix it, delete the draft release and the tag (`gh release delete v0.3.0 --yes`,
`git push origin :refs/tags/v0.3.0`, `git tag -d v0.3.0`), commit, push and tag again. The final draft is left
for the owner to publish.
