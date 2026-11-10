// download.cpp -- fetching the official game files (download.h).
#include "download.h"

#include <wx/mstream.h>

#include "version.h"

GameDownload::GameDownload() { Bind(wxEVT_WEBREQUEST_STATE, &GameDownload::OnState, this); }

void GameDownload::Start(Progress progress, Done done) {
    if (running_) return;
    progress_ = std::move(progress);
    done_ = std::move(done);
    got_.clear();
    running_ = true;
    Next();
}

void GameDownload::Next() {
    const auto& all = gamefiles::Official();
    if (got_.size() == all.size()) {
        Finish(true, wxEmptyString);
        return;
    }
    const gamefiles::Archive& a = all[got_.size()];
    if (progress_)
        progress_(wxString::Format("Downloading %s (%d of %d)...", a.label, int(got_.size()) + 1, int(all.size())));
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
    const gamefiles::Archive& a = gamefiles::Official()[got_.size()];
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
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
    request_ = wxWebRequest();
    std::vector<gamefiles::Download> got = std::move(got_);
    got_.clear();
    Done done = std::move(done_);
    if (done) done(ok, std::move(got), error);
}
