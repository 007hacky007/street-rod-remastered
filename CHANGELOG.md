# Changelog

## 0.4.2

- The remastered pictures are no longer inside the downloads: the launcher offers to download them (26 MB)
  from their own repository, street-rod-remastered-art, on its first start or with **Download Pictures...**
  next to Art, and offers newer ones when they are out. They are kept in the settings folder.
- The repository is now called street-rod-remastered.

## 0.4.1

- The remastered pictures come with the download: in the Windows zip and inside the Mac app. No separate
  art pack to install.
- Launcher: Art defaults to Remastered when the pack is there.

## 0.4.0

- HD remaster: with an art pack, every picture can be shown redrawn as pixel art at four times the resolution
  (`--remaster-dir`, launcher: Art). Painting, fades and colour cycling recolour the redrawn art; the race view
  uses the redrawn pictures and road-side shapes. The art pack is a separate download on the Releases page.
- Remastered music: the game's own tune played by a small synthesised band in stereo (`--music remastered`,
  launcher: Music in Options, default Remastered). Sound effects stay as they were.
- Launcher: the Graphics choice is now called Race view.
- Tools for building the art pack in `tools/remaster/`.
- Windows zip, built by the release workflow with the Mac disk image.
- CI: every push and pull request builds and tests the Mac app and the Windows zip.
- README rewritten for players, with screenshots.

## 0.3.0

- macOS app for Apple Silicon (M1 or later, macOS 12 or newer), on a disk image from the Releases page.
- On a Mac the launcher downloads the game from streetrodonline.com, the official site, on first launch (Street
  Rod and the Street Rod SE data disk), checks it and keeps it with the saved games in
  `~/Library/Application Support/Street Rod Enhanced`.
- Sharp picture on Retina screens.
- Mac keys: Cmd+Q quits, Ctrl+Cmd+F or the green window button switches full screen, and Cmd shortcuts no
  longer reach the game. On a Mac, Ctrl turns the sound on or off when it is let go.
- The launcher's About box credits 007hacky007 and links to this repository.

## 0.2.0

- Separate music and sound effects volumes (`--music-volume`, `--sfx-volume`).
- Launcher: menus (File > Preferences with Always on top, Game settings > Sounds, About), as in Aces of the Pacific Enhanced.
- Launcher: Game settings > Sounds, with sliders for the music and sound effects volumes.

## 0.1.0

- Project started from the faithful Street Rod port, with its launcher renamed "Street Rod Enhanced".
- Enhanced road view in races and drives through town, redrawn smoothly at the display's rate (`--motion-delay`).
- High resolution (`--res-scale`) with anti-aliasing (`--aa`).
- Redrawn: the road, kerbs, sidewalks, sky, horizon, roadside objects, signs, banners, town walls, the opponent,
  the police car and the rear-view mirror.
- `--classic` shows the original picture only; `--town off` leaves the drives through town original.
- Launcher: a Picture box; the Program field refuses a DOS program such as `SR.EXE`.
