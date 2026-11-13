# Launcher Updates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The launcher shows a strip when a newer Street Rod Enhanced is published and updates itself with one click on macOS and Windows.

**Architecture:** A window-free module `launcher/update.cpp` holds the manifest, the version rules and the file swaps (Windows folder, macOS bundle), unit-tested like `artpack`. `launcher/launcher.cpp` adds the strip, the menu item and the download flow on top of `WebFetch`. `release.yml` attaches a manifest `Street-Rod-Enhanced.txt` written by a small script.

**Tech Stack:** C++17, wxWidgets 3.2 (core, base, net), CMake/ctest, POSIX sh, GitHub Actions. macOS system tools `hdiutil`, `ditto`, `codesign`, `open`.

**Spec:** `docs/superpowers/specs/2026-11-13-launcher-updates-design.md`

## Global Constraints

- Ships in 0.5.0; built on master. `APP_VERSION_TEXT` stays "0.5.0".
- Only the release GitHub marks latest counts: `https://github.com/007hacky007/street-rod-remastered/releases/latest/download/`. `SRE_UPDATE_BASE` replaces that base.
- Asset names: `Street-Rod-Enhanced.txt` (manifest), `Street-Rod-Enhanced.dmg`, `Street-Rod-Enhanced-windows.zip` (top folder `Street Rod Enhanced/`).
- Manifest lines: `version=X.Y.Z`, `format=1`, `mac.sha256=`, `mac.size=`, `windows.sha256=`, `windows.size=`.
- Release page: `https://github.com/007hacky007/street-rod-remastered/releases/tag/vX.Y.Z`.
- Setting: `[Update] Skipped=X.Y.Z` in settings.ini.
- Strip text: `Street Rod Enhanced X.Y.Z is out.`, link `What's new`, buttons `Update`, `Later`; then `Downloading X.Y.Z… N of M MB` with `Cancel`; `The update failed: <reason>` with `Try Again`, `Later`; `X.Y.Z is ready.` with `Restart`.
- Menu item `Check for Updates…`; no newer version: `Street Rod Enhanced 0.5.0 is the latest version.`
- Never touch, on Windows, `Game`, `Game2`, `Remaster`, `settings.ini` or any file the zip does not contain; on macOS anything outside the bundle.
- Before the final swap the installed version is untouched; a failed swap rolls back.
- No Linux strip. No setting to switch the check off. No new third-party libraries.
- Short UI wording; only hyphen-minus dashes; no personal data anywhere. SR1/SR2 game objects unchanged.
- Commit messages: one short imperative line, no body, no trailers.

## Review Focus

- A manifest whose version is not three numbers (`0.6`, `v0.6.0`, `0.6.0-beta`) must never show the strip: `IsNewer` returns false. Pinned in Task 1.
- An interrupted Windows swap (a file that can't be renamed midway) must leave every installed file as it was. Pinned in Task 2 with a read-only/locked stand-in.
- A Windows folder holding user files named `*.old` (DOS games use `.OLD`) must keep them after clean-up: only files listed by the swap are removed. Pinned in Task 2.
- A macOS bundle path under `/AppTranslocation/` or inside `/Volumes/` must refuse to update with the "move to Applications" message. Pinned in Task 3.
- "Later" on 0.6.0 followed by a published 0.6.1 must show the strip again; `Check for Updates…` shows it even for the skipped version. Pinned in Task 1.

---

### Task 1: Manifest and version rules

**Files:**
- Create: `launcher/update.h`, `launcher/update.cpp`
- Create: `tests/test_update.cpp`
- Modify: `launcher/CMakeLists.txt` (add `update.cpp`), `tests/CMakeLists.txt` (add `test_update`, wxBase only, `add_test(NAME update ...)`, sources `test_update.cpp ../launcher/update.cpp ../launcher/gamefiles.cpp ../launcher/sha256.cpp`, same pattern as `test_artpack`)

**Interfaces:**
- Produces (namespace `update`):
  - `constexpr int FORMAT = 1;`
  - `struct Download { wxString sha256; unsigned long size = 0; };`
  - `struct Manifest { wxString version; int format = 0; Download mac, windows; };`
  - `wxString ManifestUrl(); wxString DmgUrl(); wxString ZipUrl();`
  - `wxString ReleasePage(const wxString& version);`
  - `bool ParseManifest(const wxString& text, Manifest& m, wxString& error);` true only with an X.Y.Z version, format > 0, and both downloads with size > 0 and a 64-hex-digit lower-case SHA-256.
  - `bool IsNewer(const wxString& a, const wxString& b);` a > b, numerically by major, minor, patch; false if either is not exactly three dot-separated decimal numbers.
  - `bool ShouldOffer(const Manifest& latest, const wxString& current, const wxString& skipped, bool asked);` `IsNewer(latest.version, current)` and (`asked` or `skipped` empty or `IsNewer(latest.version, skipped)`).
  - `bool Verify(const wxMemoryBuffer& data, const Download& d, wxString& error);` size and SHA-256 match; error `The download is damaged. Try again.`

- [ ] **Step 1: Write the failing tests** in `tests/test_update.cpp`, with the `CHECK` macro and `wxInitializer` of `test_artpack.cpp`:
  - `ParseManifest` accepts the six Global Constraints keys (with `\r\n` line ends too) and fills every field; rejects a missing `windows.size`, a 63-digit hash, `version=0.6`, `format=0`.
  - `IsNewer("0.6.0","0.5.0")`, `IsNewer("0.10.0","0.9.9")`, `IsNewer("1.0.0","0.99.99")` true; `IsNewer("0.5.0","0.5.0")`, `IsNewer("0.6","0.5.0")`, `IsNewer("v0.6.0","0.5.0")`, `IsNewer("0.6.0-beta","0.5.0")`, `IsNewer("","0.5.0")` false.
  - `ShouldOffer` with latest 0.6.0, current 0.5.0: skipped "" true; skipped "0.6.0" false; skipped "0.6.0" with asked true; latest 0.6.1 skipped "0.6.0" true; latest 0.5.0 false even when asked.
  - `ReleasePage("0.6.0")` equals the Global Constraints URL with `v0.6.0`.
  - With `SRE_UPDATE_BASE=http://127.0.0.1:8000/` (`wxSetEnv`), `ZipUrl()` is `http://127.0.0.1:8000/Street-Rod-Enhanced-windows.zip`; unset, `ManifestUrl()` is the latest/download URL.
  - `Verify` accepts the right size and hash of "abc"; rejects a wrong size and a wrong hash.

- [ ] **Step 2: Run it to see it fail**
  Run: `cmake --build build-mac && ctest --test-dir build-mac -R '^update$' --output-on-failure`
  Expected: build error, `update.h` not found.

- [ ] **Step 3: Implement** `update.h` / `update.cpp` with the interfaces above. URL base handling as `artpack::Url` (strip one trailing `/`). Manifest error text: `The description of the new version on GitHub couldn't be read.`

- [ ] **Step 4: Run it to see it pass**
  Run: the Step 2 command. Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 5: Commit** `git add launcher/update.* launcher/CMakeLists.txt tests/test_update.cpp tests/CMakeLists.txt && git commit -m "Read the launcher's update manifest"`

### Task 2: Windows folder swap

**Files:**
- Modify: `launcher/update.h`, `launcher/update.cpp`, `tests/test_update.cpp`

**Interfaces:**
- Consumes: `Verify`, `Download` (Task 1); `gamefiles::Unzip`, `gamefiles::RemoveTree`.
- Produces:
  - `wxString StagingDir(const wxString& dir);` `dir + "/.update"`.
  - `bool StageFolder(const wxString& dir, const wxMemoryBuffer& zip, const Download& d, wxString& error);` `Verify`, then remove any old staging, `Unzip(..., "Street Rod Enhanced", StagingDir(dir), "the new version", error)`; fails (removing staging) unless `StagingDir(dir)/Street Rod Enhanced.exe` exists. `dir` is untouched.
  - `bool ApplyFolder(const wxString& dir, wxString& error);` for every file under staging (recursively, relative path `p`): create missing parent folders in `dir`; if `dir/p` is an existing file (not a directory) rename it to `dir/p.old`; rename staged file to `dir/p`. Writes the list of created `.old` paths (relative, one per line) to `dir/.update-old.txt` before the first rename and appends as it goes. On any failure, undo in reverse order (move new back to staging, `.old` back to `p`) and return false with `Couldn't replace %s. Is it open?`. On success remove staging.
  - `void CleanFolder(const wxString& dir);` deletes each path listed in `.update-old.txt` that exists and can be deleted, rewrites the list with those it couldn't delete (removes the list when empty), and removes a leftover staging folder.
  - `wxString CantUpdateFolder(const wxString& dir);` "" if `dir` is writable (try creating and deleting a temp file in it), else `Street Rod Enhanced can't update itself in %s. Download the new version from GitHub instead.`

- [ ] **Step 1: Write the failing tests** (temp folders as in `test_artpack.cpp`; a zip built with its `Zip()` helper; entries under `Street Rod Enhanced/`):
  - Install folder with `Street Rod Enhanced.exe`="old", `a.dll`="old", `Game/SAVE.OLD`="mine", `Game2/x`="mine", `Remaster/pack.json`="mine", `settings.ini`="mine", `notes.old`="mine". Zip with `Street Rod Enhanced.exe`="new", `a.dll`="new", `b.dll`="new", and directory entries `Game/` and `Game2/`.
  - `StageFolder` with a wrong hash fails and leaves no staging and the install unchanged.
  - `StageFolder` of a zip without the exe fails.
  - `StageFolder` then `ApplyFolder` succeed: exe and `a.dll` read "new", `b.dll` exists, every "mine" file unchanged, `Street Rod Enhanced.exe.old` reads "old", no staging.
  - `CleanFolder` then removes `Street Rod Enhanced.exe.old` and `a.dll.old` and `.update-old.txt`, and keeps `Game/SAVE.OLD` and `notes.old`.
  - Roll-back: stage, then put a non-empty directory at `dir/b.dll` (moving a file onto it fails on POSIX and Windows alike). `ApplyFolder` returns false; exe and `a.dll` read "old"; `dir/b.dll` is still the directory; no `.old` files and no `.update-old.txt` remain.
  - `CantUpdateFolder` of a temp folder is ""; of a folder with write permission removed (POSIX only, `#ifndef __WINDOWS__`) is not "".

- [ ] **Step 2: Run it to see it fail.** Run: Task 1 Step 2 command. Expected: build error, `StageFolder` not declared.

- [ ] **Step 3: Implement** the interfaces above.

- [ ] **Step 4: Run it to see it pass.** Expected: `update` passes.

- [ ] **Step 5: Commit** `git commit -am "Swap the Windows folder for a new version"` (add new files explicitly if any).

### Task 3: macOS bundle swap

**Files:**
- Modify: `launcher/update.h`, `launcher/update.cpp`, `tests/test_update.cpp`

**Interfaces:**
- Consumes: `Verify`, `Download`, `Manifest` (Task 1); `gamefiles::RemoveTree`.
- Produces:
  - `wxString BundleOf(const wxString& executable);` the `.app` folder when `executable` is `<X>.app/Contents/MacOS/<name>`, else "".
  - `wxString StagedBundle(const wxString& bundle);` `<parent>/.<X>.app.update`; `wxString OldBundle(const wxString& bundle);` `<parent>/.<X>.app.old`.
  - `wxString CantUpdateBundle(const wxString& bundle);` "" when updating can work. Empty `bundle`: `This copy of Street Rod Enhanced isn't an installed app.` Path containing `/AppTranslocation/` or starting with `/Volumes/`: `Move Street Rod Enhanced to Applications first, then update.` Parent not writable: `Street Rod Enhanced can't update itself in %s. Download the new version from GitHub instead.`
  - `bool ApplyBundle(const wxString& bundle, wxString& error);` rename `bundle` to `OldBundle`, rename `StagedBundle` to `bundle`; if the second fails, rename the old one back and return false (`Couldn't replace %s.`).
  - `void CleanBundle(const wxString& bundle);` removes `OldBundle` and `StagedBundle` if present.
  - Only on `__WXOSX__`: `bool StageBundle(const wxString& bundle, const wxMemoryBuffer& dmg, const Manifest& m, wxString& error);` `Verify(dmg, m.mac)`; write it to a temp file; `hdiutil attach -nobrowse -readonly -noautoopen -mountpoint <tempdir> <file>`; `ditto "<mount>/Street Rod Enhanced.app" StagedBundle(bundle)`; `hdiutil detach <mount>` (always, also on failure); `codesign --verify --strict` on the staged bundle; staged `Contents/Info.plist` `CFBundleShortVersionString` equals `m.version`. Run the tools with `wxExecute(argv, wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE)`, argument arrays, no shell. Any failure removes the staged bundle and the temp files.

- [ ] **Step 1: Write the failing tests** (plain folders stand in for bundles, so they run on every platform):
  - `BundleOf("/Applications/Street Rod Enhanced.app/Contents/MacOS/Street Rod Enhanced")` is `/Applications/Street Rod Enhanced.app`; `BundleOf("/usr/local/bin/srelauncher")` is "".
  - `StagedBundle` / `OldBundle` of `/Applications/Street Rod Enhanced.app` are `/Applications/.Street Rod Enhanced.app.update` / `.old`.
  - `CantUpdateBundle` messages for "", `/private/var/folders/x/AppTranslocation/y/Street Rod Enhanced.app`, `/Volumes/Street Rod Enhanced 0.5.0/Street Rod Enhanced.app`; "" for a bundle folder in a temp dir.
  - `ApplyBundle`: bundle with `Contents/v`="old", staged with `Contents/v`="new": afterwards bundle reads "new", `OldBundle` reads "old"; `CleanBundle` removes `OldBundle`.
  - `ApplyBundle` with no staged bundle returns false and the bundle still reads "old", no `OldBundle` left.

- [ ] **Step 2: Run it to see it fail.** Expected: build error, `BundleOf` not declared.

- [ ] **Step 3: Implement** the interfaces above (`StageBundle` untested here; Task 5 checks it by hand).

- [ ] **Step 4: Run it to see it pass.** Expected: `update` passes; `cmake --build build-mac` has no new warnings.

- [ ] **Step 5: Commit** `git commit -am "Swap the macOS app for a new version"`

### Task 4: The release's manifest

**Files:**
- Create: `tools/release-manifest.sh` (usage: `tools/release-manifest.sh VERSION DIST_DIR`; reads `Street-Rod-Enhanced.dmg` and `Street-Rod-Enhanced-windows.zip` in `DIST_DIR`, writes `DIST_DIR/Street-Rod-Enhanced.txt` with the six keys; size from `wc -c`, hash from `shasum -a 256` or `sha256sum`, whichever exists; exits non-zero if a file is missing or VERSION is not X.Y.Z)
- Create: `tests/test_release_manifest.sh`; Modify: `tests/CMakeLists.txt` (`add_test(NAME release_manifest COMMAND sh .../test_release_manifest.sh)`, not under `if(TARGET srelauncher)`)
- Modify: `.github/workflows/release.yml` (in the release job, before `gh release create`: `tools/release-manifest.sh "$VERSION" dist`; this job needs `actions/checkout` if it doesn't have it)

**Interfaces:**
- Produces: the manifest Task 1 parses.

- [ ] **Step 1: Write the failing test** `tests/test_release_manifest.sh`: in a temp dir write `Street-Rod-Enhanced.dmg`="abc" and `Street-Rod-Enhanced-windows.zip`="hello"; run the script with `0.6.0`; expect exactly these lines: `version=0.6.0`, `format=1`, `mac.sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`, `mac.size=3`, `windows.sha256=2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824`, `windows.size=5`. Also: version `0.6` exits non-zero; a missing zip exits non-zero.

- [ ] **Step 2: Run it to see it fail.** Run: `cmake -S . -B build-mac >/dev/null && ctest --test-dir build-mac -R release_manifest --output-on-failure`. Expected: FAIL, script not found.

- [ ] **Step 3: Implement** `tools/release-manifest.sh` (POSIX sh, `set -eu`) and the `release.yml` step.

- [ ] **Step 4: Run it to see it pass.** Expected: `release_manifest` passes. Also `sh tools/release-manifest.sh 0.5.0 dist` after `tools/macos/package.sh` and `make-dmg.sh` fails only for the missing Windows zip (expected).

- [ ] **Step 5: Commit** `git add tools/release-manifest.sh tests/test_release_manifest.sh tests/CMakeLists.txt .github/workflows/release.yml && git commit -m "Attach an update manifest to releases"`

### Task 5: The strip and the update flow

**Files:**
- Modify: `launcher/launcher.h`, `launcher/launcher.cpp`, `launcher/app.cpp` (only if the start-up clean-up fits better there)
- Modify: `CHANGELOG.md` (0.5.0: one line "The launcher tells when a new version is out and updates itself."), `README.md` and `launcher/README.md` if they list launcher features (one short line each)

**Interfaces:**
- Consumes: everything in `update.h` (Tasks 1-3); `WebFetch`; `settings::GetString/SetString`; `LauncherDir()`; `wxLaunchDefaultBrowser`.
- Produces: `LauncherFrame` members (names are the builder's): a strip panel at the top of the main page (above the `Game:` row in `CreateMainPage`'s `all` sizer), its text, a `What's new` `wxHyperlinkCtrl`, buttons Update/Later/Cancel/Try Again/Restart, a `WebFetch updateFetch_`, a `wxTimer` for progress, `update::Manifest updateLatest_`, a state (hidden, offer, downloading, failed, ready). Menu id `ID_CHECK_UPDATES`.

Behaviour (all from the spec; Global Constraints has the exact text):
- Only on `__WXOSX__` and `__WXMSW__`; elsewhere none of this is built or shown.
- Start: before anything else in the constructor's `CallAfter`, clean up (`CleanBundle(BundleOf(exe))` on macOS, `CleanFolder(LauncherDir())` on Windows), then `CheckUpdate(false)` next to `CheckArt(true)`. Use a separate `WebFetch` from the art's so both run at once.
- `CheckUpdate(asked)`: fetch `ManifestUrl()`; on success and `ShouldOffer(m, APP_VERSION_TEXT, Skipped, asked)` show the offer. When `asked`: no newer version shows the latest-version message box; a failed fetch shows `Couldn't check for updates.` with the error.
- `What's new`: `wxLaunchDefaultBrowser(ReleasePage(version))`.
- `Later`: `settings::SetString("Update", "Skipped", version)`, hide.
- `Update`: if `m.format > update::FORMAT` or the platform's `CantUpdate*` message is not empty, show that message in a `wxMessageDialog` with buttons `Open GitHub` / `Cancel` (format case: `This version needs a manual download.`), `Open GitHub` opens `ReleasePage`; stop. Else fetch `DmgUrl()` / `ZipUrl()`, progress text every 250 ms from `Received()` and the manifest's size (as `artTimer_`), then `StageBundle` / `StageFolder`; failure shows the failed state, success the ready state.
- `Cancel` cancels the fetch and returns to the offer.
- `Restart`: `ApplyBundle` / `ApplyFolder`; on failure the failed state. On success start the new launcher (`wxExecute({"open","-n",bundle})` on macOS; the folder's `Street Rod Enhanced.exe` on Windows, cwd the folder) and `Close()`.
- The close handler also waits for and cancels `updateFetch_`, as it does for `artFetch_`; `FinishClosing` counts it.
- The strip shows and hides with `Show()` and `FitPage()` (the existing fit helper).
- Menu: `Check for Updates…` after About on macOS (Game settings menu); on Windows in the File menu above Exit. Not on other platforms.

- [ ] **Step 1: Build and run the suite.** Run: `tools/macos/package.sh --version 0.5.0 > $SCRATCH/pkg.log 2>&1; tail -3 $SCRATCH/pkg.log`. Expected: `100% tests passed` (now 48 tests) and `package ok`.

- [ ] **Step 2: Hand test, offer and Later.** Make a fake release dir: copy `dist/Street Rod Enhanced.app`, package a second app with `tools/macos/package.sh --version 0.5.1` then `tools/macos/make-dmg.sh --version 0.5.1`, keep its DMG in `$SCRATCH/fake-release/`, write the manifest with `tools/release-manifest.sh 0.5.1 $SCRATCH/fake-release` (put any file as the Windows zip). Serve it: `python3 -m http.server 8765 --bind 127.0.0.1 -d $SCRATCH/fake-release`. Rebuild the 0.5.0 app, copy it to `$SCRATCH/apps/`, run its launcher with `SRE_UPDATE_BASE=http://127.0.0.1:8765`.
  Expected: the strip `Street Rod Enhanced 0.5.1 is out.` appears; `What's new` opens the v0.5.1 tag page; `Later` hides it and a restart shows nothing; `Check for Updates…` shows it again. With the server stopped, a start shows nothing and `Check for Updates…` says it couldn't check.

- [ ] **Step 3: Hand test, update.** Same setup, click Update.
  Expected: progress text, then `0.5.1 is ready.`; `.Street Rod Enhanced.app.update` exists next to the app; Restart opens the 0.5.1 launcher (About says 0.5.1), the old bundle is gone after that start, settings and the Remaster pack untouched. Then: corrupt one byte of the served DMG: `The update failed: The download is damaged. Try again.`, the app unchanged. Run the 0.5.0 app from its mounted DMG: Update says to move it to Applications first. Report any macOS permission prompt seen (App Management) to the owner.

- [ ] **Step 4: Windows.** Push to a branch only if the owner agrees; otherwise rely on CI: the Windows job builds the launcher and runs `test_update` (Task 2's swap runs on real Windows rename rules there). A hand test on a real PC stays open (as for the zip itself).

- [ ] **Step 5: Commit** `git add launcher/ CHANGELOG.md README.md && git commit -m "Offer new versions in the launcher"`

---

## Self-review notes

- Spec coverage: check (Task 5), strip and Later and menu (Task 5, rules in Task 1), download and checksum (Tasks 1, 5), macOS staging/swap/clean-up/can't-update (Task 3, 5), Windows staging/swap/roll-back/clean-up/can't-update (Task 2, 5), format fallback (Task 5), release manifest (Task 4), 0.5.0 first with a manifest (Task 4 runs on the v0.5.0 tag), no Linux (Task 5), macOS refusal falls back to the release page (Task 5 Restart failure shows failed state; Update's can't-update path opens GitHub).
- The Windows clean-up deletes only listed `.old` files, which the spec's "on start, deletes `*.old`" left open; this is stricter and protects DOS `.OLD` files.
