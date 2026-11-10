#pragma once

#include <string>

namespace zenith {

struct Settings {
    bool autoAddText = false;
    bool autoAddScreenshots = true;
    bool showClipboardPreviews = true;

    static Settings load();
    bool save() const;
};

std::wstring startupShortcut();
bool startupEnabled(std::wstring const& shortcut);
bool setStartupEnabled(std::wstring const& shortcut, bool enabled);

}
