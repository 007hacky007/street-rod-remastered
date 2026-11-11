// test_artpack.cpp -- the art pack's manifest, the check for a newer pack and the installer
// (launcher/artpack.cpp).
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

#include "artpack.h"
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

wxString TempDir() {
    wxString path = wxFileName::CreateTempFileName("sre-art");
    wxRemoveFile(path);
    wxFileName::Mkdir(path, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return path;
}

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

const std::string PACK_JSON = "{\n \"format\": 1,\n \"pictures\": {}\n}\n";

wxMemoryBuffer GoodZip(const std::string& pic = "PNG1") {
    return Zip({{"Remaster/pack.json", PACK_JSON}, {"Remaster/pics/abc.png", pic}});
}

artpack::Manifest ManifestOf(const wxMemoryBuffer& zip, const char* version = "v1") {
    artpack::Manifest m;
    m.version = version;
    m.format = 1;
    m.size = zip.GetDataLen();
    m.sha256 = Sha256Hex(zip.GetData(), zip.GetDataLen());
    return m;
}

wxString Sep() { return wxString(wxFILE_SEP_PATH); }

void parses_manifest() {
    artpack::Manifest m;
    wxString error;
    CHECK(artpack::ParseManifest("version=v3\nformat=1\nsize=25872202\nsha256=792FF694ff196d3de16807bcd2d0ecbad"
                                 "5327be82e15ac93d571d61d82a62c2f\n",
                                 m, error));
    CHECK(m.version == "v3");
    CHECK(m.format == 1);
    CHECK(m.size == 25872202UL);
    CHECK(m.sha256 == "792ff694ff196d3de16807bcd2d0ecbad5327be82e15ac93d571d61d82a62c2f");  // lower case
    // Windows line ends and unknown keys are fine.
    CHECK(artpack::ParseManifest("version=v4\r\nnote=x\r\nformat=1\r\nsize=10\r\nsha256=" + std::string(64, 'a') +
                                     "\r\n",
                                 m, error));
    CHECK(m.version == "v4" && m.size == 10);
}

void rejects_bad_manifest() {
    artpack::Manifest m;
    wxString error;
    const std::string sha = "sha256=" + std::string(64, 'a') + "\n";
    CHECK(!artpack::ParseManifest("format=1\nsize=10\n" + sha, m, error));          // no version
    CHECK(!artpack::ParseManifest("version=v1\nsize=10\n" + sha, m, error));        // no format
    CHECK(!artpack::ParseManifest("version=v1\nformat=1\n" + sha, m, error));       // no size
    CHECK(!artpack::ParseManifest("version=v1\nformat=1\nsize=0\n" + sha, m, error));
    CHECK(!artpack::ParseManifest("version=v1\nformat=1\nsize=10\nsha256=abc\n", m, error));
    CHECK(!artpack::ParseManifest("<html>Not Found</html>", m, error));
    CHECK(!error.empty());
}

void urls_and_override() {
    wxUnsetEnv("SRE_ART_BASE");
    CHECK(artpack::ManifestUrl() ==
          "https://github.com/007hacky007/street-rod-remastered-art/releases/latest/download/"
          "Street-Rod-Remastered-art.txt");
    CHECK(artpack::ZipUrl() ==
          "https://github.com/007hacky007/street-rod-remastered-art/releases/latest/download/"
          "Street-Rod-Remastered-art.zip");
    wxSetEnv("SRE_ART_BASE", "http://127.0.0.1:8000/");
    CHECK(artpack::ManifestUrl() == "http://127.0.0.1:8000/Street-Rod-Remastered-art.txt");
    CHECK(artpack::ZipUrl() == "http://127.0.0.1:8000/Street-Rod-Remastered-art.zip");
    wxUnsetEnv("SRE_ART_BASE");
}

void installs_and_records_release() {
    const wxString dir = TempDir() + Sep() + "Remaster";
    const wxMemoryBuffer zip = GoodZip();
    const artpack::Manifest m = ManifestOf(zip);
    wxString error;
    CHECK(artpack::Install(dir, zip, m, error));
    CHECK(error.empty());
    CHECK(ReadFile(dir + Sep() + "pack.json") == PACK_JSON);
    CHECK(ReadFile(dir + Sep() + "pics" + Sep() + "abc.png") == "PNG1");
    CHECK(!Exists(dir + ".partial"));
    const artpack::Manifest got = artpack::Installed(dir);
    CHECK(got.version == "v1");
    CHECK(got.sha256 == m.sha256);
}

void update_replaces_old_pack() {
    const wxString dir = TempDir() + Sep() + "Remaster";
    WriteFile(dir + Sep() + "pack.json", PACK_JSON);
    WriteFile(dir + Sep() + "pics" + Sep() + "gone.png", "OLD");
    const wxMemoryBuffer zip = GoodZip("PNG2");
    wxString error;
    CHECK(artpack::Install(dir, zip, ManifestOf(zip, "v2"), error));
    CHECK(ReadFile(dir + Sep() + "pics" + Sep() + "abc.png") == "PNG2");
    CHECK(!Exists(dir + Sep() + "pics" + Sep() + "gone.png"));  // a picture taken out of the pack goes
    CHECK(!Exists(dir + ".old"));
    CHECK(artpack::Installed(dir).version == "v2");
}

void failed_install_keeps_old_pack() {
    const wxString dir = TempDir() + Sep() + "Remaster";
    WriteFile(dir + Sep() + "pack.json", PACK_JSON);
    WriteFile(dir + Sep() + "pics" + Sep() + "keep.png", "OLD");
    wxString error;
    // Damaged download: the checksum doesn't match.
    wxMemoryBuffer zip = GoodZip();
    artpack::Manifest m = ManifestOf(zip);
    m.sha256 = std::string(64, '0');
    CHECK(!artpack::Install(dir, zip, m, error));
    CHECK(!error.empty());
    // Wrong size.
    m = ManifestOf(zip);
    m.size += 1;
    CHECK(!artpack::Install(dir, zip, m, error));
    // No pack.json in the zip.
    zip = Zip({{"Remaster/pics/abc.png", "x"}});
    CHECK(!artpack::Install(dir, zip, ManifestOf(zip), error));
    // A newer pack format than this launcher reads.
    zip = Zip({{"Remaster/pack.json", "{\"format\": 2}"}});
    CHECK(!artpack::Install(dir, zip, ManifestOf(zip), error));
    zip = GoodZip();
    m = ManifestOf(zip);
    m.format = artpack::FORMAT + 1;
    CHECK(!artpack::Install(dir, zip, m, error));
    // Zip slip.
    zip = Zip({{"Remaster/pack.json", PACK_JSON}, {"Remaster/../../evil.txt", "x"}});
    CHECK(!artpack::Install(dir, zip, ManifestOf(zip), error));
    // Through all of it the old pack stayed, and nothing was left beside it.
    CHECK(ReadFile(dir + Sep() + "pics" + Sep() + "keep.png") == "OLD");
    CHECK(!Exists(dir + ".partial"));
    CHECK(!Exists(dir + ".old"));
}

void offers() {
    artpack::Manifest latest;
    latest.version = "v2";
    latest.format = 1;
    latest.sha256 = std::string(64, 'b');
    artpack::Manifest installed;
    // No pack at all: offer it, unless the player already said no to the first offer.
    CHECK(artpack::Offer(false, installed, latest, false, wxString()) == artpack::OFFER_DOWNLOAD);
    CHECK(artpack::Offer(false, installed, latest, true, wxString()) == artpack::OFFER_NONE);
    // A pack that isn't the latest (or of unknown origin): offer the update, unless this one was skipped.
    CHECK(artpack::Offer(true, installed, latest, true, wxString()) == artpack::OFFER_UPDATE);
    installed.sha256 = std::string(64, 'a');
    CHECK(artpack::Offer(true, installed, latest, true, wxString()) == artpack::OFFER_UPDATE);
    CHECK(artpack::Offer(true, installed, latest, true, latest.sha256) == artpack::OFFER_NONE);
    // The latest is installed.
    installed.sha256 = latest.sha256;
    CHECK(artpack::Offer(true, installed, latest, false, wxString()) == artpack::OFFER_NONE);
    // A pack format this launcher can't read is never offered.
    latest.format = artpack::FORMAT + 1;
    CHECK(artpack::Offer(false, artpack::Manifest(), latest, false, wxString()) == artpack::OFFER_NONE);
    installed.sha256 = std::string(64, 'a');
    CHECK(artpack::Offer(true, installed, latest, false, wxString()) == artpack::OFFER_NONE);
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
    RUN(urls_and_override);
    RUN(installs_and_records_release);
    RUN(update_replaces_old_pack);
    RUN(failed_install_keeps_old_pack);
    RUN(offers);
    std::printf(failures ? "%d failures\n" : "all ok\n", failures);
    return failures ? 1 : 0;
}
