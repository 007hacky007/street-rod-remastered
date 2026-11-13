# Launcher updates: design

## Intent

Players install Street Rod Enhanced once and then never hear about new versions. The launcher should
say when a newer version is published and update itself with one click, the way it already
downloads and updates the remastered pictures (`launcher/artpack.cpp`).

The owner's words: "show note about new version being released somewhere in the launcher and add
possibility to one-click update from within the launcher itself." The owner chose the note's place
from three mockups: a strip across the top of the launcher window.

## Goals

- When the launcher opens, it finds out whether a newer version has been published on GitHub
  (`007hacky007/street-rod-remastered`, its latest release).
- If so, a strip across the top of the main page says so:
  `Street Rod Enhanced 0.6.0 is out. What's new [Update] [Later]`.
- **Update** downloads the new version, checks it, and puts it in place of the running one. Then
  **Restart** reopens the launcher, now the new version.
- Settings, saved games, the game files and the remastered pictures are untouched.
- Works for the two things that are released: the macOS app (from the DMG) and the Windows folder
  (from the zip).

## Non-goals

- No automatic or background installs. Nothing is downloaded until the player clicks Update.
- No beta or draft channel. Only the release GitHub marks as latest counts; drafts never do.
- No delta updates. The whole DMG or zip is downloaded.
- No Linux updates. There is no Linux release, so there is no strip there.
- No updating of the art pack here. That already has its own button and offer.
- No on/off setting for the check (my call: keeps the window simple; offline costs nothing).

## Anti-goals (a failure even if it works)

- A failed or interrupted update that leaves a broken install. At every step before the final swap,
  the old version must be untouched and still start.
- Touching anything the player owns: on Windows the `Game` and `Game2` folders next to the launcher
  (game files and saved games live there), a `Remaster` folder, `settings.ini`; on macOS anything
  outside the app bundle.
- Nagging. One strip, no dialogs on start, no repeated asking after Later.
- Wordy UI. Labels stay short (owner: "We don't have to be wordy").
- Sending anything personal. Requests carry no identifying data beyond what wxWebRequest sends
  by default.

## How it looks and behaves

### The check

- On start, once, in the background (my call: the same moment the art manifest is fetched).
- The launcher fetches a small manifest from the latest release (see Release side below) and
  compares its version with `APP_VERSION_TEXT` numerically (major, minor, patch).
- Offline, an HTTP error, or a manifest the launcher can't read: nothing is shown, no error.
- A manifest asking for a newer updater format than this launcher knows: show the strip, but
  **Update** opens the release page instead of updating (my call, mirrors `artpack::FORMAT`).

### The strip

- A row above the `Game:` row, across the full width, tinted so it stands out (system info colours
  or a light blue; builder's call within wx). Shown only when a newer version is known and not
  skipped.
- Text: `Street Rod Enhanced X.Y.Z is out.` then a `What's new` hyperlink, then `Update` and
  `Later` buttons.
- `What's new` opens `https://github.com/007hacky007/street-rod-remastered/releases/tag/vX.Y.Z`
  in the browser.
- `Later` hides the strip and records the version (`[Update] Skipped=X.Y.Z` in settings.ini). The
  strip comes back only for a version newer than the skipped one.
- A menu item `Check for Updates…` (Game settings menu on macOS next to About; the same menu on
  Windows) runs the check again and shows the strip even for a skipped version. If there is no
  newer version, it says so in a plain message box ("Street Rod Enhanced 0.5.0 is the latest
  version."), and if offline, that the check failed. (My call: without it, Later is a dead end.)
- The window refits when the strip shows or hides.

### Updating

- Click **Update**: the buttons hide and the strip text becomes
  `Downloading 0.6.0… 12 of 31 MB` (updated a few times a second, as the art download does).
  A `Cancel` button stays (my call).
- Play, the settings and everything else stay usable during the download (my call: the swap only
  happens on Restart, so playing meanwhile is safe).
- The download is checked against the manifest's size and SHA-256, then unpacked and staged next
  to the install (details per platform below). Any failure: the strip says
  `The update failed: <reason>` with `Try Again` and `Later`. The installed version is untouched.
- When staged: `0.6.0 is ready.` with a `Restart` button. Restart swaps the staged version in,
  starts the new launcher and closes this one. Closing the launcher instead of restarting leaves
  the staged copy; the next start of the old launcher may offer it again from scratch (builder's
  call whether to reuse the staged copy; discarding it is fine).
- A game started from the launcher may still be running during the swap. Both platforms allow
  renaming a running program, so this is not blocked; the old files are cleaned up on a later start.
- Closing the launcher mid-download cancels the download, as it does for the art.

### Where updating can't work

The strip still shows, but Update explains instead of downloading, in one short message box, and
offers to open the release page:

- macOS: the app runs from the disk image, from a translocated path (`/AppTranslocation/` in the
  bundle path), or its folder is not writable. Message: move Street Rod Enhanced to Applications
  first (translocation or disk image), or that the folder can't be written.
- Windows: the launcher's folder is not writable (for example under Program Files).
- A developer build not inside a packaged app (macOS: not running from a `.app` bundle).

## Platforms

### macOS

- Download `Street-Rod-Enhanced.dmg` from the latest release.
- Attach it read-only and hidden (`hdiutil attach -nobrowse -readonly -noautoopen -mountpoint <tmp>`),
  copy `Street Rod Enhanced.app` out with `ditto` to a staging name in the same folder as the
  running bundle (for example `.Street Rod Enhanced.app.update`), detach.
- Check the staged bundle: `codesign --verify --strict` passes and its `Info.plist`
  `CFBundleShortVersionString` equals the manifest version.
- Restart: rename the running bundle to a backup name, rename the staged bundle to the original
  name, start it (`open -n <path>`), quit. On start, a launcher removes a leftover backup and a
  leftover staging bundle next to itself. If the second rename fails, rename the backup back.
- The app is ad-hoc signed. Files fetched by the launcher itself carry no quarantine flag, so
  Gatekeeper should not block the new copy. The builder confirms this on a real install.
- If macOS stops the swap (the App Management privacy permission on macOS 13 and later, or
  Gatekeeper refusing the new copy): a one-time system prompt the player can allow is acceptable.
  If the swap is refused anyway, roll back and treat it as "can't update here": a short message and
  the release page. No paid Developer ID signing for this feature. (My call: the fallback path
  exists already, so a refusal costs one manual update, not a broken install.)

### Windows

- Download `Street-Rod-Enhanced-windows.zip` from the latest release. Its top folder is
  `Street Rod Enhanced/`.
- Unpack into a staging folder inside the launcher's folder (for example `.update\`).
- Check the staged `Street Rod Enhanced.exe` exists, and every file in the zip is a plain file
  under the top folder (no absolute paths, no `..`).
- Restart: for each staged file, rename the installed one (if any) to `<name>.old`, move the new
  one in. Only files the zip contains are touched; the empty `Game` and `Game2` folders in the zip
  are not recreated or emptied. Start the new `Street Rod Enhanced.exe`, quit. On start, a launcher
  deletes `*.old` files and a leftover `.update` folder next to itself, ignoring files it can't
  delete yet (a game still running).
- If a rename fails midway, put back what was renamed so far.

## Release side

- `release.yml` also attaches a manifest `Street-Rod-Enhanced.txt` (my call: mirrors the art
  pack's `Street-Rod-Remastered-art.txt`, key=value lines):
  - `version=0.6.0`
  - `format=1`
  - `mac.sha256=...`, `mac.size=...` (the DMG)
  - `windows.sha256=...`, `windows.size=...` (the zip)
- The launcher reads `https://github.com/007hacky007/street-rod-remastered/releases/latest/download/Street-Rod-Enhanced.txt`
  and the DMG or zip from the same `latest/download/` path. An `SRE_UPDATE_BASE` environment
  variable replaces that base, like `SRE_ART_BASE` does (a developer aid for tests).
- The updater ships in 0.5.0 (owner's decision), which waits for it. The 0.5.0 release is the
  first to carry the manifest, so no updating launcher ever sees a latest release without one.
  Releases before it (0.4.2 and earlier) have no manifest; a launcher seeing "latest" without one
  shows nothing. Built on master, where releases are cut.
- The draft-then-publish flow stays: the manifest is in the draft and becomes visible when the owner
  publishes.

## Constraints

- C++ and wxWidgets, as in the rest of `launcher/`. Reuse `WebFetch` (`launcher/download.h`),
  `sha256.h`, and the zip unpacking `artpack::Install` uses.
- The logic that does not need a window (manifest parsing, version comparison, the skip rule, the
  Windows file swap and its roll-back, path checks) lives in its own file (for example
  `launcher/update.cpp`) with unit tests, like `artpack`.
- No new third-party libraries. macOS uses the system's `hdiutil`, `ditto`, `codesign`, `open`.
- UI wording is short. No dashes other than the hyphen-minus in any text.
- No personal data in requests, code or docs.
- The SR1 and SR2 game programs are not changed.

## Left to the builder

- The strip's exact colours and spacing in wx on both platforms.
- Whether the download is held in memory (as the art zip is) or streamed to a file. Around 16 MB
  on Windows and 5 MB on macOS today, so memory is acceptable.
- Exact staging and backup names, and how the leftover clean-up is ordered.
- Test approach for the macOS swap (a script test with a fake bundle is enough; the real DMG path
  is checked by hand).
