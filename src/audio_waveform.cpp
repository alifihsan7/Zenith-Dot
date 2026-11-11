#include "audio_waveform.hpp"

#include <windows.h>
#include <appmodel.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <tlhelp32.h>
#include <winrt/base.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace zenith {
namespace {

constexpr DWORD sampleRate = 44'100;
constexpr WORD channelCount = 2;
constexpr DWORD framesPerBar = sampleRate * 30 / 1000;

std::wstring_view fileName(std::wstring_view path) {
    const auto separator = path.find_last_of(L"\\/");
    return separator == path.npos ? path : path.substr(separator + 1);
}

bool sameName(std::wstring_view first, std::wstring_view second) {
    return CompareStringOrdinal(first.data(), static_cast<int>(first.size()),
                                second.data(), static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
}

bool matchesApp(HANDLE process, std::wstring_view app) {
    if (app.find(L'!') != app.npos) {
        wchar_t id[APPLICATION_USER_MODEL_ID_MAX_LENGTH]{};
        UINT32 length = static_cast<UINT32>(std::size(id));
        return GetApplicationUserModelId(process, &length, id) == ERROR_SUCCESS && sameName(id, app);
    }

    wchar_t path[1024]{};
    DWORD length = static_cast<DWORD>(std::size(path));
    if (!QueryFullProcessImageNameW(process, 0, path, &length)) return false;
    const std::wstring_view fullPath(path, length);
    if (app != fileName(app)) return sameName(fullPath, app);
    auto executable = fileName(fullPath);
    if (app.size() >= 4 && sameName(app.substr(app.size() - 4), L".exe")) return sameName(executable, app);
    if (executable.size() >= 4 && sameName(executable.substr(executable.size() - 4), L".exe"))
        executable.remove_suffix(4);
    return sameName(executable, app);
}

winrt::handle findProcess(IMMDeviceEnumerator& devices, std::wstring_view app, DWORD& pid) {
    winrt::com_ptr<IMMDeviceCollection> outputs;
    UINT count = 0;
    if (FAILED(devices.EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, outputs.put())) ||
        FAILED(outputs->GetCount(&count))) return {};
    for (UINT index = 0; index < count; ++index) {
        winrt::com_ptr<IMMDevice> output;
        winrt::com_ptr<IAudioSessionManager2> manager;
        winrt::com_ptr<IAudioSessionEnumerator> sessions;
        int sessionCount = 0;
        if (FAILED(outputs->Item(index, output.put())) ||
            FAILED(output->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, nullptr, manager.put_void())) ||
            FAILED(manager->GetSessionEnumerator(sessions.put())) || FAILED(sessions->GetCount(&sessionCount))) continue;
        for (int i = 0; i < sessionCount; ++i) {
            winrt::com_ptr<IAudioSessionControl> session;
            AudioSessionState state{};
            if (FAILED(sessions->GetSession(i, session.put())) || FAILED(session->GetState(&state)) ||
                state != AudioSessionStateActive) continue;
            const auto details = session.try_as<IAudioSessionControl2>();
            DWORD candidate = 0;
            if (!details || details->IsSystemSoundsSession() == S_OK ||
                details->GetProcessId(&candidate) != S_OK || !candidate || candidate == GetCurrentProcessId()) continue;
            winrt::handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, candidate)};
            if (process && matchesApp(process.get(), app)) {
                pid = candidate;
                return process;
            }
        }
    }
    return {};
}

winrt::handle findProcessTree(std::wstring_view app, DWORD& pid) {
    if (app.find(L'!') != app.npos) return {};
    auto executable = std::wstring(fileName(app));
    if (executable.size() < 4 || !sameName(std::wstring_view(executable).substr(executable.size() - 4), L".exe"))
        executable += L".exe";

    DWORD currentSession = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &currentSession)) return {};
    const HANDLE raw = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (raw == INVALID_HANDLE_VALUE) return {};
    const winrt::handle snapshot{raw};
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.get(), &entry)) return {};

    struct Match {
        DWORD pid;
        DWORD parent;
        winrt::handle process;
    };
    std::vector<Match> matches;
    do {
        if (entry.th32ProcessID == GetCurrentProcessId() || !sameName(entry.szExeFile, executable)) continue;
        DWORD session = 0;
        if (!ProcessIdToSessionId(entry.th32ProcessID, &session) || session != currentSession) continue;
        winrt::handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, entry.th32ProcessID)};
        if (process && matchesApp(process.get(), app))
            matches.push_back({entry.th32ProcessID, entry.th32ParentProcessID, std::move(process)});
    } while (Process32NextW(snapshot.get(), &entry));

    Match* root = nullptr;
    for (auto& match : matches) {
        if (std::ranges::find(matches, match.parent, &Match::pid) != matches.end()) continue;
        if (root) return {};
        root = &match;
    }
    if (!root) return {};
    pid = root->pid;
    return std::move(root->process);
}

struct Activation : winrt::implements<Activation, IActivateAudioInterfaceCompletionHandler> {
    winrt::handle done{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    winrt::com_ptr<IAudioClient> client;

    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) noexcept final {
        HRESULT result = E_FAIL;
        winrt::com_ptr<IUnknown> audio;
        if (SUCCEEDED(operation->GetActivateResult(&result, audio.put())) && SUCCEEDED(result) && audio)
            client = audio.try_as<IAudioClient>();
        SetEvent(done.get());
        return S_OK;
    }
};

winrt::com_ptr<IAudioClient> activate(DWORD pid, HANDLE stop) {
    const auto completion = winrt::make_self<Activation>();
    if (!completion->done) return {};
    AUDIOCLIENT_ACTIVATION_PARAMS activation{};
    activation.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    activation.ProcessLoopbackParams.TargetProcessId = pid;
    activation.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT parameters{};
    parameters.vt = VT_BLOB;
    parameters.blob = {sizeof(activation), reinterpret_cast<BYTE*>(&activation)};
    winrt::com_ptr<IActivateAudioInterfaceAsyncOperation> operation;
    if (FAILED(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                           &parameters, completion.get(), operation.put()))) return {};
    const HANDLE events[]{stop, completion->done.get()};
    return WaitForMultipleObjects(2, events, FALSE, 3000) == WAIT_OBJECT_0 + 1 ? completion->client : nullptr;
}

struct AudioEnvelope {
    float bassLeft = 0;
    float bassRight = 0;
    float bassAverage = 0;
    float power = 0;
    float bassPower = 0;
    DWORD frames = 0;

    template <typename Emit>
    void consume(int16_t const* samples, DWORD count, Emit&& emit) {
        for (DWORD frame = 0; frame < count; ++frame) {
            const float left = samples ? samples[channelCount * frame] / 32768.0f : 0;
            const float right = samples ? samples[channelCount * frame + 1] / 32768.0f : 0;
            // A low pass at about 155 Hz gives kick and bass more influence on the bars.
            bassLeft += 0.022f * (left - bassLeft);
            bassRight += 0.022f * (right - bassRight);
            power += (left * left + right * right) * 0.5f;
            bassPower += (bassLeft * bassLeft + bassRight * bassRight) * 0.5f;
            if (++frames != framesPerBar) continue;

            const float level = std::sqrt(power / framesPerBar);
            const float bass = std::sqrt(bassPower / framesPerBar);
            const float onset = std::max(0.0f, bass - bassAverage);
            bassAverage += 0.10f * (bass - bassAverage);
            emit(std::clamp(1.8f * level + 0.8f * bass + 3.4f * onset, 0.0f, 1.0f));
            power = bassPower = 0;
            frames = 0;
        }
    }
};

template <typename Emit>
void capture(IAudioClient& client, HANDLE stop, HANDLE process, Emit&& emit) {
    winrt::handle ready{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    const WAVEFORMATEX format{WAVE_FORMAT_PCM, channelCount, sampleRate, sampleRate * channelCount * 2,
                              channelCount * 2, 16, 0};
    constexpr DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    winrt::com_ptr<IAudioCaptureClient> stream;
    if (!ready || FAILED(client.Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, &format, nullptr)) ||
        FAILED(client.SetEventHandle(ready.get())) ||
        FAILED(client.GetService(__uuidof(IAudioCaptureClient), stream.put_void())) || FAILED(client.Start())) return;

    const HANDLE events[]{stop, process, ready.get()};
    AudioEnvelope envelope;
    while (WaitForMultipleObjects(3, events, FALSE, 1000) == WAIT_OBJECT_0 + 2) {
        UINT32 frames = 0;
        bool failed = false;
        while (SUCCEEDED(stream->GetNextPacketSize(&frames)) && frames) {
            BYTE* buffer = nullptr;
            DWORD state = 0;
            if (FAILED(stream->GetBuffer(&buffer, &frames, &state, nullptr, nullptr))) {
                failed = true;
                break;
            }
            envelope.consume(state & AUDCLNT_BUFFERFLAGS_SILENT ? nullptr : reinterpret_cast<int16_t const*>(buffer),
                             frames, emit);
            if (FAILED(stream->ReleaseBuffer(frames))) {
                failed = true;
                break;
            }
        }
        if (failed) break;
    }
    client.Stop();
}

}

void AudioWaveform::select(std::wstring_view app) {
    if (app == app_) return;
    worker_ = {};
    {
        const std::lock_guard lock(mutex_);
        samples_ = {};
    }
    app_ = app;
    if (!app_.empty()) worker_ = std::jthread([this, selected = app_](std::stop_token stop) {
        listen(stop, selected);
    });
}

Waveform AudioWaveform::samples() const {
    const std::lock_guard lock(mutex_);
    return samples_;
}

void AudioWaveform::listen(std::stop_token stop, std::wstring const& app) {
    winrt::handle cancelled{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    if (!cancelled || FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return;
    const std::stop_callback cancel(stop, [&] { SetEvent(cancelled.get()); });
    {
        winrt::com_ptr<IMMDeviceEnumerator> devices;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                       __uuidof(IMMDeviceEnumerator), devices.put_void()))) {
            while (!stop.stop_requested()) {
                DWORD pid = 0;
                auto process = findProcess(*devices, app, pid);
                if (!process) process = findProcessTree(app, pid);
                if (process) {
                    if (const auto client = activate(pid, cancelled.get())) {
                        capture(*client, cancelled.get(), process.get(), [this](float level) {
                            const std::lock_guard lock(mutex_);
                            std::rotate(samples_.begin(), samples_.begin() + 1, samples_.end());
                            samples_.back() = level;
                        });
                    }
                }
                {
                    const std::lock_guard lock(mutex_);
                    samples_ = {};
                }
                if (WaitForSingleObject(cancelled.get(), 1500) != WAIT_TIMEOUT) break;
            }
        }
    }
    CoUninitialize();
}

}
