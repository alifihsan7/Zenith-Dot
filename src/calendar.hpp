#pragma once

#include <windows.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace zenith {

// The next timed event in the calendars Windows syncs (all-day events are left out).
struct CalendarEvent {
    std::wstring title;
    std::wstring location;
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point end;
    bool operator==(CalendarEvent const&) const = default;
};

class CalendarMonitor {
public:
    static constexpr UINT changed = WM_APP + 23;
    static constexpr UINT_PTR refreshTimer = 7;

    CalendarMonitor();
    ~CalendarMonitor();
    CalendarMonitor(CalendarMonitor const&) = delete;
    CalendarMonitor& operator=(CalendarMonitor const&) = delete;

    void start(HWND window);
    void stop();
    // Reads the store again in the background; posts `changed` when the next event differs.
    void refresh();
    std::optional<CalendarEvent> next() const;

    struct State;

private:
    std::shared_ptr<State> state_;
};

}
