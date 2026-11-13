// update.cpp -- the launcher's own updates (update.h).
#include "update.h"

#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>

#include <vector>

#include "gamefiles.h"
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

const char* const OLD_LIST = ".update-old.txt";

wxString Join(const wxString& dir, const wxString& rel) {
    wxString r = rel;
    r.Replace("/", wxFILE_SEP_PATH);
    return dir + wxFILE_SEP_PATH + r;
}

wxString ReadText(const wxString& path) {
    wxFFile f(path, "rb");
    wxString text;
    if (!f.IsOpened() || !f.ReadAll(&text, wxConvUTF8)) return wxString();
    return text;
}

// Writes `text` to `path`, or removes the file when `text` is empty.
bool WriteText(const wxString& path, const wxString& text) {
    if (text.empty()) return !wxFileName::FileExists(path) || wxRemoveFile(path);
    wxFFile f(path, "wb");
    return f.IsOpened() && f.Write(text, wxConvUTF8) && f.Close();
}

// The files under `dir`, relative, with "/" between folders.
std::vector<wxString> FilesUnder(const wxString& dir) {
    wxArrayString all;
    wxDir::GetAllFiles(dir, &all, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN);
    std::vector<wxString> rel;
    for (const wxString& path : all) {
        wxString r = path.Mid(dir.length() + 1);
        r.Replace(wxFILE_SEP_PATH, "/");
        rel.push_back(r);
    }
    return rel;
}

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

wxString StagingDir(const wxString& dir) { return dir + wxFILE_SEP_PATH + ".update"; }

bool StageFolder(const wxString& dir, const wxMemoryBuffer& zip, const Download& d, wxString& error) {
    if (!Verify(zip, d, error)) return false;
    wxLogNull quiet;
    const wxString staging = StagingDir(dir);
    if (!gamefiles::RemoveTree(staging) || !wxFileName::Mkdir(staging, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        error = wxString::Format("Couldn't create the folder %s.", staging);
        return false;
    }
    bool ok = gamefiles::Unzip(zip.GetData(), zip.GetDataLen(), "Street Rod Enhanced", staging, "the new version",
                               error);
    if (ok && !wxFileName::FileExists(Join(staging, "Street Rod Enhanced.exe"))) {
        error = "The download of the new version doesn't hold Street Rod Enhanced.exe.";
        ok = false;
    }
    if (!ok) gamefiles::RemoveTree(staging);
    return ok;
}

bool ApplyFolder(const wxString& dir, wxString& error) {
    wxLogNull quiet;
    const wxString staging = StagingDir(dir), list = Join(dir, OLD_LIST);
    const std::vector<wxString> files = FilesUnder(staging);
    if (files.empty()) {
        error = "The new version isn't ready. Try again.";
        return false;
    }
    const wxString listBefore = ReadText(list);
    wxString listNow = listBefore;
    struct Step {
        wxString rel;
        bool keptOld;
    };
    std::vector<Step> done;
    auto undo = [&](const wxString& why) {
        for (auto it = done.rbegin(); it != done.rend(); ++it) {
            const wxString target = Join(dir, it->rel);
            wxRenameFile(target, Join(staging, it->rel), false);
            if (it->keptOld) wxRenameFile(target + ".old", target, false);
        }
        WriteText(list, listBefore);
        error = why;
        return false;
    };
    for (const wxString& rel : files) {
        const wxString target = Join(dir, rel), old = target + ".old";
        const wxString cant = wxString::Format("Couldn't replace %s. Is it open?", target);
        const wxString parent = wxFileName(target).GetPath();
        if (!wxFileName::DirExists(parent) && !wxFileName::Mkdir(parent, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
            return undo(cant);
        const bool keepOld = wxFileName::FileExists(target);
        if (keepOld) {
            if (wxFileName::FileExists(old) && !wxRemoveFile(old)) return undo(cant);
            listNow += rel + ".old\n";
            if (!WriteText(list, listNow) || !wxRenameFile(target, old, false)) return undo(cant);
        }
        if (!wxRenameFile(Join(staging, rel), target, false)) {
            if (keepOld) wxRenameFile(old, target, false);
            return undo(cant);
        }
        done.push_back({rel, keepOld});
    }
    gamefiles::RemoveTree(staging);
    return true;
}

void CleanFolder(const wxString& dir) {
    wxLogNull quiet;
    const wxString list = Join(dir, OLD_LIST);
    wxString left;
    wxStringTokenizer lines(ReadText(list), "\r\n", wxTOKEN_STRTOK);
    while (lines.HasMoreTokens()) {
        const wxString rel = lines.GetNextToken();
        if (!rel.EndsWith(".old") || rel.Contains("..") || wxFileName(rel).IsAbsolute()) continue;
        const wxString path = Join(dir, rel);
        if (wxFileName::FileExists(path) && !wxRemoveFile(path)) left += rel + "\n";
    }
    WriteText(list, left);
    gamefiles::RemoveTree(StagingDir(dir));
}

wxString CantUpdateFolder(const wxString& dir) {
    wxLogNull quiet;
    const wxString probe = Join(dir, ".update-check");
    bool ok;
    {
        wxFFile f(probe, "wb");
        ok = f.IsOpened() && f.Close();
    }
    if (ok) wxRemoveFile(probe);
    if (ok) return wxString();
    return wxString::Format("Street Rod Enhanced can't update itself in %s. Download the new version from GitHub "
                            "instead.",
                            dir);
}

}  // namespace update
