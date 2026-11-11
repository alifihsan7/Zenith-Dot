#include "settings.hpp"

#include <windows.h>
#include <shlobj.h>
#include <winrt/base.h>

#include <optional>

namespace zenith {
namespace {

constexpr wchar_t settingsKey[] = L"Software\\Zenith Dot";
constexpr wchar_t clipboardValue[] = L"ClipboardOptions";
constexpr DWORD autoAddTextBit = 1;
constexpr DWORD autoAddScreenshotsBit = 2;
constexpr DWORD showClipboardPreviewsBit = 4;
constexpr wchar_t timerValue[] = L"TimerOptions";
constexpr DWORD autoCycleBit = 1;
constexpr wchar_t displayValue[] = L"DisplayOptions";
constexpr DWORD animateIslandBit = 1;
constexpr DWORD hideInFullscreenBit = 2;
constexpr wchar_t islandValue[] = L"IslandOptions";
constexpr wchar_t featuresValue[] = L"FeatureOrder";
constexpr wchar_t accentValue[] = L"CustomAccent";

std::optional<DWORD> read(wchar_t const* name) {
    DWORD value = 0;
    DWORD bytes = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, settingsKey, name, RRF_RT_REG_DWORD, nullptr, &value, &bytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return value;
}

bool write(wchar_t const* name, DWORD value) {
    return RegSetKeyValueW(HKEY_CURRENT_USER, settingsKey, name, REG_DWORD, &value, sizeof(value)) == ERROR_SUCCESS;
}

}

Settings Settings::load() {
    Settings settings;
    if (const auto value = read(clipboardValue)) {
        settings.autoAddText = (*value & autoAddTextBit) != 0;
        settings.autoAddScreenshots = (*value & autoAddScreenshotsBit) != 0;
        settings.showClipboardPreviews = (*value & showClipboardPreviewsBit) != 0;
    }
    if (const auto value = read(timerValue)) settings.autoCycle = (*value & autoCycleBit) != 0;
    if (const auto value = read(displayValue)) {
        settings.animateIsland = (*value & animateIslandBit) != 0;
        settings.hideInFullscreen = (*value & hideInFullscreenBit) != 0;
    }
    if (const auto value = read(islandValue)) {
        const auto choice = [bits = *value](unsigned shift, unsigned fallback, unsigned count = 3) {
            const auto index = (bits >> shift) & 3u;
            return index < count ? index : fallback;
        };
        settings.fixedPillMinimum = (*value & 1u) != 0;
        settings.showWaveform = (*value & 2u) != 0;
        settings.showPillLyrics = (*value & 4u) != 0;
        settings.islandSize = choice(3, 1);
        settings.accentMode = choice(5, 0);
        settings.compactPause = choice(7, 1, 4);
        settings.focusPreset = choice(9, 1);
        settings.breakPreset = choice(11, 0);
    }
    if (const auto value = read(accentValue)) settings.customAccent = *value & 0xffffff;
    if (const auto value = read(featuresValue)) {
        settings.enabledFeatures = *value & 0x1f;
        auto order = featureOrder;
        unsigned seen = 0;
        for (unsigned index = 0; index < order.size(); ++index) {
            const auto feature = (*value >> (5 + 3 * index)) & 7;
            if (feature >= order.size() || (seen & (1u << feature))) break;
            order[index] = static_cast<Feature>(feature);
            seen |= 1u << feature;
        }
        if (seen == 0x1f) settings.features = order;
    }
    return settings;
}

bool Settings::saveClipboard() const {
    const DWORD clipboard = (autoAddText ? autoAddTextBit : 0) |
        (autoAddScreenshots ? autoAddScreenshotsBit : 0) | (showClipboardPreviews ? showClipboardPreviewsBit : 0);
    return write(clipboardValue, clipboard);
}

bool Settings::saveTimer() const {
    return write(timerValue, autoCycle ? autoCycleBit : 0);
}

bool Settings::saveDisplay() const {
    const DWORD display = (animateIsland ? animateIslandBit : 0) |
        (hideInFullscreen ? hideInFullscreenBit : 0);
    return write(displayValue, display);
}

bool Settings::saveIsland() const {
    const DWORD value = (fixedPillMinimum ? 1u : 0u) | (showWaveform ? 2u : 0u) | (showPillLyrics ? 4u : 0u) |
        (islandSize << 3) | (accentMode << 5) | (compactPause << 7) | (focusPreset << 9) | (breakPreset << 11);
    return write(islandValue, value);
}

bool Settings::saveFeatures() const {
    DWORD value = enabledFeatures;
    for (unsigned index = 0; index < features.size(); ++index) {
        value |= static_cast<DWORD>(features[index]) << (5 + 3 * index);
    }
    return write(featuresValue, value);
}

bool Settings::saveAccent() const {
    return write(accentValue, customAccent);
}

std::wstring startupShortcut() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_DONT_VERIFY, nullptr, &folder))) return {};
    struct FreePath {
        PWSTR value;
        ~FreePath() { CoTaskMemFree(value); }
    } path{folder};
    return std::wstring(path.value) + L"\\Zenith Dot.lnk";
}

bool startupEnabled(std::wstring const& shortcut) {
    if (shortcut.empty()) return false;
    const auto attributes = GetFileAttributesW(shortcut.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool setStartupEnabled(std::wstring const& shortcut, bool enabled) {
    if (shortcut.empty()) return false;
    if (!enabled) {
        return DeleteFileW(shortcut.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) return false;
    executable.resize(length);
    const auto folder = executable.substr(0, executable.find_last_of(L"\\/"));
    winrt::com_ptr<IShellLinkW> link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link.put()))) ||
        FAILED(link->SetPath(executable.c_str())) ||
        FAILED(link->SetWorkingDirectory(folder.c_str())) ||
        FAILED(link->SetIconLocation(executable.c_str(), 0)) ||
        FAILED(link->SetDescription(L"Zenith Dot"))) return false;
    const auto file = link.try_as<IPersistFile>();
    return file && SUCCEEDED(file->Save(shortcut.c_str(), TRUE));
}

}
