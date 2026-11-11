// download.cpp -- fetching the official game files (download.h).
#include "download.h"

#include <wx/mstream.h>

#include "version.h"

GameDownload::GameDownload() { Bind(wxEVT_WEBREQUEST_STATE, &GameDownload::OnState, this); }

void GameDownload::Start(Game game, Progress progress, Done done) {
    if (running_) return;
    game_ = game;
    progress_ = std::move(progress);
    done_ = std::move(done);
    got_.clear();
    running_ = true;
    cancelled_ = false;
    Next();
}

void GameDownload::Cancel() {
    if (!running_ || cancelled_) return;
    cancelled_ = true;
    if (request_.IsOk()) request_.Cancel();  // State_Cancelled follows
    else CallAfter([this] { Next(); });      // between two requests: Next() finishes
}

void GameDownload::Next() {
    const auto& all = gamefiles::Official(game_);
    if (cancelled_) {
        Finish(false, wxString::Format("The download of %s was cancelled.", all[got_.size() % all.size()].label));
        return;
    }
    if (got_.size() == all.size()) {
        Finish(true, wxEmptyString);
        return;
    }
    const gamefiles::Archive& a = all[got_.size()];
    if (progress_)
        progress_(all.size() == 1 ? wxString::Format("Downloading %s...", a.label)
                                  : wxString::Format("Downloading %s (%d of %d)...", a.label, int(got_.size()) + 1,
                                                     int(all.size())));
    request_ = wxWebSession::GetDefault().CreateRequest(this, gamefiles::Url(a));
    if (!request_.IsOk()) {
        Finish(false, wxString::Format("Couldn't start the download of %s.", a.label));
        return;
    }
    request_.SetHeader("User-Agent", "StreetRodEnhanced/" APP_VERSION_TEXT);
    request_.SetStorage(wxWebRequest::Storage_Memory);
    request_.Start();
}

void GameDownload::OnState(wxWebRequestEvent& event) {
    if (!running_) return;
    const gamefiles::Archive& a = gamefiles::Official(game_)[got_.size()];
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        if (cancelled_) {
            Finish(false, wxString::Format("The download of %s was cancelled.", a.label));
            return;
        }
        const wxWebResponse& response = event.GetResponse();
        if (response.GetStatus() != 200) {
            Finish(false, wxString::Format("The download of %s failed: the website answered %d %s.", a.label,
                                           response.GetStatus(), response.GetStatusText()));
            return;
        }
        wxMemoryOutputStream out;
        if (wxInputStream* in = response.GetStream()) in->Read(out);
        gamefiles::Download d{&a, {}};
        d.data.AppendData(out.GetOutputStreamBuffer()->GetBufferStart(), out.GetSize());
        got_.push_back(std::move(d));
        CallAfter([this] { Next(); });  // not from inside the request's own event
        break;
    }
    case wxWebRequest::State_Failed:
    case wxWebRequest::State_Unauthorized:
        Finish(false, wxString::Format("The download of %s failed: %s", a.label,
                                       event.GetErrorDescription().empty() ? wxString("no connection.")
                                                                           : event.GetErrorDescription()));
        break;
    case wxWebRequest::State_Cancelled:
        Finish(false, wxString::Format("The download of %s was cancelled.", a.label));
        break;
    default:
        break;
    }
}

void GameDownload::Finish(bool ok, const wxString& error) {
    running_ = false;
    cancelled_ = false;
    request_ = wxWebRequest();
    std::vector<gamefiles::Download> got = std::move(got_);
    got_.clear();
    Done done = std::move(done_);
    if (done) done(ok, std::move(got), error);
}

WebFetch::WebFetch() { Bind(wxEVT_WEBREQUEST_STATE, &WebFetch::OnState, this); }

void WebFetch::Start(const wxString& url, const wxString& label, Done done) {
    if (running_) return;
    done_ = std::move(done);
    label_ = label;
    running_ = true;
    request_ = wxWebSession::GetDefault().CreateRequest(this, url);
    if (!request_.IsOk()) {
        CallAfter([this] { Finish(false, {}, wxString::Format("Couldn't start the download of %s.", label_)); });
        return;
    }
    request_.SetHeader("User-Agent", "StreetRodEnhanced/" APP_VERSION_TEXT);
    request_.SetStorage(wxWebRequest::Storage_Memory);
    request_.Start();
}

wxFileOffset WebFetch::Received() const { return request_.IsOk() ? request_.GetBytesReceived() : 0; }

wxFileOffset WebFetch::Expected() const { return request_.IsOk() ? request_.GetBytesExpectedToReceive() : -1; }

void WebFetch::Cancel() {
    if (running_ && request_.IsOk()) request_.Cancel();  // State_Cancelled follows
}

void WebFetch::OnState(wxWebRequestEvent& event) {
    if (!running_) return;
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        const wxWebResponse& response = event.GetResponse();
        if (response.GetStatus() != 200) {
            Finish(false, {}, wxString::Format("The download of %s failed: the website answered %d %s.", label_,
                                               response.GetStatus(), response.GetStatusText()));
            return;
        }
        wxMemoryOutputStream out;
        if (wxInputStream* in = response.GetStream()) in->Read(out);
        wxMemoryBuffer data;
        data.AppendData(out.GetOutputStreamBuffer()->GetBufferStart(), out.GetSize());
        Finish(true, std::move(data), wxEmptyString);
        break;
    }
    case wxWebRequest::State_Failed:
    case wxWebRequest::State_Unauthorized:
        Finish(false, {}, wxString::Format("The download of %s failed: %s", label_,
                                           event.GetErrorDescription().empty() ? wxString("no connection.")
                                                                               : event.GetErrorDescription()));
        break;
    case wxWebRequest::State_Cancelled:
        Finish(false, {}, wxString::Format("The download of %s was cancelled.", label_));
        break;
    default:
        break;
    }
}

void WebFetch::Finish(bool ok, wxMemoryBuffer data, const wxString& error) {
    running_ = false;
    Done done = std::move(done_);
    // Not from inside the request's own event: `done` may start the next fetch.
    CallAfter([done, ok, data, error] {
        if (done) done(ok, data, error);
    });
    request_ = wxWebRequest();
}
