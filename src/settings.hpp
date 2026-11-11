#pragma once

#include <array>
#include <string>
#include <windows.h>

namespace zenith {

inline constexpr UINT settingsChangedMessage = WM_APP + 13;

int runSettingsWindow(HINSTANCE instance);

enum class Feature : unsigned { media, timers, scratch, drop, system };
inline constexpr std::array featureOrder{Feature::media, Feature::timers, Feature::scratch, Feature::drop, Feature::system};

struct Settings {
    bool autoAddText = false;
    bool autoAddScreenshots = true;
    bool showClipboardPreviews = true;
    bool showClipboardBubble = true;
    bool showCameraIndicator = true;
    bool showMicrophoneIndicator = true;
    bool autoCycle = false;   // Focus and Break follow each other automatically
    bool animateIsland = true;
    bool hideInFullscreen = true;
    bool fixedPillMinimum = false;
    unsigned islandSize = 1;
    unsigned accentMode = 0;
    unsigned customAccent = 0x91caff;
    bool showWaveform = true;
    bool showPillLyrics = true;
    unsigned compactPause = 1;
    unsigned focusPreset = 1;
    unsigned breakPreset = 0;
    std::array<Feature, featureOrder.size()> features = featureOrder;
    unsigned enabledFeatures = 0x1f;

    bool enabled(Feature feature) const { return (enabledFeatures & (1u << static_cast<unsigned>(feature))) != 0; }
    unsigned pauseSeconds() const { return std::array{0u, 3u, 5u, 0u}[compactPause]; }
    int focusMinutes() const { return std::array{15, 25, 45}[focusPreset]; }
    int breakMinutes() const { return std::array{5, 10, 15}[breakPreset]; }
    float islandScale() const { return std::array{0.88f, 1.0f, 1.12f}[islandSize]; }

    static Settings load();
    bool saveClipboard() const;
    bool savePrivacy() const;
    bool saveTimer() const;
    bool saveDisplay() const;
    bool saveIsland() const;
    bool saveFeatures() const;
    bool saveAccent() const;
};

std::wstring startupShortcut();
bool startupEnabled(std::wstring const& shortcut);
bool setStartupEnabled(std::wstring const& shortcut, bool enabled);

}
