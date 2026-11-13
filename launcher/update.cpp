// update.cpp -- the launcher's own updates (update.h).
#include "update.h"

#include <wx/tokenzr.h>
#include <wx/utils.h>

#include "sha256.h"

namespace update {
namespace {

const char* const REPO = "https://github.com/007hacky007/street-rod-remastered";
const char* const MANIFEST = "Street-Rod-Enhanced.txt";
const char* const DMG = "Street-Rod-Enhanced.dmg";
const char* const ZIP = "Street-Rod-Enhanced-windows.zip";

wxString Url(const char* name) {
    wxString base;
    if (!wxGetEnv("SRE_UPDATE_BASE", &base) || base.empty()) base = wxString(REPO) + "/releases/latest/download";
    if (base.EndsWith("/")) base.RemoveLast();
    return base + "/" + name;
}

bool IsSha256(const wxString& s) {
    if (s.length() != 64) return false;
    for (wxUniChar c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

// "0.6.0" into its three numbers; false for anything else.
bool Numbers(const wxString& v, unsigned long out[3]) {
    wxStringTokenizer parts(v, ".", wxTOKEN_RET_EMPTY_ALL);
    int n = 0;
    while (parts.HasMoreTokens()) {
        const wxString p = parts.GetNextToken();
        if (n == 3 || p.empty() || p.length() > 6) return false;
        for (wxUniChar c : p)
            if (c < '0' || c > '9') return false;
        p.ToULong(&out[n++]);
    }
    return n == 3;
}

bool ValidDownload(const Download& d) { return d.size > 0 && IsSha256(d.sha256); }

}  // namespace

wxString ManifestUrl() { return Url(MANIFEST); }
wxString DmgUrl() { return Url(DMG); }
wxString ZipUrl() { return Url(ZIP); }
wxString ReleasePage(const wxString& version) { return wxString(REPO) + "/releases/tag/v" + version; }

bool ParseManifest(const wxString& text, Manifest& m, wxString& error) {
    Manifest got;
    long format = 0;
    wxStringTokenizer lines(text, "\r\n", wxTOKEN_STRTOK);
    while (lines.HasMoreTokens()) {
        const wxString line = lines.GetNextToken().Strip(wxString::both);
        const wxString key = line.BeforeFirst('='), value = line.AfterFirst('=').Strip(wxString::both);
        if (key == "version") got.version = value;
        else if (key == "format") value.ToLong(&format);
        else if (key == "mac.sha256") got.mac.sha256 = value.Lower();
        else if (key == "mac.size") value.ToULong(&got.mac.size);
        else if (key == "windows.sha256") got.windows.sha256 = value.Lower();
        else if (key == "windows.size") value.ToULong(&got.windows.size);
    }
    got.format = int(format);
    unsigned long n[3];
    if (!Numbers(got.version, n) || got.format <= 0 || !ValidDownload(got.mac) || !ValidDownload(got.windows)) {
        error = "The description of the new version on GitHub couldn't be read.";
        return false;
    }
    m = got;
    return true;
}

bool IsNewer(const wxString& a, const wxString& b) {
    unsigned long x[3], y[3];
    if (!Numbers(a, x) || !Numbers(b, y)) return false;
    for (int i = 0; i < 3; i++)
        if (x[i] != y[i]) return x[i] > y[i];
    return false;
}

bool ShouldOffer(const Manifest& latest, const wxString& current, const wxString& skipped, bool asked) {
    if (!IsNewer(latest.version, current)) return false;
    return asked || skipped.empty() || IsNewer(latest.version, skipped);
}

bool Verify(const wxMemoryBuffer& data, const Download& d, wxString& error) {
    if (data.GetDataLen() != d.size || Sha256Hex(data.GetData(), data.GetDataLen()) != d.sha256.ToStdString()) {
        error = "The download is damaged. Try again.";
        return false;
    }
    return true;
}

}  // namespace update
