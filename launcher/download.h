// download.h -- fetches the official game files (gamefiles::Official()) one after the other, in memory; and
// WebFetch, one file in memory (the art pack's manifest and zip).
#pragma once

#include <wx/event.h>
#include <wx/webrequest.h>

#include <functional>
#include <vector>

#include "gamefiles.h"

class GameDownload : public wxEvtHandler {
public:
    using Done = std::function<void(bool ok, std::vector<gamefiles::Download> downloads, wxString error)>;
    using Progress = std::function<void(const wxString& text)>;  // "Downloading Street Rod SE (2 of 2)..."

    GameDownload();
    // Starts the downloads; `done` is called once, at the end, from the event loop.
    void Start(Progress progress, Done done);
    bool Running() const { return running_; }
    // Stops a running download; `done` is then called with ok = false ("... was cancelled.").
    void Cancel();

private:
    void Next();
    void OnState(wxWebRequestEvent& event);
    void Finish(bool ok, const wxString& error);

    Progress progress_;
    Done done_;
    std::vector<gamefiles::Download> got_;
    wxWebRequest request_;
    bool running_ = false;
    bool cancelled_ = false;
};

class WebFetch : public wxEvtHandler {
public:
    using Done = std::function<void(bool ok, wxMemoryBuffer data, wxString error)>;

    WebFetch();
    // Fetches `url`; `label` names it in errors ("The download of %s failed: ..."). `done` is called once,
    // from the event loop.
    void Start(const wxString& url, const wxString& label, Done done);
    bool Running() const { return running_; }
    // Bytes received so far, and the expected total (-1 if not known yet).
    wxFileOffset Received() const;
    wxFileOffset Expected() const;
    // Stops a running fetch; `done` is then called with ok = false ("... was cancelled.").
    void Cancel();

private:
    void OnState(wxWebRequestEvent& event);
    void Finish(bool ok, wxMemoryBuffer data, const wxString& error);

    Done done_;
    wxString label_;
    wxWebRequest request_;
    bool running_ = false;
};
