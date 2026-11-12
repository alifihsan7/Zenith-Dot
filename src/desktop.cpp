#include "desktop.hpp"

#include <appmodel.h>
#include <dwmapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mfapi.h>
#include <mfidl.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <oleauto.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <mmreg.h>
#include <winver.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propkey.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
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

std::wstring processPath(DWORD pid, std::wstring* appId = nullptr) {
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    wchar_t path[MAX_PATH * 2]{};
    DWORD length = static_cast<DWORD>(std::size(path));
    const BOOL found = QueryFullProcessImageNameW(process, 0, path, &length);
    if (appId) {
        wchar_t identity[APPLICATION_USER_MODEL_ID_MAX_LENGTH + 1]{};
        UINT32 size = static_cast<UINT32>(std::size(identity));
        if (GetApplicationUserModelId(process, &size, identity) == ERROR_SUCCESS) *appId = identity;
    }
    CloseHandle(process);
    return found ? std::wstring(path, length) : std::wstring{};
}

struct ScreenCaptureWindows {
    std::wstring sharing;
    ULONGLONG obsStarted = 0;
};

ScreenCaptureWindows screenCaptureWindows() {
    ScreenCaptureWindows result;
    EnumWindows([](HWND window, LPARAM context) -> BOOL {
        auto& found = *reinterpret_cast<ScreenCaptureWindows*>(context);
        wchar_t className[80]{};
        if (!GetClassNameW(window, className, static_cast<int>(std::size(className)))) return TRUE;
        const std::wstring_view type(className);
        const bool browser = found.sharing.empty() && type.starts_with(L"Chrome_WidgetWin_") &&
            (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        const bool obs = !found.obsStarted && type.starts_with(L"Qt");
        if (!browser && !obs) return TRUE;

        if (browser) {
            RECT bounds{};
            const auto dpi = GetDpiForWindow(window);
            if (!GetWindowRect(window, &bounds) || bounds.bottom - bounds.top > MulDiv(160, dpi ? dpi : 96, 96))
                return TRUE;
            wchar_t title[512]{};
            GetWindowTextW(window, title, static_cast<int>(std::size(title)));
            const std::wstring_view text(title);
            if (text.find(L" is sharing your screen") == text.npos &&
                text.find(L" is sharing a window") == text.npos) return TRUE;
        }
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        const auto app = processLabel(pid, L"");
        if (browser) {
            if (_wcsicmp(app.c_str(), L"chrome") == 0) found.sharing = L"Google Chrome";
            else if (_wcsicmp(app.c_str(), L"msedge") == 0) found.sharing = L"Microsoft Edge";
            else if (_wcsicmp(app.c_str(), L"brave") == 0) found.sharing = L"Brave";
        } else if (_wcsicmp(app.c_str(), L"obs64") == 0 || _wcsicmp(app.c_str(), L"obs32") == 0) {
            const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!process) return TRUE;
            FILETIME started{}, ended{}, kernel{}, user{};
            if (GetProcessTimes(process, &started, &ended, &kernel, &user))
                found.obsStarted = (static_cast<ULONGLONG>(started.dwHighDateTime) << 32) | started.dwLowDateTime;
            CloseHandle(process);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

struct ObsRecording {
    std::wstring path;
    std::streamoff consumed = 0;
    ULONGLONG processStarted = 0;
    bool recording = false;

    bool read(ULONGLONG started) {
        if (!started) {
            *this = {};
            return false;
        }
        wchar_t roaming[MAX_PATH]{};
        const auto length = GetEnvironmentVariableW(L"APPDATA", roaming, static_cast<DWORD>(std::size(roaming)));
        if (!length || length >= std::size(roaming)) return false;
        const std::wstring directory = std::wstring(roaming) + L"\\obs-studio\\logs\\";
        WIN32_FIND_DATAW data{};
        const auto entries = FindFirstFileW((directory + L"*.txt").c_str(), &data);
        if (entries == INVALID_HANDLE_VALUE) return false;
        std::wstring latest;
        ULONGLONG newest = 0;
        do {
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            const auto created = (static_cast<ULONGLONG>(data.ftCreationTime.dwHighDateTime) << 32) |
                data.ftCreationTime.dwLowDateTime;
            if (created < started || created <= newest) continue;
            latest = directory + data.cFileName;
            newest = created;
        } while (FindNextFileW(entries, &data));
        FindClose(entries);
        if (latest.empty()) return false;
        return readLog(std::move(latest), started);
    }

    bool readLog(std::wstring latest, ULONGLONG started) {
        std::ifstream log{std::filesystem::path(latest), std::ios::binary | std::ios::ate};
        if (!log) return false;
        const auto size = static_cast<std::streamoff>(log.tellg());
        if (size < 0) return false;
        const bool continuing = latest == path && started == processStarted && size >= consumed;
        auto offset = continuing ? consumed : 0;
        bool active = continuing && recording;
        log.seekg(offset);
        std::string line;
        while (std::getline(log, line)) {
            if (log.eof()) break;
            const auto message = std::string_view(line).substr(std::min<size_t>(line.size(), 14));
            if (message.starts_with("==== Recording Start ")) active = true;
            if (message.starts_with("==== Recording Stop ") || message.starts_with("==== Shutting down ")) active = false;
            offset = static_cast<std::streamoff>(log.tellg());
        }
        if (log.bad()) return false;
        path = std::move(latest);
        consumed = offset;
        processStarted = started;
        recording = active;
        return recording;
    }
};

// The executable's FileDescription ("Brave Browser"), or its file name without ".exe".
std::wstring appDescription(std::wstring const& path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size) {
        std::vector<uint8_t> data(size);
        void* translation = nullptr;
        UINT bytes = 0;
        if (GetFileVersionInfoW(path.c_str(), 0, size, data.data()) &&
            VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", &translation, &bytes) &&
            translation && bytes >= 2 * sizeof(WORD)) {
            const auto* words = static_cast<WORD const*>(translation);
            wchar_t key[64]{};
            swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\FileDescription", words[0], words[1]);
            wchar_t* value = nullptr;
            UINT length = 0;
            if (VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&value), &length) && value && length > 1) {
                std::wstring description(value, wcsnlen(value, length));
                while (!description.empty() && iswspace(description.back())) description.pop_back();
                if (!description.empty()) return description;
            }
        }
    }
    return captureName(path, true);
}

// Sessions stay keyed by executable path for volume changes.
template <typename Visit>
void forEachAppSession(IMMDeviceEnumerator& devices, bool readIcon, Visit&& visit) {
    winrt::com_ptr<IMMDevice> output;
    winrt::com_ptr<IAudioSessionManager2> manager;
    winrt::com_ptr<IAudioSessionEnumerator> sessions;
    int count = 0;
    if (FAILED(devices.GetDefaultAudioEndpoint(eRender, eMultimedia, output.put())) ||
        FAILED(output->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, nullptr, manager.put_void())) ||
        FAILED(manager->GetSessionEnumerator(sessions.put())) || FAILED(sessions->GetCount(&count))) return;
    for (int i = 0; i < count; ++i) {
        winrt::com_ptr<IAudioSessionControl> session;
        AudioSessionState state{};
        if (FAILED(sessions->GetSession(i, session.put())) || FAILED(session->GetState(&state)) ||
            state == AudioSessionStateExpired) continue;
        const auto details = session.try_as<IAudioSessionControl2>();
        if (!details) continue;
        std::wstring id, iconId;
        if (details->IsSystemSoundsSession() == S_OK) {
            id = L"system";
        } else {
            DWORD pid = 0;
            if (FAILED(details->GetProcessId(&pid)) || !pid || pid == GetCurrentProcessId()) continue;
            id = processPath(pid, readIcon ? &iconId : nullptr);
            if (id.empty()) continue;
        }
        const auto volume = session.try_as<ISimpleAudioVolume>();
        if (volume) visit(id, state == AudioSessionStateActive, *volume, iconId);
    }
}

std::wstring activeMicrophone(IMMDeviceEnumerator& devices, std::wstring registryApp) {
    winrt::com_ptr<IMMDeviceCollection> inputs;
    UINT count = 0;
    if (FAILED(devices.EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, inputs.put())) ||
        FAILED(inputs->GetCount(&count))) return registryApp;

    bool allInputsMuted = true;
    bool registryAppMuted = false;
    bool registryAppLive = false;
    bool unknownLiveSession = false;
    DWORD liveProcess = 0;
    for (UINT index = 0; index < count; ++index) {
        winrt::com_ptr<IMMDevice> input;
        if (FAILED(inputs->Item(index, input.put()))) {
            allInputsMuted = false;
            unknownLiveSession = true;
            continue;
        }
        winrt::com_ptr<IAudioEndpointVolume> endpoint;
        BOOL mute = FALSE;
        const bool muted = SUCCEEDED(input->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,
                                                      nullptr, endpoint.put_void())) &&
                           SUCCEEDED(endpoint->GetMute(&mute)) && mute;
        allInputsMuted = allInputsMuted && muted;
        if (muted && registryApp.empty()) continue;

        winrt::com_ptr<IAudioSessionManager2> manager;
        winrt::com_ptr<IAudioSessionEnumerator> sessions;
        int sessionCount = 0;
        if (FAILED(input->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, nullptr, manager.put_void())) ||
            FAILED(manager->GetSessionEnumerator(sessions.put())) || FAILED(sessions->GetCount(&sessionCount))) {
            if (!muted) unknownLiveSession = true;
            continue;
        }
        for (int i = 0; i < sessionCount; ++i) {
            winrt::com_ptr<IAudioSessionControl> session;
            AudioSessionState state{};
            if (FAILED(sessions->GetSession(i, session.put())) || FAILED(session->GetState(&state))) {
                if (!muted) unknownLiveSession = true;
                continue;
            }
            if (state != AudioSessionStateActive) continue;
            const auto details = session.try_as<IAudioSessionControl2>();
            DWORD pid = 0;
            if (!details || FAILED(details->GetProcessId(&pid)) || !pid) {
                if (!muted) unknownLiveSession = true;
                continue;
            }
            if (pid == GetCurrentProcessId()) continue;
            if (!muted && !liveProcess) liveProcess = pid;
            if (registryApp.empty()) continue;
            const auto label = processLabel(pid, L"");
            if (label.empty() && !muted) unknownLiveSession = true;
            if (_wcsicmp(registryApp.c_str(), label.c_str()) == 0) {
                if (muted) registryAppMuted = true;
                else registryAppLive = true;
            }
        }
    }
    if (allInputsMuted) return {};
    if (!registryApp.empty() && (!registryAppMuted || registryAppLive || unknownLiveSession)) return registryApp;
    return liveProcess ? processLabel(liveProcess, L"Audio input") : std::wstring{};
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

void watchCaptureRegistry(HWND window, HANDLE stop) {
    RegistryKey key;
    RegOpenKeyExW(HKEY_CURRENT_USER, consentStore, 0, KEY_NOTIFY, &key.value);
    const auto changed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    const auto arm = [&] {
        return key.value && changed && RegNotifyChangeKeyValue(key.value, TRUE,
            REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, changed, TRUE) == ERROR_SUCCESS;
    };
    bool watching = arm();
    if (watching) PostMessageW(window, DesktopMonitor::captureChanged, 0, 0);

    const HANDLE handles[]{stop, changed};
    for (;;) {
        const auto signaled = WaitForMultipleObjects(watching ? 2 : 1, handles, FALSE, INFINITE);
        if (signaled == WAIT_OBJECT_0 || signaled == WAIT_FAILED) break;
        if (signaled == WAIT_OBJECT_0 + 1) {
            watching = arm();
            PostMessageW(window, DesktopMonitor::captureChanged, 0, 0);
        }
    }
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

bool activateMediaWindow(HWND overlay, std::wstring const& appId) {
    if (appId.empty()) return false;
    const auto tail = appId.find_last_of(L"\\/!");
    auto executable = appId.substr(tail == std::wstring::npos ? 0 : tail + 1);
    if (executable.size() < 4 || _wcsicmp(executable.c_str() + executable.size() - 4, L".exe") != 0)
        executable += L".exe";

    struct Search {
        HWND overlay;
        std::wstring const& appId;
        std::wstring const& executable;
        HWND exact = nullptr;
        HWND fallback = nullptr;
    } search{overlay, appId, executable};

    EnumWindows([](HWND window, LPARAM context) -> BOOL {
        auto& match = *reinterpret_cast<Search*>(context);
        if (window == match.overlay || !IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return TRUE;

        winrt::com_ptr<IPropertyStore> properties;
        if (SUCCEEDED(SHGetPropertyStoreForWindow(window, IID_PPV_ARGS(properties.put())))) {
            PROPVARIANT id{};
            const bool same = SUCCEEDED(properties->GetValue(PKEY_AppUserModel_ID, &id)) &&
                id.vt == VT_LPWSTR && id.pwszVal && _wcsicmp(id.pwszVal, match.appId.c_str()) == 0;
            PropVariantClear(&id);
            if (same) {
                match.exact = window;
                return FALSE;
            }
        }

        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return TRUE;
        wchar_t id[APPLICATION_USER_MODEL_ID_MAX_LENGTH]{};
        UINT32 idSize = static_cast<UINT32>(std::size(id));
        const bool sameId = GetApplicationUserModelId(process, &idSize, id) == ERROR_SUCCESS &&
            _wcsicmp(id, match.appId.c_str()) == 0;
        wchar_t path[1024]{};
        DWORD pathSize = static_cast<DWORD>(std::size(path));
        const bool hasPath = QueryFullProcessImageNameW(process, 0, path, &pathSize) != FALSE;
        CloseHandle(process);
        if (sameId || (hasPath && _wcsicmp(path, match.appId.c_str()) == 0)) {
            match.exact = window;
            return FALSE;
        }
        if (hasPath && !match.fallback) {
            const auto* slash = wcsrchr(path, L'\\');
            if (_wcsicmp(slash ? slash + 1 : path, match.executable.c_str()) == 0) match.fallback = window;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));

    const auto window = search.exact ? search.exact : search.fallback;
    if (!window) return false;
    if (IsIconic(window)) ShowWindowAsync(window, SW_RESTORE);
    return SetForegroundWindow(window) != FALSE;
}

CaptureActivity readCaptureActivity() {
    RegistryKey root;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, consentStore, 0, KEY_READ, &root.value) != ERROR_SUCCESS) return {};
    return {captureApp(root.value, L"microphone"), captureApp(root.value, L"webcam")};
}

struct DesktopMonitor::State {
    ObsRecording obs;
    HWND window = nullptr;
    HWINEVENTHOOK foreground = nullptr;
    HANDLE stop = nullptr;
    std::thread worker;
    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    winrt::com_ptr<IMMNotificationClient> devices;
    bool mediaFoundation = false;
    winrt::com_ptr<CameraActivity> cameraActivity;
    winrt::com_ptr<IMFSensorActivityMonitor> cameraMonitor;
    std::wstring audioOutput;
    mutable std::map<std::wstring, std::wstring> appNames;   // executable path -> display name
    winrt::Windows::UI::Notifications::ToastNotificationManagerForUser notifications{nullptr};
    winrt::Windows::UI::Notifications::ToastNotificationManagerForUser::NotificationModeChanged_revoker notificationMode;
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
        state_->worker = std::thread(watchCaptureRegistry, window, state_->stop);
    }
}

void DesktopMonitor::refreshAudioDevice() {
    state_->audioOutput.clear();
    if (!state_->enumerator) return;
    winrt::com_ptr<IMMDevice> device;
    if (FAILED(state_->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.put()))) return;
    state_->audioOutput = deviceName(*device);
}

std::wstring const& DesktopMonitor::audioOutput() const { return state_->audioOutput; }

CaptureActivity DesktopMonitor::captureActivity() const {
    auto activity = readCaptureActivity();
    if (state_->enumerator) activity.microphone = activeMicrophone(*state_->enumerator, std::move(activity.microphone));
    if (activity.camera.empty() && state_->cameraActivity) {
        if (const auto pid = state_->cameraActivity->process()) {
            activity.camera = processLabel(pid, L"Camera in use");
        }
    }
    auto screen = screenCaptureWindows();
    activity.recording = state_->obs.read(screen.obsStarted);
    activity.screen = activity.recording ? L"OBS Studio" : std::move(screen.sharing);
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
        bool headphones = false;
        winrt::com_ptr<IPropertyStore> properties;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, properties.put()))) {
            PROPVARIANT factor{};
            if (SUCCEEDED(properties->GetValue(PKEY_AudioEndpoint_FormFactor, &factor)) && factor.vt == VT_UI4)
                headphones = factor.ulVal == Headphones || factor.ulVal == Headset;
            PropVariantClear(&factor);
        }
        outputs.push_back({std::move(id), name.empty() ? L"Audio device " + std::to_wstring(i + 1) : std::move(name),
                           selected, headphones});
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

std::vector<AppVolume> DesktopMonitor::appVolumes() const {
    if (!state_->enumerator) return {};
    std::vector<AppVolume> apps;
    forEachAppSession(*state_->enumerator, true, [&](std::wstring const& id, bool active, ISimpleAudioVolume& session,
                                                      std::wstring const& iconId) {
        float level = 1;
        BOOL mute = FALSE;
        if (FAILED(session.GetMasterVolume(&level)) || FAILED(session.GetMute(&mute))) return;
        auto app = std::ranges::find(apps, id, &AppVolume::id);
        if (app == apps.end()) {
            auto& name = state_->appNames[id];
            if (name.empty()) name = id == L"system" ? L"System sounds" : appDescription(id);
            apps.push_back({id, name, level, mute != FALSE, active, iconId});
            return;
        }
        // An app with several sessions is muted only if all of them are, and shows its loudest one.
        app->volume = std::max(app->volume, level);
        app->muted = app->muted && mute;
        app->active = app->active || active;
        if (app->iconId.empty()) app->iconId = iconId;
    });
    // Playing apps first, system sounds last, otherwise alphabetical so rows don't jump around.
    std::ranges::stable_sort(apps, [](AppVolume const& a, AppVolume const& b) {
        const bool systemA = a.id == L"system", systemB = b.id == L"system";
        if (systemA != systemB) return systemB;
        if (a.active != b.active) return a.active;
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return apps;
}

bool DesktopMonitor::setAppVolume(std::wstring const& id, std::optional<float> volume, std::optional<bool> muted) {
    if (!state_->enumerator) return false;
    bool changed = false;
    forEachAppSession(*state_->enumerator, false, [&](std::wstring const& session, bool, ISimpleAudioVolume& control,
                                                       std::wstring const&) {
        if (_wcsicmp(session.c_str(), id.c_str()) != 0) return;
        if (volume && SUCCEEDED(control.SetMasterVolume(std::clamp(*volume, 0.0f, 1.0f), nullptr))) changed = true;
        if (muted && SUCCEEDED(control.SetMute(*muted, nullptr))) changed = true;
    });
    return changed;
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
    if (state_->enumerator && state_->devices) state_->enumerator->UnregisterEndpointNotificationCallback(state_->devices.get());
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
