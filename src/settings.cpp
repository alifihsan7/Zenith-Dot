#include "settings.hpp"
#include "browser_tabs.hpp"
#include "resource.h"

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
constexpr wchar_t settingsTitle[] = L"Zenith Dot Settings";

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

void showClipboardOptions(HWND dialog, Settings const& settings) {
    CheckDlgButton(dialog, IDC_ADD_COPIED_TEXT, settings.autoAddText ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_ADD_SCREENSHOTS, settings.autoAddScreenshots ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_CLIPBOARD_PREVIEWS, settings.showClipboardPreviews ? BST_CHECKED : BST_UNCHECKED);
}

INT_PTR CALLBACK settingsDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrW(dialog, DWLP_USER, lparam);
        const auto icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZENITH_DOT));
        if (icon) SendMessageW(dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        showClipboardOptions(dialog, *reinterpret_cast<Settings const*>(lparam));
        const auto shortcut = startupShortcut();
        CheckDlgButton(dialog, IDC_START_WITH_WINDOWS, startupEnabled(shortcut) ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(dialog, IDC_START_WITH_WINDOWS), !shortcut.empty());
        return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL))) {
        EndDialog(dialog, 0);
        return TRUE;
    }
    if (message != WM_COMMAND || HIWORD(wparam) != BN_CLICKED) return FALSE;

    const auto checked = [dialog](int control) { return IsDlgButtonChecked(dialog, control) == BST_CHECKED; };
    const auto control = LOWORD(wparam);
    if (control == IDC_START_WITH_WINDOWS) {
        const auto shortcut = startupShortcut();
        if (!setStartupEnabled(shortcut, checked(control))) {
            CheckDlgButton(dialog, control, startupEnabled(shortcut) ? BST_CHECKED : BST_UNCHECKED);
            MessageBoxW(dialog, L"Couldn’t update the Windows startup shortcut.", settingsTitle, MB_OK | MB_ICONWARNING);
        }
        return TRUE;
    }
    if (control != IDC_ADD_COPIED_TEXT && control != IDC_ADD_SCREENSHOTS && control != IDC_CLIPBOARD_PREVIEWS) {
        return FALSE;
    }

    auto& settings = *reinterpret_cast<Settings*>(GetWindowLongPtrW(dialog, DWLP_USER));
    auto updated = settings;
    updated.autoAddText = checked(IDC_ADD_COPIED_TEXT);
    updated.autoAddScreenshots = checked(IDC_ADD_SCREENSHOTS);
    updated.showClipboardPreviews = checked(IDC_CLIPBOARD_PREVIEWS);
    if (updated.saveClipboard()) {
        settings = updated;
        if (const auto overlay = FindWindowW(overlayWindowClass, nullptr)) {
            PostMessageW(overlay, settingsChangedMessage, 0, 0);
        }
    } else {
        showClipboardOptions(dialog, settings);
        MessageBoxW(dialog, L"Couldn’t save Drop Zone settings.", settingsTitle, MB_OK | MB_ICONWARNING);
    }
    return TRUE;
}

}

int runSettingsWindow(HINSTANCE instance) {
    winrt::handle singleInstance{CreateMutexW(nullptr, FALSE, L"Local\\ZenithDot.Settings")};
    const auto error = GetLastError();
    if (!singleInstance) return 1;
    if (error == ERROR_ALREADY_EXISTS) {
        if (const auto existing = FindWindowW(L"#32770", settingsTitle)) {
            SetForegroundWindow(existing);
        }
        return 0;
    }
    auto settings = Settings::load();
    const auto result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, settingsDialog,
                                       reinterpret_cast<LPARAM>(&settings));
    if (result == -1) winrt::throw_last_error();
    return static_cast<int>(result);
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

bool Settings::saveClipboard() const {
    const DWORD clipboard = (autoAddText ? autoAddTextBit : 0) |
        (autoAddScreenshots ? autoAddScreenshotsBit : 0) | (showClipboardPreviews ? showClipboardPreviewsBit : 0);
    return write(clipboardValue, clipboard);
}

bool Settings::saveTimer() const {
    return write(timerValue, autoCycle ? autoCycleBit : 0);
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
