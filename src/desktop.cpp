#include "desktop.hpp"

#include <dwmapi.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <oleauto.h>
#include <shellapi.h>
#include <wbemidl.h>
#include <winrt/base.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace zenith {
namespace {

constexpr wchar_t consentStore[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore";

struct RegistryKey {
    HKEY value = nullptr;
    RegistryKey() = default;
    RegistryKey(RegistryKey const&) = delete;
    RegistryKey& operator=(RegistryKey const&) = delete;
    ~RegistryKey() { if (value) RegCloseKey(value); }
};

bool readTime(HKEY key, wchar_t const* name, ULONGLONG& result) {
    DWORD bytes = sizeof(result);
    return RegGetValueW(key, nullptr, name, RRF_RT_REG_QWORD, nullptr, &result, &bytes) == ERROR_SUCCESS;
}

std::wstring captureName(std::wstring_view key, bool unpackaged) {
    if (unpackaged) {
        const auto separator = key.find_last_of(L"#\\/");
        if (separator != std::wstring_view::npos) key.remove_prefix(separator + 1);
        if (key.size() > 4 && _wcsnicmp(key.data() + key.size() - 4, L".exe", 4) == 0) {
            key.remove_suffix(4);
        }
    } else {
        key = key.substr(0, key.find(L'_'));
        const auto separator = key.find_last_of(L'.');
        if (separator != std::wstring_view::npos && separator + 1 < key.size()) key.remove_prefix(separator + 1);
    }
    return std::wstring(key);
}

void findCapture(HKEY key, bool unpackaged, ULONGLONG& newest, std::wstring& app) {
    for (DWORD index = 0;; ++index) {
        wchar_t name[256]{};
        DWORD length = static_cast<DWORD>(std::size(name));
        const auto status = RegEnumKeyExW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_MORE_DATA) continue;
        if (status != ERROR_SUCCESS) break;
        RegistryKey child;
        if (RegOpenKeyExW(key, name, 0, KEY_READ, &child.value) != ERROR_SUCCESS) continue;
        if (!unpackaged && _wcsicmp(name, L"NonPackaged") == 0) {
            findCapture(child.value, true, newest, app);
            continue;
        }
        ULONGLONG start = 0, stop = 0;
        if (readTime(child.value, L"LastUsedTimeStart", start) &&
            readTime(child.value, L"LastUsedTimeStop", stop) && start > newest && stop == 0) {
            auto candidate = captureName({name, length}, unpackaged);
            if (!candidate.empty()) {
                newest = start;
                app = std::move(candidate);
            }
        }
    }
}

std::wstring captureApp(HKEY root, wchar_t const* device) {
    RegistryKey key;
    if (RegOpenKeyExW(root, device, 0, KEY_READ, &key.value) != ERROR_SUCCESS) return {};
    std::wstring app;
    ULONGLONG newest = 0;
    findCapture(key.value, false, newest, app);
    return app;
}

HWND foregroundListener = nullptr;   // Out-of-context events are delivered on the registering UI thread.

void CALLBACK foregroundEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    if (foregroundListener) PostMessageW(foregroundListener, DesktopMonitor::foregroundChanged, 0, 0);
}

struct DeviceNotifications : winrt::implements<DeviceNotifications, IMMNotificationClient> {
    explicit DeviceNotifications(HWND window) : window_(window) {}

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) noexcept override {
        if (flow == eRender && role == eMultimedia) PostMessageW(window_, DesktopMonitor::audioDeviceChanged, 0, 0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) noexcept override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) noexcept override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) noexcept override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, PROPERTYKEY const) noexcept override { return S_OK; }

private:
    HWND window_;
};

struct VolumeNotifications : winrt::implements<VolumeNotifications, IAudioEndpointVolumeCallback> {
    VolumeNotifications(HWND window, LPARAM generation) : window_(window), generation_(generation) {}

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA data) noexcept override {
        if (!data || !std::isfinite(data->fMasterVolume)) return S_OK;
        const auto percent = static_cast<unsigned>(std::lround(std::clamp(data->fMasterVolume, 0.0f, 1.0f) * 100));
        const auto volume = percent | (data->bMuted ? 0x100u : 0u);
        PostMessageW(window_, DesktopMonitor::volumeChanged, volume, generation_);
        return S_OK;
    }

private:
    HWND window_;
    LPARAM generation_;
};

using Bstr = std::unique_ptr<OLECHAR, decltype(&SysFreeString)>;

Bstr bstr(wchar_t const* value) {
    return {SysAllocString(value), SysFreeString};
}

winrt::com_ptr<IEnumWbemClassObject> subscribeBrightness() {
    winrt::com_ptr<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(locator.put())))) return {};
    const auto name = bstr(L"ROOT\\WMI");
    const auto language = bstr(L"WQL");
    const auto query = bstr(L"SELECT Brightness FROM WmiMonitorBrightnessEvent WHERE Active = TRUE");
    if (!name || !language || !query) return {};

    winrt::com_ptr<IWbemServices> service;
    if (FAILED(locator->ConnectServer(name.get(), nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                                      nullptr, nullptr, service.put()))) return {};
    if (FAILED(CoSetProxyBlanket(service.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE))) return {};
    winrt::com_ptr<IEnumWbemClassObject> events;
    if (FAILED(service->ExecNotificationQuery(language.get(), query.get(),
                 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, events.put()))) return {};
    return events;
}

std::optional<unsigned> nextBrightness(winrt::com_ptr<IEnumWbemClassObject>& events) {
    std::optional<unsigned> level;
    // Drain a burst so the UI sees the current level rather than replaying old key presses.
    for (unsigned i = 0; i < 32; ++i) {
        winrt::com_ptr<IWbemClassObject> event;
        ULONG returned = 0;
        const auto result = events->Next(WBEM_NO_WAIT, 1, event.put(), &returned);
        if (FAILED(result) || result == WBEM_S_FALSE) {
            events = nullptr;
            break;
        }
        if (returned == 0) break;
        VARIANT value{};
        if (SUCCEEDED(event->Get(L"Brightness", 0, &value, nullptr, nullptr))) {
            if (value.vt == VT_UI1 && value.bVal <= 100) level = value.bVal;
            if (value.vt == VT_I4 && value.lVal >= 0 && value.lVal <= 100) {
                level = static_cast<unsigned>(value.lVal);
            }
        }
        VariantClear(&value);
    }
    return level;
}

void watchRegistryAndBrightness(HWND window, HANDLE stop) {
    RegistryKey key;
    RegOpenKeyExW(HKEY_CURRENT_USER, consentStore, 0, KEY_NOTIFY, &key.value);
    const auto changed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    const auto arm = [&] {
        return key.value && changed && RegNotifyChangeKeyValue(key.value, TRUE,
            REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, changed, TRUE) == ERROR_SUCCESS;
    };
    bool watching = arm();
    if (watching) PostMessageW(window, DesktopMonitor::captureChanged, 0, 0);

    const bool com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    auto brightness = com ? subscribeBrightness() : nullptr;
    const HANDLE handles[]{stop, changed};
    std::optional<unsigned> lastLevel;
    for (;;) {
        const auto signaled = WaitForMultipleObjects(watching ? 2 : 1, handles, FALSE, brightness ? 250 : INFINITE);
        if (signaled == WAIT_OBJECT_0 || signaled == WAIT_FAILED) break;
        if (signaled == WAIT_OBJECT_0 + 1) {
            watching = arm();
            PostMessageW(window, DesktopMonitor::captureChanged, 0, 0);
        }
        if (brightness) {
            if (const auto level = nextBrightness(brightness); level && level != lastLevel) {
                lastLevel = level;
                PostMessageW(window, DesktopMonitor::brightnessChanged, *level, 0);
            }
        }
    }
    brightness = nullptr;
    if (com) CoUninitialize();
    if (key.value) RegCloseKey(std::exchange(key.value, nullptr));
    if (changed) CloseHandle(changed);
}

}

bool fullscreenOnPrimary(HWND overlay) {
    const auto foreground = GetForegroundWindow();
    if (!foreground || foreground == overlay || foreground == GetShellWindow() || IsIconic(foreground)) return false;
    const auto primary = MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY);
    if (MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL) != primary) return false;
    wchar_t windowClass[64]{};
    GetClassNameW(foreground, windowClass, static_cast<int>(std::size(windowClass)));
    const std::wstring_view name(windowClass);
    if (name == L"Progman" || name == L"WorkerW" || name == L"Shell_TrayWnd" || name == L"Shell_SecondaryTrayWnd") {
        return false;
    }

    QUERY_USER_NOTIFICATION_STATE notification{};
    if (SUCCEEDED(SHQueryUserNotificationState(&notification)) &&
        (notification == QUNS_BUSY || notification == QUNS_RUNNING_D3D_FULL_SCREEN ||
         notification == QUNS_PRESENTATION_MODE)) return true;

    MONITORINFO monitor{sizeof(MONITORINFO)};
    RECT bounds{};
    if (!GetMonitorInfoW(primary, &monitor)) return false;
    if (FAILED(DwmGetWindowAttribute(foreground, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))) &&
        !GetWindowRect(foreground, &bounds)) return false;
    if (IsZoomed(foreground) && (GetWindowLongPtrW(foreground, GWL_STYLE) & WS_CAPTION) == WS_CAPTION) return false;
    const auto& screen = monitor.rcMonitor;
    return bounds.left <= screen.left + 1 && bounds.top <= screen.top + 1 &&
           bounds.right >= screen.right - 1 && bounds.bottom >= screen.bottom - 1;
}

CaptureActivity readCaptureActivity() {
    RegistryKey root;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, consentStore, 0, KEY_READ, &root.value) != ERROR_SUCCESS) return {};
    return {captureApp(root.value, L"microphone"), captureApp(root.value, L"webcam")};
}

struct DesktopMonitor::State {
    HWND window = nullptr;
    HWINEVENTHOOK foreground = nullptr;
    HANDLE stop = nullptr;
    std::thread worker;
    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    winrt::com_ptr<IMMNotificationClient> devices;
    winrt::com_ptr<IAudioEndpointVolume> endpoint;
    winrt::com_ptr<IAudioEndpointVolumeCallback> volume;
    LPARAM generation = 0;
};

DesktopMonitor::DesktopMonitor() : state_(std::make_unique<State>()) {}
DesktopMonitor::~DesktopMonitor() { stop(); }

void DesktopMonitor::start(HWND window) {
    state_->window = window;
    foregroundListener = window;
    state_->foreground = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                         foregroundEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(state_->enumerator.put())))) {
        state_->devices = winrt::make<DeviceNotifications>(window);
        if (FAILED(state_->enumerator->RegisterEndpointNotificationCallback(state_->devices.get()))) {
            state_->devices = nullptr;
        }
        refreshAudioDevice();
    }
    state_->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (state_->stop) {
        state_->worker = std::thread(watchRegistryAndBrightness, window, state_->stop);
    }
}

void DesktopMonitor::refreshAudioDevice() {
    ++state_->generation;
    if (state_->endpoint && state_->volume) state_->endpoint->UnregisterControlChangeNotify(state_->volume.get());
    state_->volume = nullptr;
    state_->endpoint = nullptr;
    if (!state_->enumerator) return;
    winrt::com_ptr<IMMDevice> device;
    if (FAILED(state_->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.put()))) return;
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr,
                                state_->endpoint.put_void()))) return;
    state_->volume = winrt::make<VolumeNotifications>(state_->window, state_->generation);
    if (FAILED(state_->endpoint->RegisterControlChangeNotify(state_->volume.get()))) state_->volume = nullptr;
}

bool DesktopMonitor::isCurrentVolume(LPARAM generation) const {
    return state_->volume && generation == state_->generation;
}

void DesktopMonitor::stop() {
    if (state_->foreground) UnhookWinEvent(state_->foreground);
    state_->foreground = nullptr;
    if (foregroundListener == state_->window) foregroundListener = nullptr;
    if (state_->endpoint && state_->volume) state_->endpoint->UnregisterControlChangeNotify(state_->volume.get());
    if (state_->enumerator && state_->devices) state_->enumerator->UnregisterEndpointNotificationCallback(state_->devices.get());
    state_->volume = nullptr;
    state_->endpoint = nullptr;
    state_->devices = nullptr;
    state_->enumerator = nullptr;
    if (state_->stop) {
        SetEvent(state_->stop);
        if (state_->worker.joinable()) state_->worker.join();
        CloseHandle(state_->stop);
        state_->stop = nullptr;
    }
    state_->window = nullptr;
}

}
