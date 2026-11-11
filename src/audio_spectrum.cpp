#include "audio_spectrum.hpp"

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
#include <complex>
#include <cstdint>
#include <numbers>
#include <vector>

namespace zenith {
namespace {

constexpr DWORD sampleRate = 44'100;
constexpr WORD channelCount = 2;
constexpr size_t fftSize = 2048;
// FFT bin boundaries, roughly logarithmic from 43 Hz to 17.8 kHz.
constexpr std::array<size_t, Spectrum{}.size() + 1> bandEdges{
    2, 3, 4, 5, 7, 9, 12, 16, 22, 30, 41, 56, 77, 105, 144, 197, 270, 370, 506, 693, 825
};

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

class SpectrumAnalyzer {
public:
    SpectrumAnalyzer() {
        constexpr float turn = 2 * std::numbers::pi_v<float>;
        for (size_t i = 0; i < fftSize; ++i)
            window_[i] = 0.5f - 0.5f * std::cos(turn * static_cast<float>(i) / (fftSize - 1));
        for (size_t i = 0; i < twiddle_.size(); ++i)
            twiddle_[i] = std::polar(1.0f, -turn * static_cast<float>(i) / fftSize);
    }

    template <typename Emit>
    void consume(int16_t const* samples, DWORD count, Emit&& emit) {
        for (DWORD frame = 0; frame < count; ++frame) {
            const float weight = window_[frames_] / 32768.0f;
            const auto index = static_cast<size_t>(frame) * channelCount;
            bins_[frames_] = samples ? std::complex<float>{samples[index] * weight, samples[index + 1] * weight} : 0;
            if (++frames_ != fftSize) continue;
            transform();
            emit(levels_);
            frames_ = 0;
        }
    }

private:
    void transform() {
        for (size_t i = 1, reversed = 0; i < fftSize; ++i) {
            size_t bit = fftSize / 2;
            while (reversed & bit) {
                reversed ^= bit;
                bit /= 2;
            }
            reversed ^= bit;
            if (i < reversed) std::swap(bins_[i], bins_[reversed]);
        }
        for (size_t span = 2; span <= fftSize; span *= 2) {
            const size_t half = span / 2;
            for (size_t start = 0; start < fftSize; start += span) {
                for (size_t i = 0; i < half; ++i) {
                    const auto even = bins_[start + i];
                    const auto odd = bins_[start + i + half] * twiddle_[i * (fftSize / span)];
                    bins_[start + i] = even + odd;
                    bins_[start + i + half] = even - odd;
                }
            }
        }
        for (size_t band = 0; band < levels_.size(); ++band) {
            float power = 0;
            // Left and right are packed into the real and imaginary inputs. The paired
            // bins sum their powers without cancelling stereo signals that are out of phase.
            for (size_t bin = bandEdges[band]; bin < bandEdges[band + 1]; ++bin)
                power += std::norm(bins_[bin]) + std::norm(bins_[fftSize - bin]);
            const float magnitude = std::sqrt(power) * (2.0f / fftSize);
            const float level = std::min(1.0f, std::log1p(32 * magnitude) / std::log1p(12.0f));
            auto& shown = levels_[band];
            shown = std::lerp(shown, level, level > shown ? 0.76f : 0.32f);
        }
    }

    std::array<float, fftSize> window_{};
    std::array<std::complex<float>, fftSize> bins_{};
    std::array<std::complex<float>, fftSize / 2> twiddle_{};
    Spectrum levels_{};
    size_t frames_ = 0;
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
    SpectrumAnalyzer analyzer;
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
            analyzer.consume(state & AUDCLNT_BUFFERFLAGS_SILENT ? nullptr : reinterpret_cast<int16_t const*>(buffer),
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

void AudioSpectrum::select(std::wstring_view app) {
    if (app == app_) return;
    worker_ = {};
    {
        const std::lock_guard lock(mutex_);
        levels_ = {};
    }
    app_ = app;
    if (!app_.empty()) worker_ = std::jthread([this, selected = app_](std::stop_token stop) {
        listen(stop, selected);
    });
}

Spectrum AudioSpectrum::levels() const {
    const std::lock_guard lock(mutex_);
    return levels_;
}

void AudioSpectrum::listen(std::stop_token stop, std::wstring const& app) {
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
                        capture(*client, cancelled.get(), process.get(), [this](Spectrum const& levels) {
                            const std::lock_guard lock(mutex_);
                            levels_ = levels;
                        });
                    }
                }
                {
                    const std::lock_guard lock(mutex_);
                    levels_ = {};
                }
                if (WaitForSingleObject(cancelled.get(), 1500) != WAIT_TIMEOUT) break;
            }
        }
    }
    CoUninitialize();
}

}
