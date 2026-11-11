// artpack.h -- the remastered pictures as a download: the art repository's latest release (a manifest,
// then the zip), whether to offer it, and installing it into a folder, replacing what was there.
#pragma once

#include <wx/buffer.h>
#include <wx/string.h>

namespace artpack {

// The newest pack.json format this launcher reads.
constexpr int FORMAT = 1;

// Street-Rod-Remastered-art.txt: "key=value" lines. Also what Install() records beside the pack.
struct Manifest {
    wxString version;        // the art release, "v1"
    wxString sha256;         // of the zip, lower case
    unsigned long size = 0;  // of the zip, bytes
    int format = 0;          // of its pack.json
};

// The latest release's files: SRE_ART_BASE + "/" + name when that variable is set (a developer aid),
// else the art repository's releases/latest/download.
wxString ManifestUrl();
wxString ZipUrl();

// False (and why in `error`) unless the text has a version, a format, a size above 0 and a SHA-256.
bool ParseManifest(const wxString& text, Manifest& m, wxString& error);

// The release a folder was installed from (Install() records it), or an empty Manifest.
Manifest Installed(const wxString& dir);

enum OfferKind { OFFER_NONE, OFFER_DOWNLOAD, OFFER_UPDATE };
// What to ask the player: `havePack` whether any pack is found, `installed` what Installed() says of it,
// `declined` whether the first offer was already turned down, `skipped` the SHA-256 of an update turned down.
OfferKind Offer(bool havePack, const Manifest& installed, const Manifest& latest, bool declined,
                const wxString& skipped);

// Checks the zip against `m`, unpacks its Remaster folder into <dir>.partial, checks the pack.json, records
// `m` and puts it in place of `dir`. On failure `dir` is as it was and `error` says why.
bool Install(const wxString& dir, const wxMemoryBuffer& zip, const Manifest& m, wxString& error);

}  // namespace artpack
