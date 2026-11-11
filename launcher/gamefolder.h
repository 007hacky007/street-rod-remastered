// gamefolder.h -- what a game folder holds: Street Rod (and the Street Rod SE data disk in its datadisk
// folder) or Street Rod 2 (wxBase only).
#pragma once

#include <wx/string.h>

#include "gameargs.h"

// A game folder as the launcher sees it.
struct GameFolder {
    wxString missing;       // the first of the game's files that isn't there, empty if all are
    bool se = false;        // Street Rod: the program is SRSE.EXE (Street Rod SE, the data-disk edition)
    int saves = 0;          // saved games (Street Rod HOTROD1..15.SAV, Street Rod 2 HOTROD0..14.SAV)
    wxString dataDisk;      // Street Rod: <folder>/datadisk when it holds a complete Street Rod SE, else empty
    bool otherGame = false; // the folder holds the other game
};

GameFolder ReadGameFolder(const wxString& dir, Game game);
