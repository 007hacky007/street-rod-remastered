// game.h -- where each game's folder and program are by default, and starting the game.
#pragma once

#include <wx/string.h>

#include "gameargs.h"
#include "gamefolder.h"

// The folder the launcher runs from.
wxString LauncherDir();

// Where things are by default: the game's folder (Traits: "Game", "Game2") beside the launcher, in the
// settings folder on a Mac, and its program (streetrod-enhanced(.exe), streetrod2-enhanced(.exe)) beside
// the launcher.
wxString DefaultGameDir(Game game);
wxString DefaultProgram(Game game);

// True if `path` is a DOS program (MZ without a Windows PE header), such as the original SR.EXE: Windows
// can't start it, and it isn't the port.
bool IsDosProgram(const wxString& path);

// The remaster's art pack. UserRemasterDir: Remaster in the settings folder, where the launcher downloads
// it (artpack.h). DefaultRemasterDir: that one if it holds a pack, else Remaster beside the launcher if that
// does (a pack put there by hand, or the Windows zip of 0.4.1), else UserRemasterDir.
// IsArtPack: dir holds a pack.json.
wxString UserRemasterDir();
wxString DefaultRemasterDir();
bool IsArtPack(const wxString& dir);

// Starts the game. On failure returns false and says why in `error`.
bool LaunchGame(const GameOptions& options, wxString& error);
