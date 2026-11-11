// test_gamefolder.cpp -- recognising Street Rod's and Street Rod 2's game folders (launcher/gamefolder.cpp).
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/init.h>

#include <cstdio>
#include <initializer_list>

#include "gamefolder.h"

namespace {

int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            failures++;                                                                \
        }                                                                              \
    } while (0)

const char* const SR1_FILES[] = {"SR.EXE", "LIB1", "LIB2", "HOT_DATA"};
const char* const SR2_FILES[] = {"SR.EXE", "LIB1", "LIB2", "LIB3", "SR2_DATA", "SR2_END", "SR2_SHP", "MSR.SND"};

wxString TempDir() {
    wxString path = wxFileName::CreateTempFileName("sre-folder");
    wxRemoveFile(path);
    wxFileName::Mkdir(path, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return path;
}

void Touch(const wxString& dir, const wxString& name) {
    wxFFile f(dir + "/" + name, "wb");
    f.Write("x", 1);
}

template <size_t N>
void TouchAll(const wxString& dir, const char* const (&names)[N], bool lower = false, const char* skip = nullptr) {
    for (const char* n : names)
        if (!skip || wxString(n) != skip) Touch(dir, lower ? wxString(n).Lower() : wxString(n));
}

void sr1_folder_unchanged() {
    const wxString d = TempDir();
    TouchAll(d, SR1_FILES);
    Touch(d, "HOTROD1.SAV");
    Touch(d, "HOTROD15.SAV");
    wxFileName::Mkdir(d + "/datadisk");
    for (const char* n : {"SRSE.EXE", "LIB1", "LIB2", "HOT_DATA"}) Touch(d + "/datadisk", n);
    const GameFolder f = ReadGameFolder(d, Game::SR1);
    CHECK(f.missing.empty());
    CHECK(f.saves == 2);
    CHECK(!f.dataDisk.empty());
    CHECK(!f.se);
    CHECK(!f.otherGame);
}

void sr2_folder_complete() {
    const wxString d = TempDir();
    TouchAll(d, SR2_FILES);
    for (const char* n : {"HOTROD0.SAV", "HOTROD14.SAV", "HOTROD15.SAV"}) Touch(d, n);
    const GameFolder f = ReadGameFolder(d, Game::SR2);
    CHECK(f.missing.empty());
    CHECK(f.saves == 2);
    CHECK(f.dataDisk.empty());
    CHECK(!f.otherGame);
}

void sr2_missing_lib3() {
    const wxString d = TempDir();
    TouchAll(d, SR2_FILES, false, "LIB3");
    CHECK(ReadGameFolder(d, Game::SR2).missing == "LIB3");
}

void sr2_lowercase_names() {
    const wxString d = TempDir();
    TouchAll(d, SR2_FILES, true);
    CHECK(ReadGameFolder(d, Game::SR2).missing.empty());
}

void sr2_empty_folder() {
    CHECK(ReadGameFolder(TempDir(), Game::SR2).missing == "SR.EXE");
    CHECK(ReadGameFolder("", Game::SR2).missing == "SR.EXE");
}

void sr1_folder_as_sr2() {
    const wxString d = TempDir();
    TouchAll(d, SR1_FILES);
    const GameFolder f = ReadGameFolder(d, Game::SR2);
    CHECK(f.otherGame);
    CHECK(f.missing == "LIB3");
}

void sr2_folder_as_sr1() {
    const wxString d = TempDir();
    TouchAll(d, SR2_FILES);
    const GameFolder f = ReadGameFolder(d, Game::SR1);
    CHECK(f.otherGame);
    CHECK(f.missing == "HOT_DATA");
}

}  // namespace

int main(int argc, char** argv) {
    wxInitializer init(argc, argv);
#define RUN(t) do { int b = failures; t(); std::printf("%s %s\n", failures == b ? "ok  " : "FAIL", #t); } while (0)
    RUN(sr1_folder_unchanged);
    RUN(sr2_folder_complete);
    RUN(sr2_missing_lib3);
    RUN(sr2_lowercase_names);
    RUN(sr2_empty_folder);
    RUN(sr1_folder_as_sr2);
    RUN(sr2_folder_as_sr1);
    return failures != 0;
}
