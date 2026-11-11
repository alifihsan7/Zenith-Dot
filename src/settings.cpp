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
    return settings;
}

bool Settings::save() const {
    const DWORD clipboard = (autoAddText ? autoAddTextBit : 0) |
        (autoAddScreenshots ? autoAddScreenshotsBit : 0) | (showClipboardPreviews ? showClipboardPreviewsBit : 0);
    return write(clipboardValue, clipboard) && write(timerValue, autoCycle ? autoCycleBit : 0);
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
