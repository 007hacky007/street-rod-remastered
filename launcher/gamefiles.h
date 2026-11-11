// gamefiles.h -- the official game files: where they are downloaded from, how they are checked, and
// installing them into a game folder (Street Rod in it, Street Rod SE in its datadisk folder; Street Rod 2 in
// its own).
#pragma once

#include <wx/buffer.h>
#include <wx/string.h>

#include <vector>

#include "gameargs.h"

namespace gamefiles {

struct Archive {
    const char* label;      // "Street Rod", "Street Rod SE", "Street Rod 2"
    const char* file;       // "sr1pc.zip"
    const char* url;        // on streetrodonline.com
    const char* sha256;
    unsigned long size;
    const char* top;        // the folder inside the zip: "SR1", "SRSE"; "" when the files are at its top
    const char* subdir;     // where it goes under the game folder: "", "datadisk"
};

struct Download {
    const Archive* archive;
    wxMemoryBuffer data;
};

// A game's downloads: Street Rod, then Street Rod SE; or Street Rod 2.
const std::vector<Archive>& Official(Game game);

// Where to download `a` from: SRE_DOWNLOAD_BASE + "/" + file when that variable is set (a developer aid
// for testing failures), else the official URL.
wxString Url(const Archive& a);

// True if `dir` doesn't exist, is empty or holds only .DS_Store: installing there can't overwrite anything.
bool FolderIsFree(const wxString& dir);

// Checks every download's checksum, then installs them all into `folder` (a full path), which must be free.
// Either everything is installed or nothing is: on failure `folder` is left as it was and `error` says why.
bool Install(const wxString& folder, const std::vector<Download>& downloads, wxString& error);

// Unpacks the files under `top/` (every file when `top` is empty) in a zip in memory into `dest`, refusing entries that would land outside
// it; `label` names the download in the error ("The download of %s ..."). False if nothing was unpacked.
bool Unzip(const void* data, size_t size, const wxString& top, const wxString& dest, const wxString& label,
           wxString& error);

// Removes a folder and everything in it; true if it's gone.
bool RemoveTree(const wxString& dir);

// The first of `dirs` that holds an art pack (a pack.json), or empty if none does.
wxString FirstArtPack(const std::vector<wxString>& dirs);

}  // namespace gamefiles
