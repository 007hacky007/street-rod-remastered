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

int main() {
#define RUN(t) do { int b = failures; t(); std::printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)
    RUN(args_remastered_adds_dir);
    RUN(args_original_has_no_dir);
    RUN(args_classic_remastered);
    return failures != 0;
}
