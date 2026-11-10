// app.cpp -- the Street Rod Enhanced launcher: the game folder, the cars, the game's switches and the
// display options, then streetrod-enhanced.
#include <wx/app.h>

#include "launcher.h"

class LauncherApp : public wxApp {
public:
    bool OnInit() override {
        SetAppName("Street Rod Enhanced");
        SetVendorName("007hacky007");
        if (!wxApp::OnInit()) return false;
        auto* frame = new LauncherFrame;
        SetTopWindow(frame);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(LauncherApp);
