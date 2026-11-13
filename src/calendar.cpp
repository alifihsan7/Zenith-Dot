#include "calendar.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Appointments.h>

#include <mutex>

namespace zenith {

using namespace std::chrono_literals;
using winrt::Windows::ApplicationModel::Appointments::AppointmentManager;
using winrt::Windows::ApplicationModel::Appointments::AppointmentProperties;
using winrt::Windows::ApplicationModel::Appointments::AppointmentStore;
using winrt::Windows::ApplicationModel::Appointments::AppointmentStoreAccessType;
using winrt::Windows::ApplicationModel::Appointments::FindAppointmentsOptions;

struct CalendarMonitor::State {
    std::mutex lock;
    HWND window = nullptr;
    std::optional<CalendarEvent> next;
    AppointmentStore store{nullptr};
    winrt::event_token storeChanged{};
    bool busy = false;
    bool again = false;
    bool stopped = false;
};

namespace {

constexpr auto lookBehind = 12h;     // events that started earlier but are still running
constexpr auto lookAhead = 36h;

void schedule(std::shared_ptr<CalendarMonitor::State> const& state);

winrt::fire_and_forget load(std::shared_ptr<CalendarMonitor::State> state) {
    co_await winrt::resume_background();
    for (;;) {
        std::optional<CalendarEvent> found;
        try {
            AppointmentStore store{nullptr};
            {
                std::scoped_lock guard(state->lock);
                if (state->stopped) co_return;
                store = state->store;
            }
            if (!store) {
                store = co_await AppointmentManager::RequestStoreAsync(AppointmentStoreAccessType::AllCalendarsReadOnly);
                std::scoped_lock guard(state->lock);
                if (state->stopped) co_return;
                state->store = store;
                if (store) {
                    std::weak_ptr<CalendarMonitor::State> weak = state;
                    state->storeChanged = store.StoreChanged([weak](auto const&, auto const&) {
                        if (const auto shared = weak.lock()) schedule(shared);
                    });
                }
            }
            if (store) {
                FindAppointmentsOptions options;
                for (const auto& property : {AppointmentProperties::Subject(), AppointmentProperties::Location(),
                                             AppointmentProperties::StartTime(), AppointmentProperties::Duration(),
                                             AppointmentProperties::AllDay()}) {
                    options.FetchProperties().Append(property);
                }
                options.MaxCount(50);
                const auto now = winrt::clock::now();
                const auto items = co_await store.FindAppointmentsAsync(now - lookBehind, lookAhead + lookBehind, options);
                const auto current = std::chrono::system_clock::now();
                for (const auto& item : items) {
                    if (item.AllDay()) continue;
                    const auto start = winrt::clock::to_sys(item.StartTime());
                    const auto end = start + std::chrono::duration_cast<std::chrono::system_clock::duration>(item.Duration());
                    if (end <= current) continue;
                    if (found && found->start <= start) continue;
                    found = CalendarEvent{std::wstring(item.Subject()), std::wstring(item.Location()), start, end};
                }
            }
        } catch (winrt::hresult_error const&) {
            found.reset();
        }

        HWND window = nullptr;
        {
            std::scoped_lock guard(state->lock);
            if (state->stopped) co_return;
            if (state->next != found) {
                state->next = std::move(found);
                window = state->window;
            }
            if (!state->again) {
                state->busy = false;
                if (window) PostMessageW(window, CalendarMonitor::changed, 0, 0);
                co_return;
            }
            state->again = false;
        }
        if (window) PostMessageW(window, CalendarMonitor::changed, 0, 0);
    }
}

void schedule(std::shared_ptr<CalendarMonitor::State> const& state) {
    {
        std::scoped_lock guard(state->lock);
        if (state->stopped) return;
        // One read at a time; a request during a read runs once more afterwards.
        if (state->busy) {
            state->again = true;
            return;
        }
        state->busy = true;
    }
    load(state);
}

}

CalendarMonitor::CalendarMonitor() : state_(std::make_shared<State>()) {}

CalendarMonitor::~CalendarMonitor() {
    stop();
}

void CalendarMonitor::start(HWND window) {
    {
        std::scoped_lock guard(state_->lock);
        state_->window = window;
        state_->stopped = false;
    }
    SetTimer(window, refreshTimer, 5 * 60 * 1000, nullptr);
    refresh();
}

void CalendarMonitor::stop() {
    std::scoped_lock guard(state_->lock);
    if (state_->window) KillTimer(state_->window, refreshTimer);
    state_->stopped = true;
    state_->window = nullptr;
    if (state_->store && state_->storeChanged) {
        try {
            state_->store.StoreChanged(state_->storeChanged);
        } catch (winrt::hresult_error const&) {
        }
    }
    state_->store = nullptr;
}

void CalendarMonitor::refresh() {
    schedule(state_);
}

std::optional<CalendarEvent> CalendarMonitor::next() const {
    std::scoped_lock guard(state_->lock);
    return state_->next;
}

}
