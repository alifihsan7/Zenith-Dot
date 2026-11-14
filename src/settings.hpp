#pragma once

#include <array>
#include <string>
#include <windows.h>

namespace zenith {

inline constexpr UINT settingsChangedMessage = WM_APP + 13;
inline constexpr UINT settingsTestMessage = WM_APP + 14;

enum class SettingsTest : unsigned {
    appearance = 1, waveform, lyrics, pause, focus, resting, cycle, copied, copiedText, screenshot,
    camera, microphone, feature, order, notification
};

constexpr bool validSettingsTest(unsigned test) {
    return test >= static_cast<unsigned>(SettingsTest::appearance) && test <= static_cast<unsigned>(SettingsTest::notification);
}

int runSettingsWindow(HINSTANCE instance);

enum class Feature : unsigned { media, timers, scratch, drop, system, prayer, home };
inline constexpr std::array featureOrder{Feature::media, Feature::timers, Feature::scratch, Feature::drop,
                                         Feature::system};
inline constexpr unsigned featureMask = (1u << featureOrder.size()) - 1;

enum class QuietMode : unsigned { off, followWindows, on };
inline constexpr unsigned maxPrayerLeadMinutes = 120;

struct Settings {
    QuietMode quietMode = QuietMode::off;
    unsigned prayerCity = 0;
    bool prayerReminders = true;
    bool prayerSound = true;   // a chime with each prayer reminder
    unsigned prayerLeadMinutes = 0;
    bool showAppNotifications = false;
    bool autoAddText = false;
    bool autoAddScreenshots = true;
    bool showClipboardPreviews = true;
    bool showClipboardBubble = true;
    bool showCameraIndicator = true;
    bool showMicrophoneIndicator = true;
    bool autoCycle = false;   // Focus and Break follow each other automatically
    bool timerSound = true;   // a chime when Focus, Break, or a timer ends
    bool hideInFullscreen = true;
    bool dotOnly = false;   // the island stays a dot; hovering still opens it
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
    unsigned enabledFeatures = featureMask;

    bool enabled(Feature feature) const {
        if (feature == Feature::home) return true;
        if (feature == Feature::prayer) return prayerCity != 0;
        return (enabledFeatures & (1u << static_cast<unsigned>(feature))) != 0;
    }
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
    bool saveNotifications() const;
};

std::wstring startupShortcut();
bool startupEnabled(std::wstring const& shortcut);
bool setStartupEnabled(std::wstring const& shortcut, bool enabled);

}
