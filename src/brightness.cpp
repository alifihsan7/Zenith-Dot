#include "brightness.hpp"

#include <winrt/base.h>

#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <oleauto.h>
#include <wbemidl.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace zenith {

namespace {

// ---- DDC/CI: the physical monitors behind the primary display ----------------------------------

struct Physical {
    std::vector<PHYSICAL_MONITOR> monitors;
    Physical() {
        const auto monitor = MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY);
        DWORD count = 0;
        if (!GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count) || !count) return;
        monitors.resize(count);
        if (!GetPhysicalMonitorsFromHMONITOR(monitor, count, monitors.data())) monitors.clear();
    }
    ~Physical() {
        if (!monitors.empty()) DestroyPhysicalMonitors(static_cast<DWORD>(monitors.size()), monitors.data());
    }
};

std::optional<float> readDdc() {
    Physical physical;
    for (auto& monitor : physical.monitors) {
        DWORD low = 0, current = 0, high = 0;
        if (GetMonitorBrightness(monitor.hPhysicalMonitor, &low, &current, &high) && high > low)
            return static_cast<float>(current - low) / static_cast<float>(high - low);
    }
    return std::nullopt;
}

bool writeDdc(float value) {
    Physical physical;
    bool any = false;
    for (auto& monitor : physical.monitors) {
        DWORD low = 0, current = 0, high = 0;
        if (!GetMonitorBrightness(monitor.hPhysicalMonitor, &low, &current, &high) || high <= low) continue;
        const auto level = low + static_cast<DWORD>(std::lround(value * static_cast<float>(high - low)));
        any = SetMonitorBrightness(monitor.hPhysicalMonitor, level) || any;
    }
    return any;
}

// ---- WMI: built-in laptop panels ---------------------------------------------------------------

winrt::com_ptr<IWbemServices> wmi() {
    winrt::com_ptr<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(locator.put()))))
        return nullptr;
    winrt::com_ptr<IWbemServices> services;
    const auto space = SysAllocString(L"ROOT\\WMI");
    const auto hr = locator->ConnectServer(space, nullptr, nullptr, nullptr, 0, nullptr, nullptr, services.put());
    SysFreeString(space);
    if (FAILED(hr)) return nullptr;
    CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    return services;
}

winrt::com_ptr<IWbemClassObject> firstInstance(IWbemServices& services, wchar_t const* query) {
    const auto language = SysAllocString(L"WQL");
    const auto text = SysAllocString(query);
    winrt::com_ptr<IEnumWbemClassObject> items;
    const auto hr = services.ExecQuery(language, text, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                       nullptr, items.put());
    SysFreeString(language);
    SysFreeString(text);
    if (FAILED(hr)) return nullptr;
    winrt::com_ptr<IWbemClassObject> item;
    ULONG count = 0;
    if (items->Next(2000, 1, item.put(), &count) != WBEM_S_NO_ERROR || !count) return nullptr;
    return item;
}

std::optional<float> readWmi() {
    const auto services = wmi();
    if (!services) return std::nullopt;
    const auto item = firstInstance(*services, L"SELECT CurrentBrightness FROM WmiMonitorBrightness WHERE Active=TRUE");
    if (!item) return std::nullopt;
    VARIANT value;
    VariantInit(&value);
    std::optional<float> result;
    if (SUCCEEDED(item->Get(L"CurrentBrightness", 0, &value, nullptr, nullptr)) && value.vt == VT_UI1)
        result = value.bVal / 100.0f;
    VariantClear(&value);
    return result;
}

bool writeWmi(float value) {
    const auto services = wmi();
    if (!services) return false;
    const auto item = firstInstance(*services, L"SELECT * FROM WmiMonitorBrightnessMethods WHERE Active=TRUE");
    if (!item) return false;
    VARIANT path;
    VariantInit(&path);
    if (FAILED(item->Get(L"__PATH", 0, &path, nullptr, nullptr)) || path.vt != VT_BSTR) return false;

    winrt::com_ptr<IWbemClassObject> type;
    winrt::com_ptr<IWbemClassObject> signature;
    winrt::com_ptr<IWbemClassObject> input;
    const auto className = SysAllocString(L"WmiMonitorBrightnessMethods");
    const auto method = SysAllocString(L"WmiSetBrightness");
    bool done = false;
    if (SUCCEEDED(services->GetObject(className, 0, nullptr, type.put(), nullptr)) &&
        SUCCEEDED(type->GetMethod(method, 0, signature.put(), nullptr)) &&
        SUCCEEDED(signature->SpawnInstance(0, input.put()))) {
        VARIANT timeout;
        VariantInit(&timeout);
        timeout.vt = VT_I4;
        timeout.lVal = 0;
        VARIANT level;
        VariantInit(&level);
        level.vt = VT_UI1;
        level.bVal = static_cast<BYTE>(std::clamp(std::lround(value * 100), 0L, 100L));
        input->Put(L"Timeout", 0, &timeout, 0);
        input->Put(L"Brightness", 0, &level, 0);
        done = SUCCEEDED(services->ExecMethod(path.bstrVal, method, 0, nullptr, input.get(), nullptr, nullptr));
    }
    SysFreeString(className);
    SysFreeString(method);
    VariantClear(&path);
    return done;
}

}

struct BrightnessControl::State {
    mutable std::mutex lock;
    std::condition_variable wake;
    HWND window = nullptr;
    std::optional<float> level;
    std::optional<float> pending;
    bool read = false;
    bool stopping = false;
    bool useWmi = false;
    std::thread worker;
};

namespace {

void worker(std::shared_ptr<BrightnessControl::State> state) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    for (;;) {
        std::optional<float> target;
        bool read = false;
        bool useWmi = false;
        {
            std::unique_lock guard(state->lock);
            state->wake.wait(guard, [&] { return state->stopping || state->pending || state->read; });
            if (state->stopping) break;
            target = std::exchange(state->pending, std::nullopt);
            read = std::exchange(state->read, false);
            useWmi = state->useWmi;
        }
        if (target) {
            if (useWmi) writeWmi(*target);
            else if (!writeDdc(*target)) useWmi = writeWmi(*target);
        }
        if (!read) continue;
        auto level = useWmi ? std::nullopt : readDdc();
        if (!level) {
            level = readWmi();
            useWmi = level.has_value();
        }
        HWND window = nullptr;
        {
            std::scoped_lock guard(state->lock);
            state->useWmi = useWmi;
            if (state->level != level && !state->pending) {
                state->level = level;
                window = state->window;
            }
        }
        if (window) PostMessageW(window, BrightnessControl::changed, 0, 0);
    }
    winrt::uninit_apartment();
}

}

BrightnessControl::BrightnessControl() : state_(std::make_shared<State>()) {}

BrightnessControl::~BrightnessControl() {
    stop();
}

void BrightnessControl::start(HWND window) {
    {
        std::scoped_lock guard(state_->lock);
        state_->window = window;
        state_->stopping = false;
        state_->read = true;
    }
    state_->worker = std::thread(worker, state_);
}

void BrightnessControl::stop() {
    {
        std::scoped_lock guard(state_->lock);
        state_->stopping = true;
        state_->window = nullptr;
    }
    state_->wake.notify_all();
    if (state_->worker.joinable()) state_->worker.join();
}

void BrightnessControl::refresh() {
    {
        std::scoped_lock guard(state_->lock);
        if (state_->stopping) return;
        state_->read = true;
    }
    state_->wake.notify_all();
}

std::optional<float> BrightnessControl::level() const {
    std::scoped_lock guard(state_->lock);
    return state_->level;
}

void BrightnessControl::set(float value) {
    {
        std::scoped_lock guard(state_->lock);
        if (!state_->level || state_->stopping) return;
        // Show the new level right away; the worker sends it to the display in the background.
        state_->level = std::clamp(value, 0.0f, 1.0f);
        state_->pending = state_->level;
    }
    state_->wake.notify_all();
}

}
