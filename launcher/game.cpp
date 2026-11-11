// game.cpp -- the default folders and programs, and starting the game.
#include "game.h"

#include <wx/file.h>

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <vector>

#include "gamefiles.h"
#include "settings.h"

wxString LauncherDir() { return wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath(); }

wxString DefaultGameDir(Game game) {
#ifdef __WXOSX__
    // The app bundle stays read-only: the game and its saved games live with the settings.
    return wxFileName(settings::Folder(), Traits(game).folder).GetFullPath();
#else
    return wxFileName(LauncherDir(), Traits(game).folder).GetFullPath();
#endif
}

wxString DefaultProgram(Game game) {
    wxFileName name(LauncherDir(), Traits(game).program);
#ifdef __WXMSW__
    name.SetExt("exe");
#endif
    return name.GetFullPath();
}

bool IsDosProgram(const wxString& path) {
    wxFile f;
    if (!wxFileName::FileExists(path) || !f.Open(path)) return false;
    unsigned char h[0x40] = {};
    if (f.Read(h, sizeof h) != (ssize_t)sizeof h || h[0] != 'M' || h[1] != 'Z') return false;
    const wxFileOffset pe = h[0x3C] | h[0x3D] << 8 | h[0x3E] << 16 | (wxFileOffset)h[0x3F] << 24;
    unsigned char sig[4] = {};
    if (pe <= 0 || pe >= f.Length() - 4 || f.Seek(pe) != pe || f.Read(sig, 4) != 4) return true;
    return !(sig[0] == 'P' && sig[1] == 'E' && sig[2] == 0 && sig[3] == 0);
}

wxString UserRemasterDir() { return settings::Folder() + wxFILE_SEP_PATH + "Remaster"; }

wxString DefaultRemasterDir() {
    const wxString user = UserRemasterDir();
    const wxString found = gamefiles::FirstArtPack({user, LauncherDir() + wxFILE_SEP_PATH + "Remaster"});
    return found.empty() ? user : found;
}

bool IsArtPack(const wxString& dir) {
    return !dir.empty() && wxFileName::FileExists(dir + wxFILE_SEP_PATH + "pack.json");
}

bool LaunchGame(const GameOptions& o, wxString& error) {
    if (!wxFileName::FileExists(o.program)) {
        error = wxString::Format("The game's program isn't there:\n\n%s", o.program);
        return false;
    }
    if (IsDosProgram(o.program)) {
        error = wxString::Format("%s is the original DOS game, which Windows can't run. The program to start "
                                 "is %s.exe:\n\n%s", wxFileName(o.program).GetFullName(), Traits(o.game).program,
                                 DefaultProgram(o.game));
        return false;
    }
    const std::vector<wxString> args = GameArgs(o);

    std::vector<std::wstring> wide;
    for (const wxString& a : args) wide.push_back(a.ToStdWstring());
    std::vector<const wchar_t*> argv;
    for (const std::wstring& w : wide) argv.push_back(w.c_str());
    argv.push_back(nullptr);

    wxExecuteEnv env;  // an empty variable map: the game inherits the launcher's environment
    env.cwd = wxFileName(o.program).GetPath();
    long pid;
    {
        wxLogNull quiet;  // wxExecute would show its own error box
        pid = wxExecute(argv.data(), wxEXEC_ASYNC, nullptr, &env);
    }
    if (pid == 0) {
        error = wxString::Format("Couldn't start %s.\n\n%s", wxFileName(o.program).GetFullName(),
                                 wxSysErrorMsgStr(wxSysErrorCode()));
        return false;
    }
    return true;
}
