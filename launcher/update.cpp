// update.cpp -- the launcher's own updates (update.h).
#include "update.h"

#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/regex.h>
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

// A rename that never falls back to copying (wxRenameFile does, and then ignores a failed delete) and never
// replaces anything. A few tries: a virus scanner may hold a file it has just seen for a moment.
bool Move(const wxString& from, const wxString& to) {
    for (int i = 0; i < 3; i++) {
        if (wxFileName::Exists(to)) return false;
        if (wxRename(from, to) == 0) return true;
        wxMilliSleep(200);
    }
    return false;
}

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

// "<parent><sep>.<name><suffix>" for a path ending in <name>; either separator.
wxString Beside(const wxString& path, const wxString& suffix) {
    const size_t at = path.find_last_of("/\\");
    if (at == wxString::npos) return "." + path + suffix;
    return path.Left(at + 1) + "." + path.Mid(at + 1) + suffix;
}

#ifdef __APPLE__
// Runs a program with these arguments (no shell); true if it exits with 0.
bool Run(const std::vector<wxString>& args) {
    std::vector<wxCharBuffer> utf8;
    for (const wxString& a : args) utf8.push_back(a.utf8_str());
    std::vector<const char*> argv;
    for (const wxCharBuffer& b : utf8) argv.push_back(b.data());
    argv.push_back(nullptr);
    return wxExecute(argv.data(), wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE) == 0;
}
#endif

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
    const bool readable = got.format > FORMAT || (ValidDownload(got.mac) && ValidDownload(got.windows));
    if (!Numbers(got.version, n) || got.format <= 0 || !readable) {
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

wxString ManualReason(const Manifest& m) {
    return m.format > FORMAT ? "This version needs a manual download." : wxString();
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
    // Back in reverse order; a .old that can't go back stays listed, so CleanFolder restores it later.
    auto undo = [&](const wxString& why) {
        wxString keep = listBefore;
        for (auto it = done.rbegin(); it != done.rend(); ++it) {
            const wxString target = Join(dir, it->rel);
            const bool out = Move(target, Join(staging, it->rel));
            if (it->keptOld && !(out && Move(target + ".old", target))) keep += it->rel + ".old\n";
        }
        WriteText(list, keep);
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
            if (!WriteText(list, listNow) || !Move(target, old)) return undo(cant);
        }
        if (!Move(Join(staging, rel), target)) {
            if (keepOld) Move(old, target);
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
        const wxString path = Join(dir, rel), original = path.Left(path.length() - 4);
        if (!wxFileName::FileExists(path)) continue;
        // Stopped mid-swap: the old file is all there is, so it goes back.
        const bool gone = wxFileName::Exists(original) ? wxRemoveFile(path) : Move(path, original);
        if (!gone) left += rel + "\n";
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

wxString BundleOf(const wxString& executable) {
    const wxString macos = "/Contents/MacOS/";
    const size_t at = executable.rfind(macos);
    if (at == wxString::npos || executable.find('/', at + macos.length()) != wxString::npos) return wxString();
    const wxString bundle = executable.Left(at);
    return bundle.EndsWith(".app") ? bundle : wxString();
}

wxString StagedBundle(const wxString& bundle) { return Beside(bundle, ".update"); }
wxString OldBundle(const wxString& bundle) { return Beside(bundle, ".old"); }

wxString CantUpdateBundle(const wxString& bundle) {
    if (bundle.empty()) return "This copy of Street Rod Enhanced isn't an installed app.";
    if (bundle.Contains("/AppTranslocation/") || bundle.StartsWith("/Volumes/"))
        return "Move Street Rod Enhanced to Applications first, then update.";
    return CantUpdateFolder(wxFileName(bundle).GetPath());
}

bool ApplyBundle(const wxString& bundle, wxString& error) {
    wxLogNull quiet;
    const wxString staged = StagedBundle(bundle), old = OldBundle(bundle);
    error = wxString::Format("Couldn't replace %s.", bundle);
    if (!wxFileName::DirExists(staged) || !gamefiles::RemoveTree(old)) return false;
    if (!Move(bundle, old)) return false;
    if (!Move(staged, bundle)) {
        Move(old, bundle);
        return false;
    }
    error.clear();
    return true;
}

void CleanBundle(const wxString& bundle) {
    wxLogNull quiet;
    gamefiles::RemoveTree(OldBundle(bundle));
    gamefiles::RemoveTree(StagedBundle(bundle));
}

#ifdef __APPLE__
bool StageBundle(const wxString& bundle, const wxMemoryBuffer& dmg, const Manifest& m, wxString& error) {
    if (!Verify(dmg, m.mac, error)) return false;
    wxLogNull quiet;
    const wxString staged = StagedBundle(bundle);
    const wxString image = wxFileName::CreateTempFileName("sre-update") + ".dmg";
    wxString mount = wxFileName::CreateTempFileName("sre-update-mount");
    wxRemoveFile(mount);
    bool ok = gamefiles::RemoveTree(staged) && wxFileName::Mkdir(mount, wxS_DIR_DEFAULT, 0);
    if (ok) {
        wxFFile f(image, "wb");
        ok = f.IsOpened() && f.Write(dmg.GetData(), dmg.GetDataLen()) == dmg.GetDataLen() && f.Close();
    }
    if (!ok) error = "Couldn't save the download of the new version.";
    bool attached = false;
    if (ok) {
        attached = ok = Run({"/usr/bin/hdiutil", "attach", "-quiet", "-nobrowse", "-readonly", "-noautoopen",
                             "-mountpoint", mount, image});
        if (!ok) error = "Couldn't open the disk image of the new version.";
    }
    if (ok) {
        ok = Run({"/usr/bin/ditto", mount + "/Street Rod Enhanced.app", staged});
        if (!ok) error = wxString::Format("Couldn't copy the new version into %s.", wxFileName(bundle).GetPath());
    }
    if (attached) Run({"/usr/bin/hdiutil", "detach", "-quiet", mount});
    wxRmdir(mount);
    wxRemoveFile(image);
    if (ok) {
        ok = Run({"/usr/bin/codesign", "--verify", "--strict", staged});
        if (!ok) error = "The new version's signature doesn't check out.";
    }
    if (ok) {
        wxRegEx re("<key>CFBundleShortVersionString</key>[[:space:]]*<string>([^<]*)</string>");
        const wxString plist = ReadText(staged + "/Contents/Info.plist");
        ok = re.Matches(plist) && re.GetMatch(plist, 1) == m.version;
        if (!ok) error = "The download isn't the version it should be.";
    }
    if (!ok) gamefiles::RemoveTree(staged);
    return ok;
}
#endif

}  // namespace update
