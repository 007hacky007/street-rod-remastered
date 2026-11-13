// test_update.cpp -- the launcher's own updates: the manifest, the version rules, the Windows folder swap and
// the macOS bundle swap (launcher/update.cpp).
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/utils.h>
#include <wx/zipstrm.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "sha256.h"
#include "update.h"

namespace {

int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            failures++;                                                                \
        }                                                                              \
    } while (0)

const char* const ABC_SHA = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
const char* const HELLO_SHA = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";

wxString GoodManifest(const char* eol = "\n") {
    return wxString::Format("version=0.6.0%sformat=1%smac.sha256=%s%smac.size=3%swindows.sha256=%s%s"
                            "windows.size=5%s",
                            eol, eol, ABC_SHA, eol, eol, HELLO_SHA, eol, eol);
}

wxString TempDir() {
    wxString path = wxFileName::CreateTempFileName("sre-update");
    wxRemoveFile(path);
    wxFileName::Mkdir(path, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return path;
}

wxString Sep() { return wxString(wxFILE_SEP_PATH); }

void WriteFile(const wxString& path, const std::string& content) {
    wxFileName::Mkdir(wxFileName(path).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    wxFFile f(path, "wb");
    f.Write(content.data(), content.size());
}

std::string ReadFile(const wxString& path) {
    wxFFile f(path, "rb");
    if (!f.IsOpened()) return "<missing>";
    std::string s(static_cast<size_t>(f.Length()), '\0');
    f.Read(&s[0], s.size());
    return s;
}

bool Exists(const wxString& path) { return wxFileName::FileExists(path) || wxFileName::DirExists(path); }

// Names ending in "/" are folder entries.
wxMemoryBuffer Zip(const std::vector<std::pair<std::string, std::string>>& entries) {
    wxMemoryOutputStream mem;
    {
        wxZipOutputStream zip(mem);
        for (const auto& e : entries) {
            if (!e.first.empty() && e.first.back() == '/') {
                zip.PutNextDirEntry(wxString::FromUTF8(e.first));
                continue;
            }
            zip.PutNextEntry(wxString::FromUTF8(e.first));
            zip.Write(e.second.data(), e.second.size());
        }
    }
    wxMemoryBuffer buf;
    buf.AppendData(mem.GetOutputStreamBuffer()->GetBufferStart(), mem.GetSize());
    return buf;
}

update::Download DownloadOf(const wxMemoryBuffer& data) {
    update::Download d;
    d.size = data.GetDataLen();
    d.sha256 = Sha256Hex(data.GetData(), data.GetDataLen());
    return d;
}

update::Manifest Latest(const char* version) {
    update::Manifest m;
    m.version = version;
    m.format = 1;
    return m;
}

void parses_manifest() {
    for (const char* eol : {"\n", "\r\n"}) {
        update::Manifest m;
        wxString error;
        CHECK(update::ParseManifest(GoodManifest(eol), m, error));
        CHECK(m.version == "0.6.0");
        CHECK(m.format == 1);
        CHECK(m.mac.sha256 == ABC_SHA);
        CHECK(m.mac.size == 3);
        CHECK(m.windows.sha256 == HELLO_SHA);
        CHECK(m.windows.size == 5);
    }
}

void rejects_bad_manifest() {
    const wxString good = GoodManifest();
    wxString missingSize = good;
    missingSize.Replace("windows.size=5\n", "");
    wxString shortHash = good;
    shortHash.Replace(wxString(ABC_SHA), wxString(ABC_SHA).Left(63));
    wxString twoNumbers = good;
    twoNumbers.Replace("version=0.6.0", "version=0.6");
    wxString noFormat = good;
    noFormat.Replace("format=1", "format=0");
    for (const wxString& text : {missingSize, shortHash, twoNumbers, noFormat}) {
        update::Manifest m;
        wxString error;
        CHECK(!update::ParseManifest(text, m, error));
        CHECK(!error.empty());
    }
}

void compares_versions() {
    CHECK(update::IsNewer("0.6.0", "0.5.0"));
    CHECK(update::IsNewer("0.10.0", "0.9.9"));
    CHECK(update::IsNewer("1.0.0", "0.99.99"));
    CHECK(update::IsNewer("0.5.1", "0.5.0"));
    CHECK(!update::IsNewer("0.5.0", "0.5.0"));
    CHECK(!update::IsNewer("0.4.2", "0.5.0"));
    CHECK(!update::IsNewer("0.6", "0.5.0"));
    CHECK(!update::IsNewer("v0.6.0", "0.5.0"));
    CHECK(!update::IsNewer("0.6.0-beta", "0.5.0"));
    CHECK(!update::IsNewer("", "0.5.0"));
    CHECK(!update::IsNewer("0.6.0", ""));
}

void offers() {
    CHECK(update::ShouldOffer(Latest("0.6.0"), "0.5.0", "", false));
    CHECK(!update::ShouldOffer(Latest("0.6.0"), "0.5.0", "0.6.0", false));
    CHECK(update::ShouldOffer(Latest("0.6.0"), "0.5.0", "0.6.0", true));
    CHECK(update::ShouldOffer(Latest("0.6.1"), "0.5.0", "0.6.0", false));
    CHECK(!update::ShouldOffer(Latest("0.5.0"), "0.5.0", "", true));
    CHECK(!update::ShouldOffer(Latest("0.6"), "0.5.0", "", true));
}

void urls_and_override() {
    CHECK(update::ReleasePage("0.6.0") ==
          "https://github.com/007hacky007/street-rod-remastered/releases/tag/v0.6.0");
    wxUnsetEnv("SRE_UPDATE_BASE");
    CHECK(update::ManifestUrl() ==
          "https://github.com/007hacky007/street-rod-remastered/releases/latest/download/Street-Rod-Enhanced.txt");
    CHECK(update::DmgUrl() ==
          "https://github.com/007hacky007/street-rod-remastered/releases/latest/download/Street-Rod-Enhanced.dmg");
    wxSetEnv("SRE_UPDATE_BASE", "http://127.0.0.1:8000/");
    CHECK(update::ZipUrl() == "http://127.0.0.1:8000/Street-Rod-Enhanced-windows.zip");
    CHECK(update::ManifestUrl() == "http://127.0.0.1:8000/Street-Rod-Enhanced.txt");
    wxUnsetEnv("SRE_UPDATE_BASE");
}

void verifies_downloads() {
    wxMemoryBuffer abc;
    abc.AppendData("abc", 3);
    update::Download d;
    d.sha256 = ABC_SHA;
    d.size = 3;
    wxString error;
    CHECK(update::Verify(abc, d, error));
    update::Download wrongSize = d;
    wrongSize.size = 4;
    CHECK(!update::Verify(abc, wrongSize, error));
    CHECK(!error.empty());
    update::Download wrongHash = d;
    wrongHash.sha256 = HELLO_SHA;
    CHECK(!update::Verify(abc, wrongHash, error));
}

// An installed Windows folder: the program's files ("old") and the player's ("mine").
wxString WindowsInstall() {
    const wxString dir = TempDir();
    WriteFile(dir + Sep() + "Street Rod Enhanced.exe", "old");
    WriteFile(dir + Sep() + "a.dll", "old");
    WriteFile(dir + Sep() + "Game" + Sep() + "SAVE.OLD", "mine");
    WriteFile(dir + Sep() + "Game2" + Sep() + "x", "mine");
    WriteFile(dir + Sep() + "Remaster" + Sep() + "pack.json", "mine");
    WriteFile(dir + Sep() + "settings.ini", "mine");
    WriteFile(dir + Sep() + "notes.old", "mine");
    return dir;
}

wxMemoryBuffer WindowsZip() {
    return Zip({{"Street Rod Enhanced/", ""},
                {"Street Rod Enhanced/Street Rod Enhanced.exe", "new"},
                {"Street Rod Enhanced/a.dll", "new"},
                {"Street Rod Enhanced/b.dll", "new"},
                {"Street Rod Enhanced/Game/", ""},
                {"Street Rod Enhanced/Game2/", ""}});
}

void CheckPlayersFiles(const wxString& dir) {
    CHECK(ReadFile(dir + Sep() + "Game" + Sep() + "SAVE.OLD") == "mine");
    CHECK(ReadFile(dir + Sep() + "Game2" + Sep() + "x") == "mine");
    CHECK(ReadFile(dir + Sep() + "Remaster" + Sep() + "pack.json") == "mine");
    CHECK(ReadFile(dir + Sep() + "settings.ini") == "mine");
    CHECK(ReadFile(dir + Sep() + "notes.old") == "mine");
}

void damaged_zip_stages_nothing() {
    const wxString dir = WindowsInstall();
    const wxMemoryBuffer zip = WindowsZip();
    update::Download d = DownloadOf(zip);
    d.sha256 = HELLO_SHA;
    wxString error;
    CHECK(!update::StageFolder(dir, zip, d, error));
    CHECK(!error.empty());
    CHECK(!Exists(update::StagingDir(dir)));
    CHECK(ReadFile(dir + Sep() + "Street Rod Enhanced.exe") == "old");
}

void zip_without_launcher_stages_nothing() {
    const wxString dir = WindowsInstall();
    const wxMemoryBuffer zip = Zip({{"Street Rod Enhanced/a.dll", "new"}});
    wxString error;
    CHECK(!update::StageFolder(dir, zip, DownloadOf(zip), error));
    CHECK(!error.empty());
    CHECK(!Exists(update::StagingDir(dir)));
}

void swaps_windows_folder() {
    const wxString dir = WindowsInstall();
    const wxMemoryBuffer zip = WindowsZip();
    wxString error;
    CHECK(update::StageFolder(dir, zip, DownloadOf(zip), error));
    CHECK(ReadFile(dir + Sep() + "Street Rod Enhanced.exe") == "old");  // staged only
    CHECK(update::ApplyFolder(dir, error));
    CHECK(ReadFile(dir + Sep() + "Street Rod Enhanced.exe") == "new");
    CHECK(ReadFile(dir + Sep() + "a.dll") == "new");
    CHECK(ReadFile(dir + Sep() + "b.dll") == "new");
    CHECK(ReadFile(dir + Sep() + "Street Rod Enhanced.exe.old") == "old");
    CHECK(!Exists(update::StagingDir(dir)));
    CheckPlayersFiles(dir);

    update::CleanFolder(dir);
    CHECK(!Exists(dir + Sep() + "Street Rod Enhanced.exe.old"));
    CHECK(!Exists(dir + Sep() + "a.dll.old"));
    CHECK(!Exists(dir + Sep() + ".update-old.txt"));
    CHECK(ReadFile(dir + Sep() + "Street Rod Enhanced.exe") == "new");
    CheckPlayersFiles(dir);
}

void failed_swap_puts_everything_back() {
    const wxString dir = WindowsInstall();
    WriteFile(dir + Sep() + "b.dll" + Sep() + "inside", "mine");  // a folder where a file should go
    const wxMemoryBuffer zip = WindowsZip();
    wxString error;
    CHECK(update::StageFolder(dir, zip, DownloadOf(zip), error));
    CHECK(!update::ApplyFolder(dir, error));
    CHECK(!error.empty());
    CHECK(ReadFile(dir + Sep() + "Street Rod Enhanced.exe") == "old");
    CHECK(ReadFile(dir + Sep() + "a.dll") == "old");
    CHECK(ReadFile(dir + Sep() + "b.dll" + Sep() + "inside") == "mine");
    CHECK(!Exists(dir + Sep() + "Street Rod Enhanced.exe.old"));
    CHECK(!Exists(dir + Sep() + "a.dll.old"));
    CHECK(!Exists(dir + Sep() + ".update-old.txt"));
    CheckPlayersFiles(dir);
}

void clean_folder_removes_leftover_staging() {
    const wxString dir = WindowsInstall();
    WriteFile(update::StagingDir(dir) + Sep() + "a.dll", "new");
    update::CleanFolder(dir);
    CHECK(!Exists(update::StagingDir(dir)));
    CheckPlayersFiles(dir);
}

void writable_folder_can_update() {
    const wxString dir = TempDir();
    CHECK(update::CantUpdateFolder(dir).empty());
#ifndef __WINDOWS__
    wxFileName::Mkdir(dir + Sep() + "ro", 0555, 0);
    CHECK(!update::CantUpdateFolder(dir + Sep() + "ro").empty());
#endif
}

}  // namespace

#define RUN(t)                     \
    do {                           \
        int before = failures;     \
        t();                       \
        std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", #t); \
    } while (0)

int main(int argc, char** argv) {
    wxInitializer init(argc, argv);
    if (!init.IsOk()) return 2;
    RUN(parses_manifest);
    RUN(rejects_bad_manifest);
    RUN(compares_versions);
    RUN(offers);
    RUN(urls_and_override);
    RUN(verifies_downloads);
    RUN(damaged_zip_stages_nothing);
    RUN(zip_without_launcher_stages_nothing);
    RUN(swaps_windows_folder);
    RUN(failed_swap_puts_everything_back);
    RUN(clean_folder_removes_leftover_staging);
    RUN(writable_folder_can_update);
    std::printf(failures ? "%d failures\n" : "all ok\n", failures);
    return failures ? 1 : 0;
}
