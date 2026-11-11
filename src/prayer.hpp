#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace zenith {

inline constexpr std::array prayerNames{L"Subuh", L"Dzuhur", L"Ashar", L"Magrib", L"Isya"};

struct PrayerSchedule {
    wchar_t const* city;
    std::chrono::sys_days date;
    std::array<int, prayerNames.size()> times;
    int utcOffset;
    int tomorrowSubuh;
    bool operator==(PrayerSchedule const&) const = default;
};

struct PrayerProgress {
    double localMinute;
    std::size_t nextIndex;
    int nextMinute;
    std::chrono::minutes remaining;
    bool tomorrow;
};

PrayerProgress prayerProgress(PrayerSchedule const& schedule, std::chrono::system_clock::time_point now);
std::chrono::sys_days prayerDate(unsigned city, std::chrono::system_clock::time_point now);
std::optional<PrayerSchedule> prayerSchedule(unsigned city, std::chrono::sys_days date);
std::wstring prayerTimeText(int minute);

}
