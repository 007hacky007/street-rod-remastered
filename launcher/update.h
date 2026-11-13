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

// Windows: the launcher's folder `dir` holds the program; the player's Game, Game2 and Remaster folders and
// settings.ini beside it are never touched, only the files the zip holds.
//
// Where StageFolder unpacks the new version: <dir>/.update.
wxString StagingDir(const wxString& dir);
// Checks the zip against `d` and unpacks its "Street Rod Enhanced" folder into StagingDir(dir); `dir` itself
// is untouched. On failure nothing is left staged and `error` says why.
bool StageFolder(const wxString& dir, const wxMemoryBuffer& zip, const Download& d, wxString& error);
// Moves every staged file into `dir`, the file it replaces kept as <name>.old (listed in .update-old.txt for
// CleanFolder; a running program can be renamed, not deleted). On failure everything is put back.
bool ApplyFolder(const wxString& dir, wxString& error);
// Deletes the .old files ApplyFolder listed (those still in use stay listed for next time) and a leftover
// staging folder.
void CleanFolder(const wxString& dir);
// Empty if the launcher can write into `dir`, else what to tell the player.
wxString CantUpdateFolder(const wxString& dir);

}  // namespace update
