#pragma once

#include <algorithm>
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

struct Rhythm {
    wchar_t const* name;
    int focus;
    int rest;
    int rounds;
};
inline constexpr Rhythm rhythms[]{{L"Pomodoro", 25, 5, 4}, {L"Deep work", 50, 10, 3}, {L"Long focus", 90, 20, 2}};
inline constexpr unsigned customRhythm = 3;
inline constexpr unsigned minFocusMinutes = 5, maxFocusMinutes = 180;
inline constexpr unsigned minBreakMinutes = 1, maxBreakMinutes = 60;
inline constexpr unsigned minRounds = 1, maxRounds = 8;

// "1h 40m", "45m", "2h".
inline std::wstring durationText(int minutes) {
    const int hours = minutes / 60, rest = minutes % 60;
    if (!hours) return std::to_wstring(rest) + L"m";
    return std::to_wstring(hours) + L"h" + (rest ? L" " + std::to_wstring(rest) + L"m" : L"");
}

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
    // Focus sessions: a template, or Custom with its own lengths and rounds.
    unsigned rhythm = 0;            // 0 Pomodoro, 1 Deep work, 2 Long focus, 3 Custom
    unsigned customFocus = 40;      // minutes, 5..180
    unsigned customBreak = 8;       // minutes, 1..60
    unsigned customRounds = 4;      // 1..8
    std::array<Feature, featureOrder.size()> features = featureOrder;
    unsigned enabledFeatures = featureMask;

    bool enabled(Feature feature) const {
        if (feature == Feature::home) return true;
        if (feature == Feature::prayer) return prayerCity != 0;
        return (enabledFeatures & (1u << static_cast<unsigned>(feature))) != 0;
    }
    unsigned pauseSeconds() const { return std::array{0u, 3u, 5u, 0u}[compactPause]; }
    int focusMinutes() const { return rhythm == customRhythm ? static_cast<int>(customFocus) : rhythms[rhythm].focus; }
    int breakMinutes() const { return rhythm == customRhythm ? static_cast<int>(customBreak) : rhythms[rhythm].rest; }
    int focusRounds() const { return rhythm == customRhythm ? static_cast<int>(customRounds) : rhythms[rhythm].rounds; }
    // The break after the last round is three times as long, up to 30 minutes.
    int longBreakMinutes() const { return std::min(3 * breakMinutes(), 30); }
    int sessionMinutes() const {
        return focusRounds() * focusMinutes() + (focusRounds() - 1) * breakMinutes() + longBreakMinutes();
    }
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
