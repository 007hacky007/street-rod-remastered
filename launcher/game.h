// game.h -- what a game folder holds (Street Rod, or the Street Rod SE data disk in its datadisk
// folder), where streetrod-enhanced is, and starting it.
#pragma once

#include <wx/string.h>

#include "gameargs.h"

// A game folder as the launcher sees it.
struct GameFolder {
    wxString missing;       // the first of the game's files that isn't there, empty if all are
    bool se = false;        // the program is SRSE.EXE (Street Rod SE, the data-disk edition)
    int saves = 0;          // saved games (HOTROD1..15.SAV)
    wxString dataDisk;      // <folder>/datadisk when it holds a complete Street Rod SE, else empty
};

GameFolder ReadGameFolder(const wxString& dir);

// The folder the launcher runs from.
wxString LauncherDir();

// Where things are by default: "Game" and streetrod-enhanced(.exe) beside the launcher.
wxString DefaultGameDir();
wxString DefaultProgram();

// True if `path` is a DOS program (MZ without a Windows PE header), such as the original SR.EXE: Windows
// can't start it, and it isn't the port.
bool IsDosProgram(const wxString& path);

// The remaster's art pack: Remaster in the settings folder (macOS: Application Support) or, on Windows,
// beside the launcher. IsArtPack: dir holds a pack.json.
wxString DefaultRemasterDir();
bool IsArtPack(const wxString& dir);

// Starts the game. On failure returns false and says why in `error`.
bool LaunchGame(const GameOptions& options, wxString& error);
