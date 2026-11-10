// gameargs.cpp -- see gameargs.h.
#include "gameargs.h"

std::vector<wxString> GameArgs(const GameOptions& o) {
    std::vector<wxString> args{o.program,
                               "--game-dir", o.gameDir,
                               "--scale", wxString::Format("%d", o.scale)};
    if (o.fullscreen) args.push_back("--fullscreen");
    if (!o.remasterDir.empty()) args.insert(args.end(), {"--remaster-dir", o.remasterDir});
    if (o.classic) {
        args.push_back("--classic");
    } else {
        args.insert(args.end(), {"--res-scale", wxString::Format("%d", o.resScale),
                                 "--aa", wxString::Format("%d", o.aa), "--town", o.town ? "on" : "off"});
    }
    // The volumes only when turned down, so the default command line stays as it was.
    if (o.sounds.music != 100) args.insert(args.end(), {"--music-volume", wxString::Format("%d", o.sounds.music)});
    if (o.sounds.effects != 100) args.insert(args.end(), {"--sfx-volume", wxString::Format("%d", o.sounds.effects)});
    // The original's own command-line switches, passed on as the port receives them.
    if (o.noMouse) args.push_back("nomouse");
    if (o.demo) args.push_back("demo");
    if (o.autoDrive) args.push_back("auto");
    return args;
}
