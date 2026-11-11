// artpack.cpp -- the remastered pictures as a download (artpack.h).
#include "artpack.h"

#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>

#include "gamefiles.h"
#include "sha256.h"

namespace artpack {
namespace {

const char* const LATEST = "https://github.com/007hacky007/street-rod-remastered-art/releases/latest/download";
const char* const MANIFEST = "Street-Rod-Remastered-art.txt";
const char* const ZIP = "Street-Rod-Remastered-art.zip";
// Beside pack.json in an installed pack: the manifest it came from.
const char* const RECORD = "release.txt";

wxString Url(const char* name) {
    wxString base;
    if (!wxGetEnv("SRE_ART_BASE", &base) || base.empty()) base = LATEST;
    if (base.EndsWith("/")) base.RemoveLast();
    return base + "/" + name;
}

wxString Join(const wxString& dir, const wxString& name) { return dir + wxFILE_SEP_PATH + name; }

wxString ReadText(const wxString& path) {
    wxLogNull quiet;
    wxFFile f(path, "rb");
    wxString text;
    if (!f.IsOpened() || !f.ReadAll(&text, wxConvUTF8)) return wxString();
    return text;
}

// The "format" of a pack.json, 0 if it has none.
int PackFormat(const wxString& json) {
    wxRegEx re("\"format\"[[:space:]]*:[[:space:]]*([0-9]+)");
    long n = 0;
    if (!re.Matches(json) || !re.GetMatch(json, 1).ToLong(&n)) return 0;
    return int(n);
}

bool IsSha256(const wxString& s) {
    if (s.length() != 64) return false;
    for (wxUniChar c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

}  // namespace

wxString ManifestUrl() { return Url(MANIFEST); }
wxString ZipUrl() { return Url(ZIP); }

bool ParseManifest(const wxString& text, Manifest& m, wxString& error) {
    Manifest got;
    wxString size;
    long format = 0;
    wxStringTokenizer lines(text, "\r\n", wxTOKEN_STRTOK);
    while (lines.HasMoreTokens()) {
        const wxString line = lines.GetNextToken().Strip(wxString::both);
        const wxString key = line.BeforeFirst('='), value = line.AfterFirst('=').Strip(wxString::both);
        if (key == "version") got.version = value;
        else if (key == "format") value.ToLong(&format);
        else if (key == "size") size = value;
        else if (key == "sha256") got.sha256 = value.Lower();
    }
    got.format = int(format);
    unsigned long n = 0;
    if (size.ToULong(&n)) got.size = n;
    if (got.version.empty() || got.format <= 0 || got.size == 0 || !IsSha256(got.sha256)) {
        error = "The description of the remastered pictures on GitHub couldn't be read.";
        return false;
    }
    m = got;
    return true;
}

Manifest Installed(const wxString& dir) {
    Manifest m;
    wxString error;
    const wxString text = ReadText(Join(dir, RECORD));
    if (text.empty() || !ParseManifest(text, m, error)) return Manifest();
    return m;
}

OfferKind Offer(bool havePack, const Manifest& installed, const Manifest& latest, bool declined,
                const wxString& skipped) {
    if (latest.format > FORMAT || latest.sha256.empty()) return OFFER_NONE;
    if (!havePack) return declined ? OFFER_NONE : OFFER_DOWNLOAD;
    if (installed.sha256 == latest.sha256 || skipped == latest.sha256) return OFFER_NONE;
    return OFFER_UPDATE;
}

bool Install(const wxString& dir, const wxMemoryBuffer& zip, const Manifest& m, wxString& error) {
    const wxString label = "the remastered pictures";
    if (m.format > FORMAT) {
        error = "These remastered pictures need a newer Street Rod Enhanced.";
        return false;
    }
    if (zip.GetDataLen() != m.size || Sha256Hex(zip.GetData(), zip.GetDataLen()) != m.sha256) {
        error = "The download of the remastered pictures is damaged. Try again.";
        return false;
    }
    wxFileName name = wxFileName::DirName(dir);
    name.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    const wxString target = name.GetPath();
    const wxString partial = target + ".partial", old = target + ".old";

    wxLogNull quiet;
    if (!gamefiles::RemoveTree(partial) || !gamefiles::RemoveTree(old) ||
        !wxFileName::Mkdir(partial, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        error = wxString::Format("Couldn't create the folder %s.", partial);
        return false;
    }
    auto fail = [&](const wxString& why) {
        error = why;
        gamefiles::RemoveTree(partial);
        return false;
    };
    if (!gamefiles::Unzip(zip.GetData(), zip.GetDataLen(), "Remaster", partial, label, error)) return fail(error);
    const int format = PackFormat(ReadText(Join(partial, "pack.json")));
    if (format == 0) return fail("The download of the remastered pictures has no pack.json.");
    if (format > FORMAT) return fail("These remastered pictures need a newer Street Rod Enhanced.");
    {
        wxFFile record(Join(partial, RECORD), "wb");
        const wxString text = wxString::Format("version=%s\nformat=%d\nsize=%lu\nsha256=%s\n", m.version,
                                               m.format, m.size, m.sha256);
        if (!record.IsOpened() || !record.Write(text, wxConvUTF8) || !record.Close())
            return fail(wxString::Format("Couldn't write into %s.", partial));
    }
    // The old pack steps aside first, so a failed move can put it back.
    const bool hadOld = wxFileName::DirExists(target);
    if (hadOld && !wxRenameFile(target, old, false))
        return fail(wxString::Format("Couldn't replace the pictures in %s. Is the game running?", target));
    if (!wxRenameFile(partial, target, false)) {
        if (hadOld) wxRenameFile(old, target, false);
        return fail(wxString::Format("Couldn't move the remastered pictures into %s.", target));
    }
    if (hadOld) gamefiles::RemoveTree(old);
    return true;
}

}  // namespace artpack
