#pragma once

#include <string>
#include <windows.h>

namespace zenith {

inline constexpr UINT settingsChangedMessage = WM_APP + 13;

int runSettingsWindow(HINSTANCE instance);

struct Settings {
    bool autoAddText = false;
    bool autoAddScreenshots = true;
    bool showClipboardPreviews = true;
    bool autoCycle = false;   // Focus and Break follow each other automatically

    static Settings load();
    bool saveClipboard() const;
    bool saveTimer() const;
};

std::wstring startupShortcut();
bool startupEnabled(std::wstring const& shortcut);
bool setStartupEnabled(std::wstring const& shortcut, bool enabled);

}
