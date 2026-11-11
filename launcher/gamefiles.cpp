// gamefiles.cpp -- the official game files (gamefiles.h).
#include "gamefiles.h"

#include <wx/dir.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <memory>

#include "sha256.h"

namespace gamefiles {
namespace {

// The path of a zip entry under `top/`, without it; empty if the entry isn't one of the game's files
// (another folder, a directory, macOS metadata) or isn't a safe relative path.
bool EntryPath(const wxString& name, const wxString& top, wxString& rest, bool& unsafe) {
    unsafe = false;
    wxString n = name;
    n.Replace("\\", "/");
    if (n.StartsWith("/") || n.Contains(":")) { unsafe = true; return false; }
    for (const wxString& part : wxSplit(n, '/', 0))
        if (part == "..") { unsafe = true; return false; }
    if (n.StartsWith("__MACOSX/") || n.EndsWith("/")) return false;
    if (top.empty()) {
        rest = n;
    } else {
        if (!n.StartsWith(top + "/")) return false;
        rest = n.Mid(top.length() + 1);
    }
    if (rest.empty() || wxFileName(rest, wxPATH_UNIX).GetFullName() == ".DS_Store") return false;
    return true;
}

bool Extract(const Download& d, const wxString& dest, wxString& error) {
    return Unzip(d.data.GetData(), d.data.GetDataLen(), d.archive->top, dest, d.archive->label, error);
}

}  // namespace

bool RemoveTree(const wxString& dir) {
    if (!wxFileName::DirExists(dir)) return true;
    return wxFileName::Rmdir(dir, wxPATH_RMDIR_RECURSIVE);
}

bool Unzip(const void* data, size_t size, const wxString& top, const wxString& dest, const wxString& label,
           wxString& error) {
    wxMemoryInputStream mem(data, size);
    wxZipInputStream zip(mem);
    if (!zip.IsOk()) {
        error = wxString::Format("The download of %s isn't a zip file.", label);
        return false;
    }
    bool any = false;
    while (std::unique_ptr<wxZipEntry> entry{zip.GetNextEntry()}) {
        wxString rest;
        bool unsafe = false;
        if (!EntryPath(entry->GetName(wxPATH_UNIX), top, rest, unsafe)) {
            if (unsafe) {
                error = wxString::Format("The download of %s holds a file outside its folder.", label);
                return false;
            }
            continue;
        }
        wxFileName target(dest + "/" + rest);
        if (!wxFileName::Mkdir(target.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) &&
            !wxFileName::DirExists(target.GetPath())) {
            error = wxString::Format("Couldn't create the folder %s.", target.GetPath());
            return false;
        }
        wxFileOutputStream out(target.GetFullPath());
        if (!out.IsOk() || !zip.Read(out).Eof() || !out.Close()) {
            error = wxString::Format("Couldn't unpack %s from the download of %s.", rest, label);
            return false;
        }
        any = true;
    }
    if (!any) {
        error = wxString::Format("The download of %s doesn't hold the files it should.", label);
        return false;
    }
    return true;
}

const std::vector<Archive>& Official(Game game) {
    static const std::vector<Archive> sr2 = {
        {"Street Rod 2", "sr2pc.zip", "https://www.streetrodonline.com/wp-content/uploads/2017/05/sr2pc.zip",
         "e2d503bf5e8438d85d3ebb297121b783dbbd44f62424ac8ecfd5361e4fd0eaac", 716452, "", ""},
    };
    if (game == Game::SR2) return sr2;
    static const std::vector<Archive> archives = {
        {"Street Rod", "sr1pc.zip", "https://www.streetrodonline.com/wp-content/uploads/2017/05/sr1pc.zip",
         "74bbf38bea126cdfd6085e44827e9c193f9f27829619f38f8dbfe353ac7354d9", 415994, "SR1", ""},
        {"Street Rod SE", "srsepc.zip", "https://www.streetrodonline.com/wp-content/uploads/2017/05/srsepc.zip",
         "0e91fefdbc5dea95291da05480b6f9647b7caa35d5f29a18223f0c0edf18cb12", 568242, "SRSE", "datadisk"},
    };
    return archives;
}

wxString Url(const Archive& a) {
    wxString base;
    if (wxGetEnv("SRE_DOWNLOAD_BASE", &base) && !base.empty()) {
        if (base.EndsWith("/")) base.RemoveLast();
        return base + "/" + a.file;
    }
    return a.url;
}

bool FolderIsFree(const wxString& dir) {
    if (!wxFileName::DirExists(dir)) return !wxFileName::FileExists(dir);
    wxLogNull quiet;
    wxDir d(dir);
    if (!d.IsOpened()) return false;
    wxString name;
    for (bool more = d.GetFirst(&name, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN); more;
         more = d.GetNext(&name))
        if (name != ".DS_Store") return false;
    return true;
}

bool Install(const wxString& folder, const std::vector<Download>& downloads, wxString& error) {
    // One spelling of the folder, without a trailing separator, so <folder>.partial sits beside it.
    wxFileName name = wxFileName::DirName(folder);
    if (!name.IsAbsolute()) {
        error = wxString::Format("%s isn't a full path. Choose the folder with Browse.", folder);
        return false;
    }
    name.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    wxString gameDir = name.GetPath();
    if (gameDir.empty()) gameDir = wxFileName::GetPathSeparator();
    for (const Download& d : downloads)
        if (Sha256Hex(d.data.GetData(), d.data.GetDataLen()) != d.archive->sha256) {
            error = wxString::Format("The download of %s is damaged or has changed on the website.", d.archive->label);
            return false;
        }
    if (!FolderIsFree(gameDir)) {
        error = wxString::Format("%s isn't empty, so nothing was installed there.", gameDir);
        return false;
    }

    wxLogNull quiet;
    const wxString partial = gameDir + ".partial";
    if (!RemoveTree(partial) || !wxFileName::Mkdir(partial, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        error = wxString::Format("Couldn't create the folder %s.", partial);
        return false;
    }
    for (const Download& d : downloads) {
        wxString dest = partial;
        if (*d.archive->subdir) dest += wxString("/") + d.archive->subdir;
        if (!Extract(d, dest, error)) {
            RemoveTree(partial);
            return false;
        }
    }
    if (!RemoveTree(gameDir) || !wxRenameFile(partial, gameDir, false)) {
        error = wxString::Format("Couldn't move the game into %s.", gameDir);
        RemoveTree(partial);
        return false;
    }
    return true;
}

wxString FirstArtPack(const std::vector<wxString>& dirs) {
    for (const wxString& dir : dirs)
        if (!dir.empty() && wxFileName::FileExists(wxFileName(dir, "pack.json").GetFullPath())) return dir;
    return wxString();
}

}  // namespace gamefiles
