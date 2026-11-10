#include "desktop.hpp"

#include <dwmapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mmdeviceapi.h>
#include <oleauto.h>
#include <shellapi.h>
#include <wbemidl.h>
#include <mmreg.h>
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iterator>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace zenith {
namespace {

constexpr wchar_t consentStore[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore";

// The Windows audio policy COM class sets defaults. MMDevice exposes only queries.
struct __declspec(uuid("870af99c-171d-4f9e-af0d-e63df40c2bc9")) PolicyConfigClient;
struct __declspec(uuid("f8679f50-850a-41cf-9c72-430f290290c8")) IPolicyConfig : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, INT, PROPERTYKEY const&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, INT, PROPERTYKEY const&, PROPVARIANT const*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
};

std::wstring deviceId(IMMDevice& device) {
    LPWSTR value = nullptr;
    if (FAILED(device.GetId(&value))) return {};
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> id(value, CoTaskMemFree);
    return id ? id.get() : L"";
}

std::wstring deviceName(IMMDevice& device) {
    winrt::com_ptr<IPropertyStore> properties;
    if (FAILED(device.OpenPropertyStore(STGM_READ, properties.put()))) return {};
    PROPVARIANT name{};
    struct Clear { PROPVARIANT& value; ~Clear() { PropVariantClear(&value); } } clear{name};
    if (FAILED(properties->GetValue(PKEY_Device_FriendlyName, &name)) || name.vt != VT_LPWSTR || !name.pwszVal) return {};
    return name.pwszVal;
}

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

std::wstring processLabel(DWORD pid, wchar_t const* fallback) {
    if (!pid || pid == MAXDWORD) return fallback;
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return fallback;
    wchar_t path[MAX_PATH]{};
    DWORD length = static_cast<DWORD>(std::size(path));
    const BOOL found = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    if (!found) return fallback;
    return captureName({path, length}, true);
}

DWORD microphoneProcess(IMMDeviceEnumerator& devices) {
    winrt::com_ptr<IMMDeviceCollection> inputs;
    UINT count = 0;
    if (FAILED(devices.EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, inputs.put())) ||
        FAILED(inputs->GetCount(&count))) return 0;
    for (UINT index = 0; index < count; ++index) {
        winrt::com_ptr<IMMDevice> input;
        winrt::com_ptr<IAudioSessionManager2> manager;
        winrt::com_ptr<IAudioSessionEnumerator> sessions;
        int sessionCount = 0;
        if (FAILED(inputs->Item(index, input.put())) ||
            FAILED(input->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, nullptr, manager.put_void())) ||
            FAILED(manager->GetSessionEnumerator(sessions.put())) || FAILED(sessions->GetCount(&sessionCount))) continue;
        for (int i = 0; i < sessionCount; ++i) {
            winrt::com_ptr<IAudioSessionControl> session;
            AudioSessionState state{};
            if (FAILED(sessions->GetSession(i, session.put())) ||
                FAILED(session->GetState(&state)) || state != AudioSessionStateActive) continue;
            const auto details = session.try_as<IAudioSessionControl2>();
            DWORD pid = 0;
            if (details && SUCCEEDED(details->GetProcessId(&pid)) && pid && pid != GetCurrentProcessId()) return pid;
        }
    }
    return 0;
}

struct CameraActivity : winrt::implements<CameraActivity, IMFSensorActivitiesReportCallback> {
    explicit CameraActivity(HWND window) : window_(window) {}
    DWORD process() const { return pid_.load(); }

    HRESULT STDMETHODCALLTYPE OnActivitiesReport(IMFSensorActivitiesReport* reports) noexcept override {
        ULONG count = 0;
        if (FAILED(reports->GetCount(&count))) return S_OK;
        DWORD active = 0;
        for (ULONG index = 0; index < count && active == 0; ++index) {
            winrt::com_ptr<IMFSensorActivityReport> report;
            ULONG processes = 0;
            if (FAILED(reports->GetActivityReport(index, report.put())) ||
                FAILED(report->GetProcessCount(&processes))) continue;
            for (ULONG i = 0; i < processes; ++i) {
                winrt::com_ptr<IMFSensorProcessActivity> process;
                BOOL streaming = FALSE;
                if (FAILED(report->GetProcessActivity(i, process.put())) ||
                    FAILED(process->GetStreamingState(&streaming)) || !streaming) continue;
                ULONG pid = 0;
                process->GetProcessId(&pid);
                active = pid ? pid : MAXDWORD;
                break;
            }
        }
        if (pid_.exchange(active) != active) PostMessageW(window_, DesktopMonitor::captureChanged, 0, 0);
        return S_OK;
    }

private:
    HWND window_;
    std::atomic<DWORD> pid_ = 0;
};

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

winrt::com_ptr<IWbemServices> brightnessService() {
    winrt::com_ptr<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(locator.put())))) return {};
    const auto name = bstr(L"ROOT\\WMI");
    if (!name) return {};

    winrt::com_ptr<IWbemServices> service;
    if (FAILED(locator->ConnectServer(name.get(), nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                                      nullptr, nullptr, service.put()))) return {};
    if (FAILED(CoSetProxyBlanket(service.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE))) return {};
    return service;
}

winrt::com_ptr<IEnumWbemClassObject> subscribeBrightness(IWbemServices& service) {
    const auto language = bstr(L"WQL");
    const auto query = bstr(L"SELECT Brightness FROM WmiMonitorBrightnessEvent WHERE Active = TRUE");
    if (!language || !query) return {};
    winrt::com_ptr<IEnumWbemClassObject> events;
    if (FAILED(service.ExecNotificationQuery(language.get(), query.get(),
                 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, events.put()))) return {};
    return events;
}

std::optional<unsigned> brightnessValue(IWbemClassObject& object, wchar_t const* property) {
    std::optional<unsigned> level;
    VARIANT value{};
    if (SUCCEEDED(object.Get(property, 0, &value, nullptr, nullptr))) {
        if (value.vt == VT_UI1 && value.bVal <= 100) level = value.bVal;
        if (value.vt == VT_I4 && value.lVal >= 0 && value.lVal <= 100) level = static_cast<unsigned>(value.lVal);
    }
    VariantClear(&value);
    return level;
}

std::optional<unsigned> readBrightness(IWbemServices& service) {
    const auto language = bstr(L"WQL");
    const auto query = bstr(L"SELECT CurrentBrightness FROM WmiMonitorBrightness WHERE Active = TRUE");
    if (!language || !query) return std::nullopt;
    winrt::com_ptr<IEnumWbemClassObject> rows;
    if (FAILED(service.ExecQuery(language.get(), query.get(), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                 nullptr, rows.put()))) return std::nullopt;
    winrt::com_ptr<IWbemClassObject> row;
    ULONG count = 0;
    if (FAILED(rows->Next(250, 1, row.put(), &count)) || !count) return std::nullopt;
    return brightnessValue(*row, L"CurrentBrightness");
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
        if (const auto value = brightnessValue(*event, L"Brightness")) level = value;
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
    auto service = com ? brightnessService() : nullptr;
    auto brightness = service ? subscribeBrightness(*service) : nullptr;
    const HANDLE handles[]{stop, changed};
    auto lastLevel = service ? readBrightness(*service) : std::nullopt;
    bool polling = lastLevel.has_value();
    for (;;) {
        const DWORD wait = brightness ? 250 : polling ? 1000 : INFINITE;
        const auto signaled = WaitForMultipleObjects(watching ? 2 : 1, handles, FALSE, wait);
        if (signaled == WAIT_OBJECT_0 || signaled == WAIT_FAILED) break;
        if (signaled == WAIT_OBJECT_0 + 1) {
            watching = arm();
            PostMessageW(window, DesktopMonitor::captureChanged, 0, 0);
        }
        std::optional<unsigned> level;
        if (brightness) level = nextBrightness(brightness);
        else if (polling && signaled == WAIT_TIMEOUT) level = readBrightness(*service);
        if (level && level != lastLevel) {
            lastLevel = level;
            PostMessageW(window, DesktopMonitor::brightnessChanged, *level, 0);
        }
    }
    brightness = nullptr;
    service = nullptr;
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
        (notification == QUNS_RUNNING_D3D_FULL_SCREEN || notification == QUNS_PRESENTATION_MODE)) return true;

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
    bool mediaFoundation = false;
    winrt::com_ptr<CameraActivity> cameraActivity;
    winrt::com_ptr<IMFSensorActivityMonitor> cameraMonitor;
    std::wstring audioOutput;
    winrt::Windows::UI::Notifications::ToastNotificationManagerForUser notifications{nullptr};
    winrt::Windows::UI::Notifications::ToastNotificationManagerForUser::NotificationModeChanged_revoker notificationMode;
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
    state_->mediaFoundation = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    if (state_->mediaFoundation) {
        state_->cameraActivity = winrt::make_self<CameraActivity>(window);
        if (SUCCEEDED(MFCreateSensorActivityMonitor(state_->cameraActivity.get(), state_->cameraMonitor.put()))) {
            state_->cameraMonitor->Start();
        }
    }
    try {
        state_->notifications = winrt::Windows::UI::Notifications::ToastNotificationManager::GetDefault();
        if (state_->notifications) {
            state_->notificationMode = state_->notifications.NotificationModeChanged(winrt::auto_revoke,
                [window](auto const&, auto const&) { PostMessageW(window, notificationModeChanged, 0, 0); });
        }
    } catch (winrt::hresult_error const&) {
        // Older Windows builds may expose the manager without notification-mode events.
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
    state_->audioOutput.clear();
    if (!state_->enumerator) return;
    winrt::com_ptr<IMMDevice> device;
    if (FAILED(state_->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.put()))) return;
    state_->audioOutput = deviceName(*device);
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr,
                                state_->endpoint.put_void()))) return;
    state_->volume = winrt::make<VolumeNotifications>(state_->window, state_->generation);
    if (FAILED(state_->endpoint->RegisterControlChangeNotify(state_->volume.get()))) state_->volume = nullptr;
}

std::wstring const& DesktopMonitor::audioOutput() const { return state_->audioOutput; }

CaptureActivity DesktopMonitor::captureActivity() const {
    auto activity = readCaptureActivity();
    if (activity.microphone.empty() && state_->enumerator) {
        if (const auto pid = microphoneProcess(*state_->enumerator)) {
            activity.microphone = processLabel(pid, L"Audio input");
        }
    }
    if (activity.camera.empty() && state_->cameraActivity) {
        if (const auto pid = state_->cameraActivity->process()) {
            activity.camera = processLabel(pid, L"Camera in use");
        }
    }
    return activity;
}

std::vector<AudioOutput> DesktopMonitor::audioOutputs() const {
    if (!state_->enumerator) return {};
    winrt::com_ptr<IMMDeviceCollection> devices;
    if (FAILED(state_->enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, devices.put()))) return {};
    winrt::com_ptr<IMMDevice> current;
    state_->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, current.put());
    const auto active = current ? deviceId(*current) : std::wstring{};
    UINT count = 0;
    if (FAILED(devices->GetCount(&count))) return {};
    std::vector<AudioOutput> outputs;
    for (UINT i = 0; i < count; ++i) {
        winrt::com_ptr<IMMDevice> device;
        if (FAILED(devices->Item(i, device.put()))) continue;
        auto id = deviceId(*device);
        if (id.empty()) continue;
        auto name = deviceName(*device);
        const bool selected = id == active;
        outputs.push_back({std::move(id), name.empty() ? L"Audio device " + std::to_wstring(i + 1) : std::move(name), selected});
    }
    return outputs;
}

bool DesktopMonitor::selectAudioOutput(std::wstring const& id) {
    if (!state_->enumerator) return false;
    winrt::com_ptr<IMMDevice> device;
    DWORD status = 0;
    if (FAILED(state_->enumerator->GetDevice(id.c_str(), device.put())) ||
        FAILED(device->GetState(&status)) || status != DEVICE_STATE_ACTIVE) return false;
    winrt::com_ptr<IPolicyConfig> policy;
    if (FAILED(CoCreateInstance(__uuidof(PolicyConfigClient), nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(policy.put())))) return false;
    winrt::com_ptr<IMMDevice> previous;
    state_->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, previous.put());
    const auto previousId = previous ? deviceId(*previous) : std::wstring{};
    if (FAILED(policy->SetDefaultEndpoint(id.c_str(), eMultimedia))) return false;
    if (FAILED(policy->SetDefaultEndpoint(id.c_str(), eConsole))) {
        if (!previousId.empty()) policy->SetDefaultEndpoint(previousId.c_str(), eMultimedia);
        return false;
    }
    return true;
}

std::optional<bool> DesktopMonitor::notificationsQuiet() const {
    using namespace winrt::Windows::UI::Notifications;
    try {
        if (state_->notifications) {
            switch (state_->notifications.NotificationMode()) {
            case ToastNotificationMode::Unrestricted: return false;
            case ToastNotificationMode::PriorityOnly:
            case ToastNotificationMode::AlarmsOnly: return true;
            }
        }
    } catch (winrt::hresult_error const&) {
        // The notification-mode API is optional on Windows 11.
    }
    return std::nullopt;
}

bool DesktopMonitor::isCurrentVolume(LPARAM generation) const {
    return state_->volume && generation == state_->generation;
}

void DesktopMonitor::stop() {
    if (state_->cameraMonitor) {
        state_->cameraMonitor->Stop();
        if (const auto shutdown = state_->cameraMonitor.try_as<IMFShutdown>()) shutdown->Shutdown();
    }
    state_->cameraMonitor = nullptr;
    state_->cameraActivity = nullptr;
    if (std::exchange(state_->mediaFoundation, false)) MFShutdown();
    state_->notificationMode.revoke();
    state_->notifications = nullptr;
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
