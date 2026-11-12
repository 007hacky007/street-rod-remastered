// gameargs.h -- the options the launcher starts the game with, and its command line (wxBase only).
#pragma once

#include <vector>

#include <wx/string.h>

// The two games the launcher starts.
enum class Game { SR1 = 1, SR2 = 2 };

// The value of [Launcher] Game: 2 is Street Rod 2, anything else Street Rod.
Game GameFromSetting(int value);

// What a game is called, where it lives and which of the launcher's options it takes.
struct GameTraits {
    const char* name;       // "Street Rod", "Street Rod 2"
    const char* program;    // the port, without .exe
    const char* folder;     // its game folder's default name
    const char* section;    // the settings section of its own keys
    bool cars;              // the Street Rod SE data disk's cars
    bool autoDrive;         // the "auto" switch
    bool soundDevice;       // AdLib or the PC speaker ("adlib", "speaker")
    bool enhanced;          // the enhanced race view (--classic, --res-scale, --aa, --town)
    bool art;               // the remastered pictures (--remaster-dir)
    bool remasteredMusic;   // the band (--music remastered)
    bool gearKeys;          // the port's A / Z gear keys
    const char* pictureNote;  // the Picture box's note on what is still original, "" for none
};

const GameTraits& Traits(Game game);

// Game settings > Sounds: the volumes in percent, 0..100.
struct Sounds {
    int music = 100;        // --music-volume: the background tune
    int effects = 100;      // --sfx-volume: the effects, the engine and the garage's loop sounds
    bool remasteredMusic = true;  // --music remastered: the tune played by a small band, not the speaker
};

struct GameOptions {
    Game game = Game::SR1;
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
    bool speaker = false;   // Street Rod 2: "speaker" (the PC speaker) instead of "adlib"
    Sounds sounds;
};

// The command line for `options`: the program first, then its arguments. Options the game doesn't take
// (Traits) are left out: Street Rod 2 stops at an option it doesn't know.
std::vector<wxString> GameArgs(const GameOptions& options);
