// test_gamefiles.cpp -- the checksum and the installer of the game files (launcher/gamefiles.cpp).
#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/utils.h>
#include <wx/zipstrm.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "gamefiles.h"
#include "sha256.h"

namespace {

int failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            failures++;                                                                \
        }                                                                              \
    } while (0)

// A fresh, empty temporary folder; the game folder used by a test is <it>/game.
wxString TempDir() {
    wxString path = wxFileName::CreateTempFileName("sre-test");
    wxRemoveFile(path);
    wxFileName::Mkdir(path, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return path;
}

void WriteFile(const wxString& path, const std::string& content) {
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

wxMemoryBuffer Zip(const std::vector<std::pair<std::string, std::string>>& entries) {
    wxMemoryOutputStream mem;
    {
        wxZipOutputStream zip(mem);
        for (const auto& e : entries) {
            zip.PutNextEntry(wxString::FromUTF8(e.first));
            zip.Write(e.second.data(), e.second.size());
        }
    }
    wxMemoryBuffer buf;
    buf.AppendData(mem.GetOutputStreamBuffer()->GetBufferStart(), mem.GetSize());
    return buf;
}

// An archive description for a zip built by the test.
struct TestArchive {
    std::string sha;
    gamefiles::Archive archive;
    TestArchive(const char* label, const char* top, const char* subdir, const wxMemoryBuffer& zip)
        : sha(Sha256Hex(zip.GetData(), zip.GetDataLen())),
          archive{label, "x.zip", "http://example.invalid/x.zip", nullptr, 0, top, subdir} {
        archive.sha256 = sha.c_str();
    }
};

void sha256_vectors() {
    CHECK(Sha256Hex("", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256Hex("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const std::string two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";  // two blocks
    CHECK(Sha256Hex(two.data(), two.size()) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

void official_table() {
    const auto& a = gamefiles::Official();
    CHECK(a.size() == 2);
    if (a.size() != 2) return;
    CHECK(std::string(a[0].url) == "https://www.streetrodonline.com/wp-content/uploads/2017/05/sr1pc.zip");
    CHECK(std::string(a[0].sha256) == "74bbf38bea126cdfd6085e44827e9c193f9f27829619f38f8dbfe353ac7354d9");
    CHECK(a[0].size == 415994);
    CHECK(std::string(a[0].file) == "sr1pc.zip");
    CHECK(std::string(a[0].top) == "SR1");
    CHECK(std::string(a[0].subdir) == "");
    CHECK(std::string(a[1].url) == "https://www.streetrodonline.com/wp-content/uploads/2017/05/srsepc.zip");
    CHECK(std::string(a[1].sha256) == "0e91fefdbc5dea95291da05480b6f9647b7caa35d5f29a18223f0c0edf18cb12");
    CHECK(a[1].size == 568242);
    CHECK(std::string(a[1].file) == "srsepc.zip");
    CHECK(std::string(a[1].top) == "SRSE");
    CHECK(std::string(a[1].subdir) == "datadisk");
}

void url_override() {
    const auto& a = gamefiles::Official()[0];
    wxSetEnv("SRE_DOWNLOAD_BASE", "http://127.0.0.1:8765");
    CHECK(gamefiles::Url(a) == "http://127.0.0.1:8765/sr1pc.zip");
    wxUnsetEnv("SRE_DOWNLOAD_BASE");
    CHECK(gamefiles::Url(a) == a.url);
}

void free_folder_rules() {
    const wxString root = TempDir();
    CHECK(gamefiles::FolderIsFree(root + "/missing"));
    CHECK(gamefiles::FolderIsFree(root));
    WriteFile(root + "/.DS_Store", "x");
    CHECK(gamefiles::FolderIsFree(root));
    const wxString saves = TempDir();
    WriteFile(saves + "/HOTROD1.SAV", "x");
    CHECK(!gamefiles::FolderIsFree(saves));
    const wxString sub = TempDir();
    wxFileName::Mkdir(sub + "/datadisk");
    CHECK(!gamefiles::FolderIsFree(sub));
}

void installs_layout() {
    const wxString game = TempDir() + "/game";
    const wxMemoryBuffer za = Zip({{"SR1/SR.EXE", "exe"}, {"SR1/LIB1", "lib1"}, {"SR1/.DS_Store", "ds"},
                                   {"__MACOSX/SR1/._SR.EXE", "junk"}});
    const wxMemoryBuffer zb = Zip({{"SRSE/SRSE.EXE", "se"}});
    TestArchive a("Street Rod", "SR1", "", za);
    TestArchive b("Street Rod SE", "SRSE", "datadisk", zb);
    wxString error;
    const bool ok = gamefiles::Install(game, {{&a.archive, za}, {&b.archive, zb}}, error);
    CHECK(ok);
    if (!ok) std::printf("  error: %s\n", static_cast<const char*>(error.utf8_str()));
    CHECK(ReadFile(game + "/SR.EXE") == "exe");
    CHECK(ReadFile(game + "/LIB1") == "lib1");
    CHECK(ReadFile(game + "/datadisk/SRSE.EXE") == "se");
    CHECK(!Exists(game + "/.DS_Store"));
    CHECK(!Exists(game + "/__MACOSX"));
    CHECK(!Exists(game + "/SR1"));
    CHECK(!Exists(game + ".partial"));
}

void installs_into_path_with_trailing_slash() {
    const wxString game = TempDir() + "/game";
    const wxMemoryBuffer za = Zip({{"SR1/SR.EXE", "exe"}});
    TestArchive a("Street Rod", "SR1", "", za);
    wxString error;
    const bool ok = gamefiles::Install(game + "/", {{&a.archive, za}}, error);
    CHECK(ok);
    if (!ok) std::printf("  error: %s\n", static_cast<const char*>(error.utf8_str()));
    CHECK(ReadFile(game + "/SR.EXE") == "exe");
    CHECK(!Exists(game + ".partial"));
    CHECK(!Exists(game + "/.partial"));
}

void refuses_relative_path() {
    const wxMemoryBuffer za = Zip({{"SR1/SR.EXE", "exe"}});
    TestArchive a("Street Rod", "SR1", "", za);
    wxString error;
    CHECK(!gamefiles::Install("~/sre-test-relative", {{&a.archive, za}}, error));
    CHECK(error.Contains("full path"));
    CHECK(!Exists("~"));
}

void rejects_checksum_mismatch() {
    const wxString game = TempDir() + "/game";
    const wxMemoryBuffer za = Zip({{"SR1/SR.EXE", "exe"}}), zb = Zip({{"SRSE/SRSE.EXE", "se"}});
    TestArchive a("Street Rod", "SR1", "", za);
    TestArchive b("Street Rod SE", "SRSE", "datadisk", Zip({{"SRSE/SRSE.EXE", "different"}}));
    wxString error;
    CHECK(!gamefiles::Install(game, {{&a.archive, za}, {&b.archive, zb}}, error));
    CHECK(error.Contains("Street Rod SE"));
    CHECK(!Exists(game));
    CHECK(!Exists(game + ".partial"));
}

void refuses_nonfree_folder() {
    const wxString game = TempDir() + "/game";
    wxFileName::Mkdir(game);
    WriteFile(game + "/HOTROD1.SAV", "x");
    const wxMemoryBuffer za = Zip({{"SR1/SR.EXE", "exe"}});
    TestArchive a("Street Rod", "SR1", "", za);
    wxString error;
    CHECK(!gamefiles::Install(game, {{&a.archive, za}}, error));
    CHECK(!error.empty());
    CHECK(ReadFile(game + "/HOTROD1.SAV") == "x");
    CHECK(!Exists(game + "/SR.EXE"));
}

void rejects_zip_slip() {
    const wxString root = TempDir();
    const wxString game = root + "/game";
    const wxMemoryBuffer za = Zip({{"SR1/SR.EXE", "exe"}, {"SR1/../evil", "evil"}});
    TestArchive a("Street Rod", "SR1", "", za);
    wxString error;
    CHECK(!gamefiles::Install(game, {{&a.archive, za}}, error));
    CHECK(!Exists(root + "/evil"));
    CHECK(!Exists(game));
    CHECK(!Exists(game + ".partial"));
}

// The art pack the launcher uses: the first of its folders that holds a pack.json (on a Mac, one the player
// put in the settings folder before the one in the app).
void first_art_pack() {
    const wxString root = TempDir();
    const wxString user = root + "/user", bundled = root + "/bundled", empty = root + "/empty";
    wxFileName::Mkdir(user);
    wxFileName::Mkdir(bundled);
    wxFileName::Mkdir(empty);
    WriteFile(bundled + "/pack.json", "{}");
    CHECK(gamefiles::FirstArtPack({user, bundled}) == bundled);
    CHECK(gamefiles::FirstArtPack({root + "/missing", empty}).empty());
    CHECK(gamefiles::FirstArtPack({}).empty());
    WriteFile(user + "/pack.json", "{}");
    CHECK(gamefiles::FirstArtPack({user, bundled}) == user);
}

#define RUN(t)                                                                         \
    do {                                                                               \
        int before = failures;                                                         \
        t();                                                                           \
        std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", #t);             \
    } while (0)

}  // namespace

int main(int argc, char** argv) {
    wxInitializer init(argc, argv);
    if (!init.IsOk()) return 2;
    RUN(sha256_vectors);
    RUN(official_table);
    RUN(url_override);
    RUN(free_folder_rules);
    RUN(installs_layout);
    RUN(installs_into_path_with_trailing_slash);
    RUN(refuses_relative_path);
    RUN(rejects_checksum_mismatch);
    RUN(refuses_nonfree_folder);
    RUN(rejects_zip_slip);
    RUN(first_art_pack);
    return failures != 0;
}
