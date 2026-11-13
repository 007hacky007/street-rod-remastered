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
    std::printf(failures ? "%d failures\n" : "all ok\n", failures);
    return failures ? 1 : 0;
}
