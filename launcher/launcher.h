// launcher.h -- the launcher window: choose the game (Street Rod or Street Rod 2), its folder and, for Street
// Rod, the cars (the original ones or the Street Rod SE data disk's), the game's own switches and the display
// options, then Play. Game settings > Sounds shows its page in the window.
#pragma once

#include <wx/frame.h>
#include <wx/timer.h>

#include "artpack.h"
#include "download.h"
#include "game.h"
#include "update.h"

// The launcher updates itself where there's a release to update from: the macOS app and the Windows zip.
#if defined(__WXOSX__) || defined(__WXMSW__)
#define SRL_UPDATES 1
#else
#define SRL_UPDATES 0
#endif

class wxButton;
class wxCheckBox;
class wxChoice;
class wxHyperlinkCtrl;
class wxPanel;
class wxSimplebook;
class wxSlider;
class wxSpinCtrl;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

extern const char* const APP_TITLE;

class LauncherFrame : public wxFrame {
public:
    LauncherFrame();

private:
    wxWindow* CreateMainPage(wxWindow* parent);
    wxWindow* CreateSoundsPage(wxWindow* parent);
    void CreateMenus();
#ifdef __WXMSW__
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

    // Checks the folder and the program again and enables Play and the controls to match.
    void UpdateState();
    void BrowseFolder();
    void BrowseProgram();
    // macOS: the official game files into the folder when it's free (gamefiles.h).
    void OfferDownload();
    void StartDownload();
    void DownloadFailed(const wxString& error);
    // The remastered pictures (artpack.h): CheckArt reads the latest release's manifest quietly and, when
    // `ask`, offers the pictures or an update; StartArtDownload fetches the manifest and the zip and installs
    // them into UserRemasterDir().
    void CheckArt(bool ask);
    void AskArt(artpack::OfferKind kind);
    void StartArtDownload();
    void ArtFailed(const wxString& error);
    // The launcher's own updates (updatestrip.cpp): the strip across the top of the main page; CheckUpdate
    // reads the latest release's manifest quietly unless `asked` (Check for Updates).
    enum StripStage { STRIP_HIDDEN, STRIP_OFFER, STRIP_DOWNLOADING, STRIP_FAILED, STRIP_READY };
    wxWindow* CreateUpdateStrip(wxWindow* parent);  // null where the launcher can't update
    void ShowStrip(StripStage stage, const wxString& text = wxString());
    void CleanUpdateLeftovers();
    void CheckUpdate(bool asked);
    // Empty if this copy can update itself, else why not.
    wxString CantUpdateHere() const;
    void AskUpdate();
    void StartUpdate();
    void UpdateFailed(const wxString& error);
    void RestartIntoUpdate();
    unsigned long UpdateSize() const;  // this platform's download
    // After a download ends while the window is closing: destroys it once nothing runs any more.
    bool FinishClosing();
    void Play();
    void About();
    void Preferences();
    void SetTopmost(bool on);
    void Save();
    // The chosen game's own settings (Traits(game).section) from / to the controls.
    void LoadGame();
    void SaveGame();
    // The Game choice changed.
    void SwitchGame();
    // The folder the game runs in: the chosen folder, or its datadisk folder for the SE cars.
    wxString PlayFolder() const;

    void FitPage();
    // A settings page in place of the main one, and back.
    void OpenPage(int page);
    void ClosePage();
    // Sounds: the controls from / to a Sounds.
    void ShowSounds(const Sounds& s);
    Sounds EditedSounds() const;

    wxSimplebook* book_ = nullptr;
    wxChoice* gameChoice_ = nullptr;   // Street Rod / Street Rod 2
    Game current_ = Game::SR1;
    bool offered_[2] = {false, false};  // macOS: the download was offered this run, per game
    wxTextCtrl* folder_ = nullptr;
    wxTextCtrl* program_ = nullptr;
    wxButton* browseFolder_ = nullptr;
    wxButton* browseProgram_ = nullptr;
    wxButton* download_ = nullptr;  // macOS only
    GameDownload downloader_;
    wxString downloadNote_;         // the download's progress, shown while it runs
    bool closing_ = false;          // closed mid-download: destroyed once the cancelled download ends
    wxStaticBitmap* statusIcon_ = nullptr;
    wxStaticText* statusNote_ = nullptr;
    wxStaticText* carsLabel_ = nullptr;
    wxChoice* cars_ = nullptr;     // the original cars / the data disk's
    wxStaticText* carsNote_ = nullptr;
    wxCheckBox* demo_ = nullptr;
    wxCheckBox* autoDrive_ = nullptr;
    wxCheckBox* noMouse_ = nullptr;
    wxChoice* scale_ = nullptr;
    wxChoice* frameRate_ = nullptr;  // the display's refresh rate / uncapped
    wxStaticText* soundLabel_ = nullptr;
    wxChoice* sound_ = nullptr;      // Street Rod 2: AdLib / PC speaker
    wxStaticText* gearKeys_[2] = {nullptr, nullptr};  // the A / Z row of the keys
    wxStaticText* pictureNote_ = nullptr;
    wxCheckBox* fullscreen_ = nullptr;
    wxChoice* graphics_ = nullptr;   // race view: enhanced / original
    wxChoice* art_ = nullptr;        // art: original / remastered (with an art pack)
    wxButton* artButton_ = nullptr;  // Download / Update pictures, and the download's progress
    WebFetch artFetch_;
    wxTimer artTimer_;               // the progress on artButton_
    artpack::Manifest artLatest_;    // the latest release, once CheckArt has read it
    bool artKnown_ = false;
    bool artDownloading_ = false;    // the zip, not only the manifest
    wxPanel* strip_ = nullptr;
    wxStaticText* stripText_ = nullptr;
    wxHyperlinkCtrl* stripLink_ = nullptr;
    wxButton* stripUpdate_ = nullptr;
    wxButton* stripLater_ = nullptr;
    wxButton* stripCancel_ = nullptr;
    wxButton* stripRetry_ = nullptr;
    wxButton* stripRestart_ = nullptr;
    StripStage stripStage_ = STRIP_HIDDEN;
    WebFetch updateFetch_;
    wxTimer updateTimer_;             // the download's progress on the strip
    update::Manifest updateLatest_;   // the latest release, once CheckUpdate has read it
    bool updateCancelled_ = false;    // Cancel, not a failure
    wxChoice* resolution_ = nullptr;
    wxChoice* aa_ = nullptr;
    wxCheckBox* town_ = nullptr;
    wxButton* play_ = nullptr;
    // The Sounds page: a slider and a number field for each volume.
    wxSlider* musicSlider_ = nullptr;
    wxChoice* music_ = nullptr;      // main window: the music, remastered / original
    wxSpinCtrl* musicField_ = nullptr;
    wxSlider* effectsSlider_ = nullptr;
    wxSpinCtrl* effectsField_ = nullptr;
    Sounds sounds_;  // saved

    GameFolder game_;
    bool topmost_ = false;
    bool loading_ = true;  // no edits are recorded while the window is built
};
