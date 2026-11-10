// test_download.cpp -- runs GameDownload (launcher/download.cpp) and gamefiles::Install for real, without the
// launcher's window. Not part of ctest: it needs a network or a local server.
//
//   test_download <game dir> ok|fail|cancel [download base URL]
//
// The base URL goes into SRE_DOWNLOAD_BASE (omit it for the official site). "ok" expects the game installed
// in <game dir>; "fail" expects an error and nothing installed; "cancel" cancels after half a second (use a
// server that never answers) and expects the download to end as cancelled, with nothing installed.
#include <wx/app.h>
#include <wx/timer.h>
#include <wx/filename.h>
#include <wx/utils.h>

#include <cstdio>

#include "download.h"
#include "gamefiles.h"

class TestApp : public wxAppConsole {
public:
    bool OnInit() override {
        if (argc < 3) {
            std::fprintf(stderr, "usage: test_download <game dir> ok|fail [base URL]\n");
            return false;
        }
        dir_ = argv[1];
        expectOk_ = argv[2] == "ok";
        cancel_ = argv[2] == "cancel";
        if (argc > 3) wxSetEnv("SRE_DOWNLOAD_BASE", argv[3]);
        download_.Start([](const wxString& text) { std::printf("%s\n", static_cast<const char*>(text.utf8_str())); },
                        [this](bool ok, std::vector<gamefiles::Download> downloads, wxString error) {
                            const bool installed = ok && gamefiles::Install(dir_, downloads, error);
                            if (!installed) std::printf("error: %s\n", static_cast<const char*>(error.utf8_str()));
                            const bool game = wxFileName::FileExists(dir_ + "/SR.EXE") &&
                                              wxFileName::FileExists(dir_ + "/datadisk/SRSE.EXE");
                            const bool partial = wxFileName::DirExists(dir_ + ".partial");
                            bool pass = expectOk_ ? installed && game && !partial
                                                  : !installed && !error.empty() && !game && !partial;
                            if (cancel_) pass = pass && !ok && error.Contains("cancelled");
                            std::printf("%s\n", pass ? "PASS" : "FAIL");
                            exitCode_ = pass ? 0 : 1;
                            ExitMainLoop();
                        });
        if (cancel_) {
            timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { download_.Cancel(); });
            timer_.StartOnce(500);
        }
        return true;
    }
    int OnExit() override { return exitCode_; }
    int OnRun() override {
        wxAppConsole::OnRun();
        return exitCode_;
    }

private:
    GameDownload download_;
    wxString dir_;
    bool expectOk_ = false;
    bool cancel_ = false;
    wxTimer timer_;
    int exitCode_ = 1;
};

wxIMPLEMENT_APP_CONSOLE(TestApp);
