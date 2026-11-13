// updatestrip.cpp -- the launcher's own updates in its window (update.h): the strip across the top of the main
// page that says a new version is out, its download, and the restart into the new version. macOS and Windows
// only; elsewhere the strip is never made.
#include <wx/utils.h>
#include <wx/button.h>
#include <wx/hyperlink.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include "launcher.h"
#include "settings.h"
#include "update.h"
#include "version.h"

namespace {

const char* const UPDATE = "Update";  // Skipped: the version turned down with Later

wxString Mb(unsigned long bytes) { return wxString::Format("%lu MB", (bytes + 500000) / 1000000); }

#ifdef __WXOSX__
wxString Bundle() { return update::BundleOf(wxStandardPaths::Get().GetExecutablePath()); }
#endif

}  // namespace

wxWindow* LauncherFrame::CreateUpdateStrip(wxWindow* parent) {
#if SRL_UPDATES
    const int gap = FromDIP(8), small = FromDIP(4);
    strip_ = new wxPanel(parent);
    const bool dark = wxSystemSettings::GetAppearance().IsDark();
    strip_->SetBackgroundColour(dark ? wxColour(0x1d, 0x35, 0x57) : wxColour(0xe3, 0xef, 0xff));
    const wxColour ink = dark ? wxColour(0xf0, 0xf4, 0xff) : wxColour(0x1a, 0x1a, 0x1a);
    stripText_ = new wxStaticText(strip_, wxID_ANY, wxEmptyString);
    stripText_->SetForegroundColour(ink);
    stripLink_ = new wxHyperlinkCtrl(strip_, wxID_ANY, "What's new", wxEmptyString);
    stripUpdate_ = new wxButton(strip_, wxID_ANY, "Update");
    stripLater_ = new wxButton(strip_, wxID_ANY, "Later");
    stripCancel_ = new wxButton(strip_, wxID_ANY, "Cancel");
    stripRetry_ = new wxButton(strip_, wxID_ANY, "Try Again");
    stripRestart_ = new wxButton(strip_, wxID_ANY, "Restart");
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(stripText_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, small);
    row->Add(stripLink_, 0, wxALIGN_CENTER_VERTICAL);
    row->AddStretchSpacer();
    for (wxButton* b : {stripUpdate_, stripRetry_, stripRestart_, stripLater_, stripCancel_})
        row->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, small);
    auto* box = new wxBoxSizer(wxVERTICAL);
    box->Add(row, 0, wxEXPAND | wxALL, gap);
    strip_->SetSizer(box);
    strip_->Hide();

    stripUpdate_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AskUpdate(); });
    stripRetry_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { StartUpdate(); });
    stripRestart_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RestartIntoUpdate(); });
    stripLater_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        settings::SetString(UPDATE, "Skipped", updateLatest_.version);
        ShowStrip(STRIP_HIDDEN);
    });
    stripCancel_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        updateCancelled_ = true;
        updateFetch_.Cancel();
    });
    updateTimer_.Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        const unsigned long all = updateLatest_.version.empty() ? 0 : UpdateSize();
        const wxFileOffset got = updateFetch_.Received();
        stripText_->SetLabel(all && got > 0
                                 ? wxString::Format(wxString::FromUTF8("Downloading %s… %lu of %s"),
                                                    updateLatest_.version, (unsigned long)(got + 500000) / 1000000,
                                                    Mb(all))
                                 : wxString::FromUTF8("Downloading ") + updateLatest_.version +
                                       wxString::FromUTF8("…"));
        strip_->Layout();
    });
    return strip_;
#else
    (void)parent;
    return nullptr;
#endif
}

unsigned long LauncherFrame::UpdateSize() const {
#ifdef __WXOSX__
    return updateLatest_.mac.size;
#else
    return updateLatest_.windows.size;
#endif
}

void LauncherFrame::ShowStrip(StripStage stage, const wxString& text) {
    if (!strip_) return;
    stripStage_ = stage;
    const wxString v = updateLatest_.version;
    switch (stage) {
    case STRIP_OFFER: stripText_->SetLabel("Street Rod Enhanced " + v + " is out."); break;
    case STRIP_DOWNLOADING:
        stripText_->SetLabel(wxString::FromUTF8("Downloading ") + v + wxString::FromUTF8("…"));
        break;
    case STRIP_FAILED: stripText_->SetLabel("The update failed: " + text); break;
    case STRIP_READY: stripText_->SetLabel(v + " is ready."); break;
    case STRIP_HIDDEN: break;
    }
    if (stage == STRIP_FAILED) stripText_->Wrap(FromDIP(460));
    stripLink_->SetURL(update::ReleasePage(v));
    stripLink_->Show(stage == STRIP_OFFER);
    stripUpdate_->Show(stage == STRIP_OFFER);
    stripRetry_->Show(stage == STRIP_FAILED);
    stripRestart_->Show(stage == STRIP_READY);
    stripLater_->Show(stage == STRIP_OFFER || stage == STRIP_FAILED);
    stripCancel_->Show(stage == STRIP_DOWNLOADING);
    strip_->Show(stage != STRIP_HIDDEN);
    strip_->Layout();
    wxWindow* page = strip_->GetParent();
    page->InvalidateBestSize();
    page->Layout();
    if (book_->GetSelection() == 0) FitPage();
}

void LauncherFrame::CleanUpdateLeftovers() {
#ifdef __WXOSX__
    const wxString bundle = Bundle();
    if (!bundle.empty()) update::CleanBundle(bundle);
#elif defined(__WXMSW__)
    update::CleanFolder(LauncherDir());
#endif
}

void LauncherFrame::CheckUpdate(bool asked) {
    if (!strip_) return;
    if (updateFetch_.Running() || stripStage_ == STRIP_DOWNLOADING || stripStage_ == STRIP_READY) {
        if (asked && stripStage_ != STRIP_HIDDEN) Raise();
        return;
    }
    updateFetch_.Start(update::ManifestUrl(), "the description of the new version",
                       [this, asked](bool ok, wxMemoryBuffer data, wxString error) {
                           if (FinishClosing()) return;
                           update::Manifest m;
                           if (ok)
                               ok = update::ParseManifest(
                                   wxString::FromUTF8(static_cast<const char*>(data.GetData()), data.GetDataLen()),
                                   m, error);
                           if (!ok) {
                               // Offline or no manifest: quiet unless the player asked.
                               if (asked)
                                   wxMessageBox("Couldn't check for updates.\n\n" + error, APP_TITLE,
                                                wxOK | wxICON_WARNING, this);
                               return;
                           }
                           updateLatest_ = m;
                           if (update::ShouldOffer(m, APP_VERSION_TEXT, settings::GetString(UPDATE, "Skipped", ""),
                                                   asked))
                               ShowStrip(STRIP_OFFER);
                           else if (asked)
                               wxMessageBox(wxString(APP_TITLE) + " " APP_VERSION_TEXT " is the latest version.",
                                            APP_TITLE, wxOK | wxICON_INFORMATION, this);
                       });
}

wxString LauncherFrame::CantUpdateHere() const {
#ifdef __WXOSX__
    return update::CantUpdateBundle(Bundle());
#elif defined(__WXMSW__)
    // A build folder isn't an install: the zip puts "Read me.txt" beside the launcher.
    if (!wxFileExists(LauncherDir() + "\\Read me.txt"))
        return "This copy of Street Rod Enhanced isn't an installed app.";
    return update::CantUpdateFolder(LauncherDir());
#else
    return "Street Rod Enhanced can't update itself here.";
#endif
}

void LauncherFrame::AskUpdate() {
    wxString why = update::ManualReason(updateLatest_);
    if (why.empty()) why = CantUpdateHere();
    if (why.empty()) return StartUpdate();
    OfferManualDownload(why);
}

void LauncherFrame::OfferManualDownload(const wxString& why) {
    wxMessageDialog dialog(this, why, APP_TITLE, wxOK | wxCANCEL | wxICON_INFORMATION);
    dialog.SetOKCancelLabels("Open GitHub", "Cancel");
    if (dialog.ShowModal() == wxID_OK) wxLaunchDefaultBrowser(update::ReleasePage(updateLatest_.version));
}

void LauncherFrame::StartUpdate() {
    if (updateFetch_.Running()) return;
    updateCancelled_ = false;
    ShowStrip(STRIP_DOWNLOADING);
    updateTimer_.Start(250);
    // The manifest again, so the download is checked against the release it belongs to.
    updateFetch_.Start(update::ManifestUrl(), "the description of the new version",
                       [this](bool ok, wxMemoryBuffer data, wxString error) {
                           if (FinishClosing()) return;
                           update::Manifest m;
                           if (ok)
                               ok = update::ParseManifest(
                                   wxString::FromUTF8(static_cast<const char*>(data.GetData()), data.GetDataLen()),
                                   m, error);
                           if (!ok) return UpdateFailed(error);
                           if (!update::IsNewer(m.version, APP_VERSION_TEXT)) {
                               updateTimer_.Stop();
                               return ShowStrip(STRIP_HIDDEN);
                           }
                           updateLatest_ = m;
                           const wxString manual = update::ManualReason(m);
                           if (!manual.empty()) {
                               updateTimer_.Stop();
                               ShowStrip(STRIP_OFFER);
                               return OfferManualDownload(manual);
                           }
#ifdef __WXOSX__
                           const wxString url = update::DmgUrl();
#else
                           const wxString url = update::ZipUrl();
#endif
                           updateFetch_.Start(url, "the new version", [this, m](bool ok, wxMemoryBuffer file,
                                                                                wxString error) {
                               if (FinishClosing()) return;
                               updateTimer_.Stop();
                               if (!ok) return UpdateFailed(error);
                               wxBusyCursor busy;
#ifdef __WXOSX__
                               ok = update::StageBundle(Bundle(), file, m, error);
#elif defined(__WXMSW__)
                               ok = update::StageFolder(LauncherDir(), file, m.windows, error);
#endif
                               if (!ok) return UpdateFailed(error);
                               ShowStrip(STRIP_READY);
                           });
                       });
}

void LauncherFrame::UpdateFailed(const wxString& error) {
    updateTimer_.Stop();
    if (updateCancelled_) {
        updateCancelled_ = false;
        return ShowStrip(STRIP_OFFER);
    }
    ShowStrip(STRIP_FAILED, error);
}

// A swap that fails here fails again on Try Again (a locked file, macOS refusing): the release page instead.
void LauncherFrame::SwapFailed(const wxString& error) {
    ShowStrip(STRIP_FAILED, error);
    OfferManualDownload(error);
}

void LauncherFrame::RestartIntoUpdate() {
    wxString error;
    updateCancelled_ = false;
    Save();
#ifdef __WXOSX__
    const wxString bundle = Bundle();
    if (!update::ApplyBundle(bundle, error)) return SwapFailed(error);
    const wxCharBuffer path = bundle.utf8_str();
    const char* argv[] = {"/usr/bin/open", "-n", path.data(), nullptr};
    const bool started = wxExecute(argv, wxEXEC_ASYNC) != 0;
#elif defined(__WXMSW__)
    if (!update::ApplyFolder(LauncherDir(), error)) return SwapFailed(error);
    wxExecuteEnv env;
    env.cwd = LauncherDir();
    const wxString exe = LauncherDir() + "\\Street Rod Enhanced.exe";
    const wchar_t* argv[] = {exe.wc_str(), nullptr};
    const bool started = wxExecute(argv, wxEXEC_ASYNC, nullptr, &env) != 0;
#else
    const bool started = false;
#endif
    if (!started)
        wxMessageBox("Street Rod Enhanced " + updateLatest_.version + " is installed. Start it again to use it.",
                     APP_TITLE, wxOK | wxICON_INFORMATION, this);
    Close();
}
