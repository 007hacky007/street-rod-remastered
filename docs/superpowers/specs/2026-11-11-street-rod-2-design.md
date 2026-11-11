# Street Rod 2 - design

## Intent

Street Rod Enhanced plays the 1989 Street Rod natively on Windows and Apple Silicon Macs, with a smooth
high-resolution race view, every picture redrawn and the tune played by a small band. The owner wants the
sequel, *Street Rod 2: The Next Generation* (California Dreams, 1991), in the same package, with the same
treatment: enhanced graphics and remastered music. A player downloads one thing, picks a game in the launcher
and plays either one.

## Decisions made by the owner

* **Port Street Rod 2, with enhanced graphics and music.**
* **Same repository if the games share a significant amount of code.** They do (see Starting point), so it
  is one repository.
* **Repository renamed to `street-rod-remastered`.** Done: `github.com/007hacky007/street-rod-remastered`,
  links updated in 62570e7. GitHub redirects the old URLs, including the v0.4.0 pack asset.
* **App names stay.** The app is still "Street Rod Enhanced"; the settings folder and saved games do not move.
* **One launcher, both games.** One Windows zip and one Mac app; the launcher has a Game choice.
* **Street Rod 2's remastered music is a band arrangement** of its own songs, note for note, played by the
  same small synthesised band as Street Rod 1, in stereo. The AdLib music stays as the Original choice.
* For Street Rod 1's art the owner accepted the takedown risk ("release it, should takedown appear, the
  repository goes down. Fair"), and confirmed the same for Street Rod 2: every SR2 picture and road-side
  shape is redrawn and shipped in the bundled pack.
* **No reference recordings.** The owner's playtest decides whether SR2 plays like the original; no DOSBox
  captures are made. Where upstream reproduced something deliberately from the disassembly (the 136 s
  silence after the first title song, `mus.c` "PORT: verify U5"), the port keeps it unless the playtest
  says otherwise, and the band follows the sequencer, so it is silent then too.

## Starting point

* **The upstream port exists.** `github.com/kylofon/street-rod-2-sdl3` (MIT, Krzysztof Kania, already named in
  our LICENSE), last commit 9a3d488, release v0.1.0: a faithful C/SDL3 port of SR2's `SR.EXE`, VGA path, the
  whole game (title, garage, shops, gas station, Bob's, all five race kinds including the new Aqueduct,
  police, endings, saves), AdLib music and effects on Nuked-OPL3 (`third_party/nuked-opl3`, LGPL-2.1+) and the
  PC speaker. It was built as a delta of the same Street Rod engine this repository uses. Its headless smoke
  test (`tools/sr2_smoke.py`, 15 paths through every screen) passed upstream. Upstream's own known open
  points: the owner playtests were never finished; the title music stops after the first song and about 136 s
  of silence follow (not yet checked against the original).
* **Checked on this Mac (2026-11-11):** `sr2port/` at 9a3d488 builds with the SDL3 in `build-deps/prefix`
  (5 harmless warnings) and runs an Aqueduct race headless (`SR_DEBUG_RACE=aqueduct`, `demo v`) to the end.
* **What is shared and what is not.** Identical or nearly: `codeptr`, `types.h`, `mem`, `vga`, `ega`, most of
  `host.c` (SR2 adds the OPL chip and audio dumps). The game code and the platform layer are the same source
  but differ throughout (other addresses and data offsets, `symbols.h`, new screens and the Aqueduct). Our
  additions are engine-independent or close to it: the host changes (frame size, Retina, Mac keys, volumes),
  `src/enhanced/` (bound to the race data, so it needs SR2's layouts), `src/remaster/` (tags, pack, colour),
  the band synthesiser, the launcher, packaging, CI and the art tools.
* **Game files.** `sr2pc.zip` from `https://www.streetrodonline.com/wp-content/uploads/2017/05/sr2pc.zip`, the
  official site, free from the copyright owner: `SR.EXE` (264 468 bytes), `LIB1`, `LIB2`, `LIB3` (and the CGA
  `LIB1C`/`LIB2C`/`LIB3C`), `SR2_DATA`, `SR2_END`, `SR2_SHP`, `MSR.SND`, six `.MUS` songs, files at the top of
  the zip (plus `__MACOSX/` entries to skip). No data disk.

## Goals

1. **Street Rod 2 runs** from this repository on Windows and Apple Silicon Macs, exactly as upstream's
   faithful port does, as a second program beside Street Rod 1's.
2. **Same comforts as Street Rod 1**: window size, full screen, sharp Retina picture, Mac keys, separate music
   and sound effects volumes.
3. **Enhanced race view** for every SR2 race kind and the drive through town: high resolution, smooth motion,
   anti-aliasing, original draw distance, the same coverage rules (messages, start light, dashboard and
   cockpit stay original), the same options (`--classic`, `--res-scale`, `--aa`, `--motion-delay`, `--town`).
4. **Remastered pictures** for SR2: every picture of `LIB1`, `LIB2` and `LIB3`, made with the same pipeline
   and rules as Street Rod 1 (`docs/superpowers/specs/2026-11-10-hd-remaster-design.md`), with art reused
   wherever a picture is byte-identical to one of Street Rod 1's.
5. **Remastered music** for SR2 as decided above, with the game choosing songs, loops and pauses as it does.
6. **One launcher** for both games, and both games in the Mac app and the Windows zip.
7. **Street Rod 1 does not change.** Same behaviour, same pictures, same files, same settings.

## Non-goals

* CGA, EGA, Tandy and Hercules graphics, and the Tandy sound (upstream parked them; VGA only, as SR1).
* The Amiga versions.
* A longer draw distance (not done for SR1 either).
* Gameplay changes or fixes of the original's own quirks.
* Linux packages.
* Merging the two games into one executable.

## Anti-goals (failure even if it works)

* Any regression in Street Rod 1.
* SR2 playing, sounding or timing differently from the original where no option asks for it.
* Moving the existing settings folder or saved games.
* The macOS username, the owner's real name or email, or AI attribution in anything committed or shipped.

## Design

### Repository layout

Two engine trees and a shared part, each game its own executable:

* Street Rod 1's engine stays where it is or moves to its own folder; Street Rod 2's engine is imported from
  upstream `sr2port/src` at 9a3d488 into its own folder, with a provenance note like `ENGINE.md`
  (`ENGINE2.md`: upstream repository, commit, the porting rules, where the specs live) and upstream's
  third-party Nuked-OPL3 with its licence.
* Shared code lives in one place and does not include either game's `symbols.h`: at least the host (one
  `host.c` with SR2's OPL and audio-dump additions folded in), the Mac key handling, the remaster core (tags,
  pack, colour) and the band synthesiser. What else can be shared (mem, vga, ega, codeptr) is the builder's
  call; sharing is wanted where the files are the same, not forced where they differ.
* Each engine keeps its `ENH:` markers for deliberate changes, as now.
* Executables: `streetrod-enhanced` (unchanged) and `streetrod2-enhanced`.

Before and after the move, Street Rod 1 must give the same result: all existing tests pass, and demo snapshots
with a fixed seed are byte-identical (`SR_SNAPSHOT_DIR`, `SR_SEED=3`, the dummy SDL drivers), with and without
the art pack and the enhanced view.

### Street Rod 2 faithful core

* Command line as Street Rod 1's where it applies (`--game-dir`, `--scale`, `--fullscreen`, `--check`, the
  volumes), plus the game's own switches. Upstream starts with text menus for video and sound unless given
  `v` and `adlib`/`speaker`; the launcher always passes them.
* Upstream's developer aids stay (`SR_TRACE`, `SR_KEYS`, `SR_DEBUG_RACE`, `SR_DEBUG_END`, `SR_SEED`,
  `SR_SNAPSHOT_*`, `SR_AUDIO_*`) and upstream's smoke paths become tests that run when SR2 game files are
  given (like `SRE_TEST_GAME_DIR` for SR1).
* Bugs found in the faithful core are fixed here, with an `ENH:`-style marker naming them as port fixes
  (`PORT:` as upstream does).

### Enhanced race view

The same approach as `ENHANCED.md` stage 1, driven by SR2's race data: the road strip from SR2's segment
table, its road modes, the Aqueduct, `SR2_SHP` road-side objects and signs, the rotated opponent sprites,
the mirror. The renderer is shared where the data matches; SR2-specific geometry and sprites are separate.
Acceptance as for SR1: with time stopped, the enhanced view scaled down to 320x200 matches the original frame
(road edges within +-1 px, objects at the same row and size), for each race kind and the town drive.

### Remastered pictures

* Dump all pictures of `LIB1`, `LIB2` and `LIB3` through SR2's engine (`--dump-pictures`), add them to the
  catalogue as a third source (`sr2`), and reuse existing art for every picture whose SHA-1 matches one of
  Street Rod 1's.
* The rest go through the pipeline exactly as SR1's did: palettes from play logs, jobs, generation in ChatGPT
  through Claude in Chrome in the owner's browser, ingest, checks, the builder's review, the owner's in-game
  review in batches, repairs.
* The pack format does not change. One pack holds both games' art. It is published as a new release asset
  and `tools/remaster/fetch-pack.sh` points at it (new URL and SHA-256); the v0.4.0 asset stays.
* The race-view shapes are in scope, as they were for Street Rod 1 (its `shape-<sha1>` entries). SR2 keeps
  them in `SR2_SHP`: 26 sets of coloured line shapes in a format other than SR1's. Sets 0..19 are the same
  objects as SR1's and reuse SR1's art by matching the drawing, not the bytes; sets 23..25 are the Aqueduct's,
  pre-rotated by roll angle, and need new art and a renderer that draws the remastered art rotated.
  (Builder's call, following the SR1 scope "everything".)
* In the game, a picture without art shows the original, as now. So SR2 can ship before its art is complete.

### Remastered music

* SR2's music is AdLib Visual Composer songs (`MSRBEG`, `MSR2`, `MSR20`, `MSR21`, `MSR24`, `PAUSE`) on the
  timbre bank `MSR.SND` (named instruments such as `piano1`, `jazzguit`), played by the game's own sequencer
  (upstream `sound/mus.c`).
* The band takes its notes from that sequencer, so song choice, tempo, loops and pauses stay the game's. The
  sequencer's note events (voice, pitch, volume, timbre) go to the band instead of the OPL chip when Music is
  Remastered. Each timbre is mapped to one of the band's instruments; percussion voices to the drums.
* With Sound set to PC speaker the game never starts a `.MUS` song; it plays its own speaker tune
  (`spk_bg_tune`, DS:5594, 14 sections) and a jingle, in the same (duration, divisor) format as Street Rod 1's
  tune. Remastered music then plays that tune through Street Rod 1's arrangement (`song_parse`, `arrange`),
  so both sound devices get the band and the game clock is never changed (it runs at 68.6 Hz while AdLib
  music plays and 72.8 Hz otherwise, upstream `sound.md` 1.4). (Builder's call.)
* Sound effects stay as they are (AdLib or speaker).
* Options: `--music original|remastered` (default original in the game, Remastered in the launcher, as SR1),
  and the sound device `adlib|speaker` as now.
* Nothing derived from the songs is committed or shipped, as for SR1: the band reads the songs from the game
  files at run time.

### Launcher

* A **Game** choice at the top: Street Rod or Street Rod 2. The rest of the window shows that game's options;
  choices that do not apply are hidden or greyed (Cars and the data disk are SR1 only; Sound AdLib/PC speaker
  is SR2 only).
* Per game: the game folder, the start switches, Music, Art. Shared: window size, full screen, Race view,
  resolution, anti-aliasing, volumes. Existing settings keys keep their meaning for Street Rod 1.
* SR2's folder: `Game2` beside the launcher on Windows; `~/Library/Application Support/Street Rod
  Enhanced/Game2` on a Mac. Recognised by `SR.EXE` plus `LIB3` and `SR2_DATA`; saved games counted from
  `HOTROD*.SAV`.
* On a Mac the launcher offers to download SR2 like SR1: the official zip, a pinned SHA-256, unpacked to
  `Game2.partial` first, `__MACOSX` skipped, a non-empty folder never written to.
* The Windows zip gets an empty `Game2\` and `Read me.txt` says where `sr2pc.zip`'s files go.

### Packaging, CI, docs, release

* Mac app and Windows zip carry both executables (and SR2's DLL needs). CI and the release workflow build and
  test both.
* README for players covering both games, with SR2 screenshots taken as SR1's were (no username in files).
  CHANGELOG, REMASTER.md, ENHANCED.md and launcher/README.md updated.
* Release 0.5.0 when SR2 plays with the enhanced view, the band and the launcher; SR2's art may still be
  partial then (pictures without art show the original). A later release ships the completed pack.
  Publishing each release is the owner's call.

## Testing

* SR1: the snapshot comparison above, the existing unit tests, CI.
* SR2: upstream's smoke paths (title, new game, the demo tour, each race kind, jail, endings) headless, no
  crash and every marker reached; `--check`.
* Unit tests for the new pure parts: `.MUS`/`.SND` parsing, the timbre-to-instrument mapping, launcher folder
  detection, defaults and the SR2 download check.
* Enhanced view: the scaled-down comparison per race kind; snapshots looked at by the builder.
* The owner playtests SR2 on a Mac (and, if possible, Windows) before a release.

## Left to the builder

* Exact folders and file names of the split, and how far sharing goes beyond the minimum above.
* The timbre-to-instrument map and how the band treats AdLib percussion mode.
* Settings keys for SR2.
* Order of work, as long as SR1 never breaks along the way.

## Open for the owner

* **Generation run:** SR2's new pictures need several days of ChatGPT image generation in the owner's
  browser, as SR1's did. The builder asks before starting it.
* **Playtests:** the owner plays SR2 before each release; this is the only check against the original.
