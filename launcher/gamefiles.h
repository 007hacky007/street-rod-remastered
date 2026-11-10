// gamefiles.h -- the official game files: where they are downloaded from, how they are checked, and
// installing them into a game folder (Street Rod in it, Street Rod SE in its datadisk folder).
#pragma once

#include <wx/buffer.h>
#include <wx/string.h>

#include <vector>

namespace gamefiles {

struct Archive {
    const char* label;      // "Street Rod", "Street Rod SE"
    const char* file;       // "sr1pc.zip"
    const char* url;        // on streetrodonline.com
    const char* sha256;
    unsigned long size;
    const char* top;        // the folder inside the zip: "SR1", "SRSE"
    const char* subdir;     // where it goes under the game folder: "", "datadisk"
};

struct Download {
    const Archive* archive;
    wxMemoryBuffer data;
};

// Street Rod, then Street Rod SE.
const std::vector<Archive>& Official();

// Where to download `a` from: SRE_DOWNLOAD_BASE + "/" + file when that variable is set (a developer aid
// for testing failures), else the official URL.
wxString Url(const Archive& a);

// True if `dir` doesn't exist, is empty or holds only .DS_Store: installing there can't overwrite anything.
bool FolderIsFree(const wxString& dir);

// Checks every download's checksum, then installs them all into `gameDir`, which must be free. Either
// everything is installed or nothing is: on failure `gameDir` is left as it was and `error` says why.
bool Install(const wxString& gameDir, const std::vector<Download>& downloads, wxString& error);

}  // namespace gamefiles
