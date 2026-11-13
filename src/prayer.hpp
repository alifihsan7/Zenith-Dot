#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <windows.h>
#include <winrt/Windows.Foundation.h>

namespace zenith {

inline constexpr std::array prayerNames{L"Subuh", L"Dzuhur", L"Ashar", L"Magrib", L"Isya"};

struct PrayerLocation {
    wchar_t const* name;
    wchar_t const* province;
    int utcOffset;

    constexpr wchar_t const* zone() const {
        return utcOffset == 9 ? L"WIT" : utcOffset == 8 ? L"WITA" : L"WIB";
    }
};

#include "prayer_locations.inc"

struct PrayerSchedule {
    wchar_t const* city;
    std::chrono::sys_days date;
    std::array<int, prayerNames.size()> times;
    int utcOffset;
    std::optional<int> tomorrowSubuh;
    wchar_t const* zone = L"";
    bool operator==(PrayerSchedule const&) const = default;
};

struct PrayerMonth {
    unsigned city = 0;
    std::chrono::year_month month{};
    std::array<std::optional<std::array<int, prayerNames.size()>>, 31> days{};
};

enum class PrayerLoad { loading, unavailable };

struct PrayerProgress {
    double localMinute;
    std::size_t nextIndex;
    int nextMinute;
    std::chrono::minutes remaining;
    bool tomorrow;
};

PrayerProgress prayerProgress(PrayerSchedule const& schedule, std::chrono::system_clock::time_point now);
std::chrono::sys_days prayerDate(unsigned city, std::chrono::system_clock::time_point now);
std::optional<PrayerSchedule> prayerSchedule(PrayerMonth const& month, std::chrono::sys_days date,
                                              PrayerMonth const& nextMonth);
std::wstring prayerTimeText(int minute);
std::optional<unsigned> choosePrayerLocation(HWND owner, unsigned current);
winrt::Windows::Foundation::IAsyncAction fetchPrayerMonth(unsigned city, std::chrono::year_month month,
                                                         std::shared_ptr<PrayerMonth> result);

}
