// gameargs.cpp -- see gameargs.h.
#include "gameargs.h"

Game GameFromSetting(int value) { return value == 2 ? Game::SR2 : Game::SR1; }

const GameTraits& Traits(Game game) {
    static const GameTraits sr1{"Street Rod", "streetrod-enhanced", "Game", "Game",
                                true, true, false, true, true, true, true, ""};
    static const GameTraits sr2{"Street Rod 2", "streetrod2-enhanced", "Game2", "Game2",
                                false, false, true, true, true, true, false,
                                "Street Rod 2: a picture the art pack has not redrawn shows the original."};
    return game == Game::SR2 ? sr2 : sr1;
}

std::vector<wxString> GameArgs(const GameOptions& o) {
    const GameTraits& t = Traits(o.game);
    std::vector<wxString> args{o.program,
                               "--game-dir", o.gameDir,
                               "--scale", wxString::Format("%d", o.scale)};
    if (o.fullscreen) args.push_back("--fullscreen");
    if (o.uncapped) args.insert(args.end(), {"--fps", "0", "--vsync", "off"});
    if (t.art && !o.remasterDir.empty()) args.insert(args.end(), {"--remaster-dir", o.remasterDir});
    if (t.enhanced) {
        if (o.classic) {
            args.push_back("--classic");
        } else {
            args.insert(args.end(), {"--res-scale", wxString::Format("%d", o.resScale),
                                     "--aa", wxString::Format("%d", o.aa), "--town", o.town ? "on" : "off"});
        }
    }
    // The volumes only when turned down, so the default command line stays as it was.
    if (o.sounds.music != 100) args.insert(args.end(), {"--music-volume", wxString::Format("%d", o.sounds.music)});
    if (o.sounds.effects != 100) args.insert(args.end(), {"--sfx-volume", wxString::Format("%d", o.sounds.effects)});
    if (t.remasteredMusic && o.sounds.remasteredMusic) args.insert(args.end(), {"--music", "remastered"});
    // Street Rod 2's start-up menus for the picture and the sound, answered: VGA, and AdLib or the speaker.
    if (t.soundDevice) args.insert(args.end(), {"v", o.speaker ? "speaker" : "adlib"});
    // The original's own command-line switches, passed on as the port receives them.
    if (o.noMouse) args.push_back("nomouse");
    if (o.demo) args.push_back("demo");
    if (t.autoDrive && o.autoDrive) args.push_back("auto");
    return args;
}
