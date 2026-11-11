// gamefolder.cpp -- see gamefolder.h.
#include "gamefolder.h"

#include <wx/filename.h>

namespace {

// Whether `dir` holds the file `name` as written, in upper or in lower case (case matters outside
// Windows; the port itself finds files whatever their case).
bool FilePresent(const wxString& dir, const wxString& name) {
    if (dir.empty()) return false;
    for (const wxString& n : {name, name.Upper(), name.Lower()})
        if (wxFileName::FileExists(wxFileName(dir, n).GetFullPath())) return true;
    return false;
}

// The first file Street Rod's port needs that `dir` lacks (SR.EXE or SRSE.EXE, LIB1, LIB2, HOT_DATA).
wxString Missing(const wxString& dir, bool* se) {
    *se = !FilePresent(dir, "SR.EXE") && FilePresent(dir, "SRSE.EXE");
    if (!*se && !FilePresent(dir, "SR.EXE")) return "SR.EXE";
    for (const char* name : {"LIB1", "LIB2", "HOT_DATA"})
        if (!FilePresent(dir, name)) return name;
    return wxString();
}

// The first file Street Rod 2's port needs that `dir` lacks.
wxString Missing2(const wxString& dir) {
    for (const char* name : {"SR.EXE", "LIB1", "LIB2", "LIB3", "SR2_DATA", "SR2_END", "SR2_SHP", "MSR.SND"})
        if (!FilePresent(dir, name)) return name;
    return wxString();
}

}  // namespace

GameFolder ReadGameFolder(const wxString& dir, Game game) {
    GameFolder f;
    if (dir.empty()) {
        f.missing = "SR.EXE";
        return f;
    }
    if (game == Game::SR2) {
        f.missing = Missing2(dir);
        for (int n = 0; n <= 14; ++n)   // save_slots_scan: HOTROD0..14.SAV
            if (FilePresent(dir, wxString::Format("HOTROD%d.SAV", n))) ++f.saves;
        f.otherGame = FilePresent(dir, "HOT_DATA") && !FilePresent(dir, "LIB3");
        return f;
    }
    f.missing = Missing(dir, &f.se);
    for (int n = 1; n <= 15; ++n)
        if (FilePresent(dir, wxString::Format("HOTROD%d.SAV", n))) ++f.saves;
    const wxString disk = wxFileName(dir, "datadisk").GetFullPath();
    bool se = false;
    if (wxFileName::DirExists(disk) && Missing(disk, &se).empty()) f.dataDisk = disk;
    f.otherGame = FilePresent(dir, "SR2_DATA");
    return f;
}
