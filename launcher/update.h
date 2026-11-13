// update.h -- the launcher's own updates: the latest release's manifest, whether to offer it, and putting the
// new version in place of the running one (a Windows folder, a macOS app bundle), each step undoable.
#pragma once

#include <wx/buffer.h>
#include <wx/string.h>

namespace update {

// The newest manifest format this launcher can update from; a newer one only gets the release page.
constexpr int FORMAT = 1;

struct Download {
    wxString sha256;         // lower case
    unsigned long size = 0;  // bytes
};

// Street-Rod-Enhanced.txt: "key=value" lines (version, format, mac.sha256, mac.size, windows.sha256,
// windows.size).
struct Manifest {
    wxString version;  // "0.6.0"
    int format = 0;
    Download mac, windows;
};

// The latest release's files: SRE_UPDATE_BASE + "/" + name when that variable is set (a developer aid),
// else the game repository's releases/latest/download.
wxString ManifestUrl();
wxString DmgUrl();
wxString ZipUrl();
// The release's page on GitHub, for "What's new".
wxString ReleasePage(const wxString& version);

// False (and why in `error`) unless the text has an X.Y.Z version, a format above 0 and both downloads with a
// size above 0 and a SHA-256.
bool ParseManifest(const wxString& text, Manifest& m, wxString& error);

// Whether version `a` is newer than `b`; false unless both are three dot-separated numbers.
bool IsNewer(const wxString& a, const wxString& b);

// Whether to show `latest` to a launcher at version `current`: it's newer, and it isn't the version turned
// down with Later (`skipped`) or an older one, unless the player `asked` (Check for Updates).
bool ShouldOffer(const Manifest& latest, const wxString& current, const wxString& skipped, bool asked);

// Whether `data` has the size and SHA-256 of `d`.
bool Verify(const wxMemoryBuffer& data, const Download& d, wxString& error);

}  // namespace update
