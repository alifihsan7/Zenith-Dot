#include "browser_tabs.hpp"

#include <shellapi.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string_view>
#include <thread>

namespace zenith {
namespace {

using namespace winrt::Windows::Data::Json;

constexpr wchar_t hostClass[] = L"ZenithDot.BrowserHost";
constexpr wchar_t extensionOrigin[] = L"chrome-extension://achehkjcdkfodmalnalebhpafbonlccf/";
constexpr ULONG_PTR snapshotMessage = 0x5a445431;
constexpr ULONG_PTR commandMessage = 0x5a445432;
constexpr DWORD messageLimit = 128 * 1024;

bool transfer(HANDLE pipe, void* bytes, DWORD size, bool write) {
    auto* position = static_cast<char*>(bytes);
    while (size != 0) {
        DWORD count = 0;
        const bool success = write ? WriteFile(pipe, position, size, &count, nullptr) != FALSE :
                                     ReadFile(pipe, position, size, &count, nullptr) != FALSE;
        if (!success || count == 0) return false;
        position += count;
        size -= count;
    }
    return true;
}

bool isPeer(HWND window, wchar_t const* expectedClass) {
    wchar_t type[64]{};
    if (!GetClassNameW(window, type, static_cast<int>(std::size(type))) ||
        std::wstring_view(type) != expectedClass) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    wchar_t sender[MAX_PATH]{}, self[MAX_PATH]{};
    DWORD size = MAX_PATH;
    const bool found = QueryFullProcessImageNameW(process, 0, sender, &size) != FALSE;
    CloseHandle(process);
    const auto selfSize = GetModuleFileNameW(nullptr, self, MAX_PATH);
    return found && selfSize != 0 && selfSize < MAX_PATH && _wcsicmp(self, sender) == 0;
}

void copyData(HWND target, HWND sender, ULONG_PTR kind, std::string_view json, UINT timeout) {
    COPYDATASTRUCT packet{kind, static_cast<DWORD>(json.size()), const_cast<char*>(json.data())};
    DWORD_PTR delivered = 0;
    SendMessageTimeoutW(target, WM_COPYDATA, reinterpret_cast<WPARAM>(sender), reinterpret_cast<LPARAM>(&packet),
                        SMTO_ABORTIFHUNG | SMTO_BLOCK, timeout, &delivered);
}

LRESULT CALLBACK hostProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_COPYDATA) {
        const auto packet = reinterpret_cast<COPYDATASTRUCT const*>(lparam);
        if (!packet || packet->dwData != commandMessage || packet->cbData > 1024 || !packet->lpData ||
            !isPeer(reinterpret_cast<HWND>(wparam), overlayWindowClass)) return FALSE;
        auto length = packet->cbData;
        const auto out = GetStdHandle(STD_OUTPUT_HANDLE);
        return transfer(out, &length, sizeof(length), true) && transfer(out, packet->lpData, length, true);
    }
    if (message == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(window, message, wparam, lparam);
}

std::wstring browserName(std::wstring_view app) {
    const auto separator = app.find_last_of(L"\\/");
    if (separator != std::wstring_view::npos) app.remove_prefix(separator + 1);
    if (CompareStringOrdinal(app.data(), static_cast<int>(app.size()), L"chrome.exe", -1, TRUE) == CSTR_EQUAL ||
        CompareStringOrdinal(app.data(), static_cast<int>(app.size()), L"Chrome", -1, TRUE) == CSTR_EQUAL) {
        return L"chrome.exe";
    }
    if (CompareStringOrdinal(app.data(), static_cast<int>(app.size()), L"msedge.exe", -1, TRUE) == CSTR_EQUAL ||
        CompareStringOrdinal(app.data(), static_cast<int>(app.size()), L"MSEdge", -1, TRUE) == CSTR_EQUAL) {
        return L"msedge.exe";
    }
    return {};
}

std::wstring text(JsonObject const& object, winrt::hstring const& name) {
    const auto value = object.GetNamedString(name, L"");
    return std::wstring(std::wstring_view(value).substr(0, 240));
}

double number(JsonObject const& object, winrt::hstring const& name) {
    const auto value = object.GetNamedNumber(name, 0);
    return std::isfinite(value) ? value : 0;
}

}

bool browserHostRequested() {
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    const bool match = args && count >= 2 && std::wstring_view(args[1]) == extensionOrigin;
    if (args) LocalFree(args);
    return match;
}

int runBrowserHost(HINSTANCE instance) {
    WNDCLASSW type{};
    type.lpfnWndProc = hostProc;
    type.hInstance = instance;
    type.lpszClassName = hostClass;
    if (!RegisterClassW(&type)) return 1;
    const auto window = CreateWindowExW(0, hostClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (!window) return 1;
    const auto input = GetStdHandle(STD_INPUT_HANDLE);
    std::thread reader([=] {
        DWORD size = 0;
        while (transfer(input, &size, sizeof(size), false) && size != 0 && size <= messageLimit) {
            std::string bytes(size, '\0');
            if (!transfer(input, bytes.data(), size, false)) break;
            if (const auto target = FindWindowW(overlayWindowClass, nullptr)) {
                copyData(target, window, snapshotMessage, bytes, 200);
            }
        }
        PostMessageW(window, WM_CLOSE, 0, 0);
    });

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) DispatchMessageW(&msg);
    CancelSynchronousIo(reader.native_handle());
    reader.join();
    return 0;
}

bool BrowserTabs::receive(HWND sender, COPYDATASTRUCT const& message) {
    if (message.dwData != snapshotMessage || message.cbData == 0 || message.cbData > messageLimit ||
        !message.lpData || !isPeer(sender, hostClass)) return false;
    std::vector<Tab> incoming;
    try {
        const auto bytes = std::string_view(static_cast<char const*>(message.lpData), message.cbData);
        JsonObject snapshot{nullptr};
        if (!JsonObject::TryParse(winrt::to_hstring(bytes), snapshot)) return false;
        const auto browser = snapshot.GetNamedString(L"browser", L"");
        if (browser != L"chrome.exe" && browser != L"msedge.exe") return false;
        const auto entries = snapshot.GetNamedArray(L"tabs");
        const auto now = std::chrono::steady_clock::now();
        for (const auto& entry : entries) {
            if (incoming.size() == 16) break;
            if (entry.ValueType() != JsonValueType::Object) continue;
            const auto value = entry.GetObject();
            const double tabId = value.GetNamedNumber(L"id", -1);
            if (tabId < 0 || tabId > INT_MAX || std::floor(tabId) != tabId) continue;
            const auto tab = static_cast<unsigned>(tabId);
            if (std::ranges::find(incoming, tab, &Tab::number) != incoming.end()) continue;

            MediaState media;
            media.hasSession = true;
            media.appId = browser.c_str();
            media.sourceId = L"tab:" + std::to_wstring(reinterpret_cast<UINT_PTR>(sender)) + L":" + std::to_wstring(tab);
            media.title = text(value, L"title");
            media.artist = text(value, L"artist");
            media.album = text(value, L"album");
            media.playing = value.GetNamedBoolean(L"playing", false);
            media.duration = std::clamp(number(value, L"duration"), 0.0, 86400.0);
            media.position = std::clamp(number(value, L"position"), 0.0, media.duration);
            const auto rate = number(value, L"rate");
            media.rate = rate > 0 ? std::clamp(rate, 0.1, 16.0) : 1;
            media.canSeek = media.duration > 0 && value.GetNamedBoolean(L"canSeek", false);
            media.canSkip = false;
            media.sampledAt = now;
            incoming.push_back({sender, tab, std::move(media)});
        }
    } catch (winrt::hresult_error const&) {
        return false;
    }
    std::erase_if(tabs_, [&](Tab const& tab) { return tab.host == sender; });
    tabs_.insert(tabs_.end(), std::make_move_iterator(incoming.begin()), std::make_move_iterator(incoming.end()));
    std::ranges::sort(tabs_, {}, [](Tab const& tab) { return std::pair{reinterpret_cast<UINT_PTR>(tab.host), tab.number}; });
    if (!find(selected_)) selected_.clear();
    return true;
}

bool BrowserTabs::prune() {
    const auto removed = std::erase_if(tabs_, [](Tab const& tab) { return !IsWindow(tab.host); });
    if (removed && !find(selected_)) selected_.clear();
    return removed != 0;
}

BrowserTabs::Tab const* BrowserTabs::find(std::wstring const& id) const {
    const auto tab = std::ranges::find(tabs_, id, [](Tab const& entry) -> std::wstring const& {
        return entry.media.sourceId;
    });
    return tab == tabs_.end() ? nullptr : &*tab;
}

bool BrowserTabs::select(std::wstring const& id) {
    const auto tab = find(id);
    selected_ = tab ? id : std::wstring{};
    return tab != nullptr;
}

MediaState BrowserTabs::combine(MediaState const& native) const {
    if (tabs_.empty()) return native;
    const auto browser = browserName(native.appId);
    const auto* tab = find(selected_);
    if (!tab) {
        for (const auto& entry : tabs_) {
            if (native.hasSession && browser != entry.media.appId) continue;
            if (!native.title.empty() && native.title == entry.media.title) {
                tab = &entry;
                break;
            }
            if (!tab || (entry.media.playing && !tab->media.playing)) tab = &entry;
        }
    }

    MediaState shown = tab ? tab->media : native;
    if (tab && browser == shown.appId && !native.title.empty() && native.title == shown.title) {
        shown.cover = native.cover;
        shown.theme = native.theme;
    }
    shown.sources = native.sources;
    std::erase_if(shown.sources, [&](MediaSource const& source) {
        const auto app = browserName(source.appId);
        return std::ranges::any_of(tabs_, [&](Tab const& entry) { return app == entry.media.appId; });
    });
    unsigned chrome = 0, edge = 0;
    for (const auto& entry : tabs_) {
        const auto& media = entry.media;
        auto& count = media.appId == L"chrome.exe" ? chrome : edge;
        shown.sources.push_back({media.appId, media.playing, media.sourceId, ++count});
    }
    return shown;
}

void BrowserTabs::send(std::wstring const& source, std::wstring_view action, double seconds) const {
    const auto tab = find(source);
    if (!tab) return;
    JsonObject command;
    command.SetNamedValue(L"id", JsonValue::CreateNumberValue(tab->number));
    command.SetNamedValue(L"action", JsonValue::CreateStringValue(winrt::hstring(action)));
    command.SetNamedValue(L"seconds", JsonValue::CreateNumberValue(seconds));
    copyData(tab->host, window_, commandMessage, winrt::to_string(command.Stringify()), 70);
}

void BrowserTabs::control(std::wstring const& source, MediaCommand command) const {
    if (command == MediaCommand::toggle) send(source, L"toggle", 0);
}

void BrowserTabs::seek(std::wstring const& source, double seconds) const {
    send(source, L"seek", seconds);
}

}
