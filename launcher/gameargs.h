// gameargs.h -- the options the launcher starts the game with, and its command line (wxBase only).
#pragma once

#include <vector>

#include <wx/string.h>

// Game settings > Sounds: the volumes in percent, 0..100.
struct Sounds {
    int music = 100;        // --music-volume: the background tune
    int effects = 100;      // --sfx-volume: the effects, the engine and the garage's loop sounds
};

struct GameOptions {
    wxString program;       // streetrod-enhanced(.exe)
    wxString gameDir;       // --game-dir
    bool demo = false;      // the game's "demo" switch: it plays itself
    bool autoDrive = false; // the game's "auto" switch: races drive themselves
    bool noMouse = false;   // the game's "nomouse" switch
    int scale = 3;          // --scale: the window is 320x240 times this
    bool fullscreen = false;  // --fullscreen
    bool classic = false;   // --classic: the original's picture only
    int resScale = 4;       // --res-scale: the picture is 320x200 times this
    int aa = 2;             // --aa: anti-aliasing, aa x aa samples per pixel
    bool town = true;       // --town on|off: the drives through town enhanced too
    wxString remasterDir;   // --remaster-dir: the remastered art pack, empty = the original art
    Sounds sounds;
};

// The command line for `options`: the program first, then its arguments.
std::vector<wxString> GameArgs(const GameOptions& options);
