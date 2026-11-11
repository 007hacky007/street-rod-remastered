// test_args.cpp -- the launcher's command line for the game (launcher/gameargs.cpp).
#include <algorithm>
#include <cstdio>

#include "gameargs.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool has(const std::vector<wxString>& a, const wxString& x) { return std::find(a.begin(), a.end(), x) != a.end(); }

static void args_remastered_adds_dir() {
    GameOptions o;
    o.program = "/p/streetrod-enhanced";
    o.gameDir = "/g";
    o.remasterDir = "/art/Remaster";
    auto a = GameArgs(o);
    auto it = std::find(a.begin(), a.end(), wxString("--remaster-dir"));
    CHECK(it != a.end() && it + 1 != a.end() && *(it + 1) == "/art/Remaster");
    CHECK(a.front() == "/p/streetrod-enhanced");
}

static void args_original_has_no_dir() {
    GameOptions o;
    o.program = "/p/streetrod-enhanced";
    o.gameDir = "/g";
    auto a = GameArgs(o);
    CHECK(!has(a, "--remaster-dir"));
    CHECK(has(a, "--res-scale") && !has(a, "--classic"));
}

static void args_classic_remastered() {
    GameOptions o;
    o.program = "/p/streetrod-enhanced";
    o.gameDir = "/g";
    o.classic = true;
    o.remasterDir = "/art";
    auto a = GameArgs(o);
    CHECK(has(a, "--classic") && has(a, "--remaster-dir") && !has(a, "--res-scale"));
}

static void args_music_remastered() {
    GameOptions o;
    o.program = "/p/streetrod-enhanced";
    o.gameDir = "/g";
    auto a = GameArgs(o);                          // the default: the band
    auto it = std::find(a.begin(), a.end(), wxString("--music"));
    CHECK(it != a.end() && it + 1 != a.end() && *(it + 1) == "remastered");
    o.sounds.remasteredMusic = false;
    CHECK(!has(GameArgs(o), "--music"));
}

static void args_sr1_default_pinned() {
    GameOptions o;
    o.program = "/p/streetrod-enhanced";
    o.gameDir = "/g";
    const std::vector<wxString> want{"/p/streetrod-enhanced", "--game-dir", "/g", "--scale", "3", "--res-scale", "4",
                                     "--aa", "2", "--town", "on", "--music", "remastered"};
    CHECK(GameArgs(o) == want);
}

static bool before(const std::vector<wxString>& a, const wxString& x, const wxString& y) {
    auto i = std::find(a.begin(), a.end(), x), j = std::find(a.begin(), a.end(), y);
    return i != a.end() && j != a.end() && i < j;
}

static bool followed_by(const std::vector<wxString>& a, const wxString& x, const wxString& y) {
    auto i = std::find(a.begin(), a.end(), x);
    return i != a.end() && i + 1 != a.end() && *(i + 1) == y;
}

static void traits_table() {
    const GameTraits& a = Traits(Game::SR1);
    CHECK(wxString(a.name) == "Street Rod");
    CHECK(wxString(a.program) == "streetrod-enhanced");
    CHECK(wxString(a.folder) == "Game");
    CHECK(wxString(a.section) == "Game");
    CHECK(a.cars && a.autoDrive && a.enhanced && a.art && a.remasteredMusic && a.gearKeys && !a.soundDevice);
    const GameTraits& b = Traits(Game::SR2);
    CHECK(wxString(b.name) == "Street Rod 2");
    CHECK(wxString(b.program) == "streetrod2-enhanced");
    CHECK(wxString(b.folder) == "Game2");
    CHECK(wxString(b.section) == "Game2");
    CHECK(b.soundDevice && b.remasteredMusic && b.enhanced && !b.cars && !b.autoDrive && !b.art && !b.gearKeys);
}

static void game_from_setting() {
    CHECK(GameFromSetting(2) == Game::SR2);
    for (int v : {1, 0, 3, -1}) CHECK(GameFromSetting(v) == Game::SR1);
}

static void args_sr2_minimal() {
    GameOptions o;
    o.game = Game::SR2;
    o.program = "/p/streetrod2-enhanced";
    o.gameDir = "/g2";
    o.remasterDir = "/art";
    o.autoDrive = true;
    o.classic = false;
    o.sounds.remasteredMusic = true;
    const std::vector<wxString> want{"/p/streetrod2-enhanced", "--game-dir", "/g2", "--scale", "3", "--res-scale", "4",
                                     "--aa", "2", "--town", "on", "--music", "remastered", "v", "adlib"};
    CHECK(GameArgs(o) == want);
}

static void args_sr2_speaker_switches_volumes() {
    GameOptions o;
    o.game = Game::SR2;
    o.program = "/p/streetrod2-enhanced";
    o.gameDir = "/g2";
    o.speaker = true;
    o.demo = true;
    o.noMouse = true;
    o.fullscreen = true;
    o.sounds.music = 40;
    o.sounds.effects = 0;
    auto a = GameArgs(o);
    CHECK(has(a, "--fullscreen"));
    CHECK(followed_by(a, "--music-volume", "40"));
    CHECK(followed_by(a, "--sfx-volume", "0"));
    CHECK(has(a, "speaker") && has(a, "nomouse") && has(a, "demo"));
    CHECK(!has(a, "adlib") && !has(a, "auto"));
    CHECK(before(a, "v", "speaker"));
}

static void args_sr2_classic() {
    GameOptions o;
    o.game = Game::SR2;
    o.program = "/p/streetrod2-enhanced";
    o.gameDir = "/g2";
    o.classic = true;
    o.remasterDir = "/art";
    auto a = GameArgs(o);
    CHECK(has(a, "--classic") && !has(a, "--res-scale") && !has(a, "--remaster-dir"));
}

static void args_sr2_original_music() {
    GameOptions o;
    o.game = Game::SR2;
    o.program = "/p/streetrod2-enhanced";
    o.gameDir = "/g2";
    o.sounds.remasteredMusic = false;
    CHECK(!has(GameArgs(o), "--music"));
    CHECK(has(GameArgs(o), "adlib"));
}

int main() {
#define RUN(t) do { int b = failures; t(); std::printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)
    RUN(args_remastered_adds_dir);
    RUN(args_original_has_no_dir);
    RUN(args_classic_remastered);
    RUN(args_music_remastered);
    RUN(args_sr1_default_pinned);
    RUN(traits_table);
    RUN(game_from_setting);
    RUN(args_sr2_minimal);
    RUN(args_sr2_speaker_switches_volumes);
    RUN(args_sr2_original_music);
    RUN(args_sr2_classic);
    return failures != 0;
}
