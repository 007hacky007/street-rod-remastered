# macOS Apple Silicon port - design

Phase 1 of two. Phase 2 (remastered HD art, chosen in the launcher next to the original graphics) is a
separate project and is out of scope here.

## Intent

Street Rod Enhanced currently builds for Windows (MSYS2), and upstream publishes Windows releases. The goal is a native Apple Silicon Mac
version that anyone can download from GitHub Releases and play without installing developer tools, Homebrew or
the game files by hand. It follows the spirit of the owner's NFS macOS ports (`../NFSV-self-contained`), but is
simpler: our code contains nothing copyrighted and the game is a free official download, so players get a
ready-made app instead of a builder that compiles on their Mac.

Success: a person with a fresh Apple Silicon Mac downloads the DMG, opens the app (after the usual
unsigned-app steps), clicks one button to fetch the game, presses Play and gets the same game, enhanced road
view and launcher options as on Windows.

## Goals

* `Street Rod Enhanced.app` for arm64 macOS, self-contained: SDL3, wxWidgets and everything else they need live
  inside the bundle. No Homebrew, Xcode or Python on the player's Mac.
* Shipped as `Street Rod Enhanced.dmg` on GitHub Releases, with a background that explains how to open an
  unsigned app the first time (as the NFSV DMG does, `../NFSV-self-contained/builder/dmg`).
* First launch: the launcher offers to download the official game files and installs them.
* Saved games, the hall of fame and the launcher settings live in the user's Library, never inside the bundle.
* Mac-native behaviour in the launcher and the game (see below).
* The Windows build keeps building and behaving exactly as now.
* Version 0.3.0.

## Non-goals

* Intel Macs or universal binaries.
* Apple Developer ID signing or notarization (see Constraints).
* Any change to gameplay, the enhanced view, the faithful engine (`ENGINE.md` rules still apply) or the
  launcher's options.
* HD / remastered graphics (phase 2).
* A separate builder app.

## Anti-goals

* Redistributing the game files ourselves, in the DMG, the repo or a mirror.
* The app writing into its own bundle, or needing to be run from a writable folder.
* Breaking the Windows build or changing Windows file locations.
* Putting the owner's real name, email or any other personal data anywhere in the repo, commits, app metadata
  (bundle id, copyright string, About box) or network requests.

## Constraints

* **Identity.** The remote is `github.com/007hacky007/streetrod-enhanced` (public). Before the first commit, set
  this repo's git config to `user.name "007hacky007"`, `user.email "007hacky007@users.noreply.github.com"`,
  `commit.gpgsign false`. Without it, the global git config's real name would be used; it must not reach any
  commit. Commit
  messages: one short imperative line, no body unless needed, no trailers, no AI attribution.
* **No Apple signing identity.** A Developer ID would expose the owner's legal name and link it to the
  pseudonym. The app and its libraries are ad-hoc signed only (required for arm64 code to run; re-sign after
  any `install_name_tool` change).
* **HTTP client identity.** The downloader's User-Agent is a generic `StreetRodEnhanced/<version>`; nothing
  personal.
* **Text style.** Docs use plain hyphens only, no Unicode dashes.
* Existing toolchain: C11 + SDL3 for the game, C++17 + wxWidgets 3.2 for the launcher, CMake 3.24+.

## Decided

### The game files

* Source: the official page https://www.streetrodonline.com/downloads/, direct links (checked 2026-11-09, no
  login or JavaScript):

  | File | URL | Size | SHA-256 |
  |---|---|---|---|
  | Street Rod 1, PC | https://www.streetrodonline.com/wp-content/uploads/2017/05/sr1pc.zip | 415994 | `74bbf38bea126cdfd6085e44827e9c193f9f27829619f38f8dbfe353ac7354d9` |
  | Street Rod SE, PC | https://www.streetrodonline.com/wp-content/uploads/2017/05/srsepc.zip | 568242 | `0e91fefdbc5dea95291da05480b6f9647b7caa35d5f29a18223f0c0edf18cb12` |

  `sr1pc.zip` holds `SR1/` (SR.EXE, LIB1, LIB1C, LIB2, HOT_DATA, HALL_DAT, SR.COD, SR.FDL, VERSION.BAT);
  `srsepc.zip` holds `SRSE/` (SRSE.EXE, LIB1, LIB1C, LIB2, LIB2C, HOT_DATA, README). Both contain `__MACOSX/`
  and `.DS_Store` junk to skip.
* Install layout, matching what the launcher and game already expect:
  `~/Library/Application Support/Street Rod Enhanced/Game/` gets the contents of `SR1/`, and
  `.../Game/datadisk/` the contents of `SRSE/`.
* The launcher verifies each zip's SHA-256 before unpacking. On a mismatch or a network failure it says so
  plainly and offers to retry or to choose a folder instead. It never half-installs: unpack to a temporary
  folder, then move into place.
* Existing files are never overwritten: if `Game/` already has the files (or saved games), the download is not
  offered.
* "Choose folder..." stays available for players with their own copy.

### Where things live on macOS

* Settings: `~/Library/Application Support/Street Rod Enhanced/settings.ini`.
* Default game folder: `~/Library/Application Support/Street Rod Enhanced/Game` (saved games and the hall of
  fame are written there by the game, as today).
* Default program: the game executable inside the bundle.
* Windows and Linux paths unchanged.

### Mac behaviour

* Cmd+Q quits (launcher and game).
* In the game: Ctrl+Cmd+F and the green window button toggle full screen; Alt+Enter (Option+Return) keeps
  working.
* Mac shortcuts must not reach the game as key presses (the game uses Ctrl to toggle sound).
* Retina: the game window renders at the display's real pixel density, so the enhanced view is sharp.
* The launcher uses the standard Mac menu bar: About and Preferences in the app menu, Game settings as its
  own menu, as wxWidgets maps them.
* Controls unchanged. Mac keyboards lack Ins and a keypad; Space already covers click and shift, and the
  README's Mac section says so.
* Gamepads as today (SDL3).

### Releases

* A GitHub Actions workflow on an Apple Silicon runner builds the DMG when a `v*` tag is pushed and attaches
  it to a **draft** release. The owner publishes it by hand.
* The workflow must not depend on the runner's Homebrew bottles for what goes into the bundle (they target the
  runner's macOS version); build the bundled libraries with an explicit deployment target.

* The builder pushes to `origin` (under the pseudonym identity) and runs the workflow for real: a test tag or
  a `workflow_dispatch` trigger, producing a draft release that is downloaded and checked. Test tags and draft
  releases that are not the final 0.3.0 are deleted afterwards.
* This repo is the owner's own fork of `kylofon/streetrod-enhanced`; it is developed independently, not sent
  upstream. Releases, tags and README links point at `github.com/007hacky007/streetrod-enhanced`. Changes do
  not need to be upstream-friendly, and 0.3.0 is this fork's version.

### Authorship and credits

Upstream is MIT licensed (`LICENSE`, "Copyright (c) 2026 Krzysztof Kania"). MIT allows modifying and
redistributing, on the condition that the copyright and permission notice stay in all copies. So:

* `LICENSE` keeps Kania's copyright line and gains `Copyright (c) 2026 007hacky007` above it. The LICENSE file
  ships inside the app bundle and on the DMG.
* The author becomes 007hacky007 everywhere the launcher shows or stores it, on all platforms: the About box
  (`launcher/launcher.cpp`, both the Windows and the non-Windows variants), `SetVendorName` (`launcher/app.cpp`),
  `CompanyName` (`launcher/app.rc`), the manifest identity (`launcher/app.manifest`), and the Mac bundle's
  `NSHumanReadableCopyright`.
* The About box's Website link points at the GitHub repo. The donation link and the README's Support section are
  removed.
* Bundle id: `io.github.007hacky007.streetrod-enhanced`.
* `ENGINE.md`'s reference to the upstream reverse-engineering repository stays: it is the code's provenance.

### Docs

* README: a short macOS section (download, first open, first launch download, where saves live, keys).
* `launcher/README.md`: the macOS paths and the download step.
* CHANGELOG: 0.3.0 entry.

## My calls (made without asking; easy to revisit)

* Prebuilt app with a first-launch download, instead of a separate builder app (the owner agreed to this
  approach; the details below are mine).
* Keep the wxWidgets launcher on macOS, so Windows and Mac share one launcher, rather than a native Cocoa
  launcher like NFSV's.
* Minimum macOS 12, as NFSV.
* Download and unzip with what wxWidgets 3.2 already has (`wxWebRequest`, `wxZipInputStream`), so no new
  dependency. The builder may pick otherwise if these fall short.
* The launcher stays open after Play, as on Windows.
* Draft releases from CI rather than publishing automatically.
* The release workflow builds the Mac DMG only. Windows binaries are not added to this fork's releases in this
  phase; the Windows build must still work from source.
* `LICENSE`'s last paragraph names Test Drive (c) 1987 Accolade, a leftover from another project. It is corrected
  to Street Rod (c) 1989 California Dreams.
* Removing the donation link also from the Windows launcher, since the About box text is shared.

## Left to the builder

* Bundle layout (where the game executable and the libraries sit inside `Contents/`), static vs bundled
  dynamic wxWidgets, and how the bundle is assembled (CMake `MACOSX_BUNDLE`, a script, or both).
* How the game process shows in the Dock. Requirement: a sensible name and the app's icon, not a generic
  terminal-style icon or a stray second app the player can't make sense of.
* The `.icns` icon, from the existing night-road icon (`launcher/make_icon.py`, `launcher/icon.cpp`).
* How Cmd+Q and Ctrl+Cmd+F are wired in SDL3 and kept away from the game's keyboard.
* The DMG tooling (reuse or adapt NFSV's `builder/dmg`).
* How the launcher's first-launch prompt looks, within the existing window's style.
* Testing approach. At minimum, on this Mac: a clean build; `--check` passes against the downloaded files;
  the demo runs; a race and a drive through town run with the enhanced view; the data disk cars load; a
  fresh-user run (empty Application Support) goes from first launch through the download to Play; the app
  runs when copied to another folder; the Windows build is not affected (CMake logic reviewed, Windows-only
  branches untouched).
