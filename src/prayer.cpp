#include "prayer.hpp"

#include <algorithm>
#include <cwchar>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Filters.h>
#include <winrt/Windows.Web.Http.Headers.h>

namespace zenith {
namespace {

using namespace std::chrono;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::Http;

constexpr wchar_t sihatUrl[] = L"https://bimasislam.kemenag.go.id/api/sihat/";
constexpr std::array fields{L"subuh", L"dzuhur", L"ashar", L"maghrib", L"isya"};

std::wstring dayKey(year_month_day day) {
    wchar_t key[11]{};
    swprintf_s(key, L"%04d-%02u-%02u", int(day.year()), unsigned(day.month()), unsigned(day.day()));
    return key;
}

std::optional<int> parsePrayerTime(winrt::hstring const& value) {
    const auto digit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };
    if (value.size() != 5 || value[2] != L':' || !digit(value[0]) || !digit(value[1]) ||
        !digit(value[3]) || !digit(value[4])) return std::nullopt;
    const int hour = 10 * (value[0] - L'0') + value[1] - L'0';
    const int minute = 10 * (value[3] - L'0') + value[4] - L'0';
    if (hour > 23 || minute > 59) return std::nullopt;
    return hour * 60 + minute;
}

IAsyncOperation<JsonObject> readSihat(HttpClient http, std::wstring path) {
    const auto cancel = co_await winrt::get_cancellation_token();
    cancel.enable_propagation();
    const auto response = co_await http.GetAsync(Uri(std::wstring(sihatUrl) + path));
    response.EnsureSuccessStatusCode();
    const auto body = co_await response.Content().ReadAsStringAsync();
    JsonObject result{nullptr};
    if (body.size() <= 262144) JsonObject::TryParse(body, result);
    co_return result;
}

winrt::hstring regionId(JsonObject const& response, wchar_t const* name) {
    if (!response) return {};
    const auto entries = response.GetNamedArray(L"data", nullptr);
    if (!entries) return {};
    for (auto const& entry : entries) {
        if (entry.ValueType() != JsonValueType::Object) continue;
        const auto region = entry.GetObject();
        const auto label = region.GetNamedString(L"name", L"");
        if (CompareStringOrdinal(label.c_str(), -1, name, -1, TRUE) == CSTR_EQUAL)
            return region.GetNamedString(L"id", L"");
    }
    return {};
}

std::optional<std::array<int, prayerNames.size()>> readDay(JsonObject const& table, year_month_day day) {
    const auto row = table.GetNamedObject(dayKey(day), nullptr);
    if (!row) return std::nullopt;
    std::array<int, prayerNames.size()> times{};
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto time = parsePrayerTime(row.GetNamedString(fields[i], L""));
        if (!time || (i && *time <= times[i - 1])) return std::nullopt;
        times[i] = *time;
    }
    return times;
}

}

IAsyncAction fetchPrayerMonth(unsigned city, year_month month, std::shared_ptr<PrayerMonth> result) {
    const auto cancel = co_await winrt::get_cancellation_token();
    cancel.enable_propagation();
    if (!city || city > prayerLocations.size() || !month.ok()) co_return;
    const auto& location = prayerLocations[city - 1];
    const Filters::HttpBaseProtocolFilter filter;
    filter.AllowAutoRedirect(false);
    filter.CookieUsageBehavior(Filters::HttpCookieUsageBehavior::NoCookies);
    const HttpClient http(filter);
    http.DefaultRequestHeaders().Append(L"Accept", L"application/json");
    const auto provinces = co_await readSihat(http, L"provinsi");
    const auto province = regionId(provinces, location.province);
    if (province.empty()) co_return;
    const std::wstring provinceQuery = Uri::EscapeComponent(province).c_str();
    const auto regions = co_await readSihat(http, L"kabko?prov=" + provinceQuery);
    const auto region = regionId(regions, location.name);
    if (region.empty()) co_return;
    const std::wstring regionQuery = Uri::EscapeComponent(region).c_str();
    const unsigned number = unsigned(month.month());
    const auto response = co_await readSihat(http, L"jadwal?prov=" + provinceQuery + L"&kabko=" + regionQuery +
        L"&thn=" + std::to_wstring(int(month.year())) + L"&bln=" + (number < 10 ? L"0" : L"") + std::to_wstring(number));
    if (!response || response.GetNamedNumber(L"status", 0) != 1) co_return;
    const auto actual = response.GetNamedString(L"kabko", L"");
    if (CompareStringOrdinal(actual.c_str(), -1, location.name, -1, TRUE) != CSTR_EQUAL) co_return;
    const auto table = response.GetNamedObject(L"data", nullptr);
    if (!table) co_return;
    PrayerMonth parsed;
    const unsigned dayCount = unsigned(year_month_day_last{month / std::chrono::last}.day());
    for (unsigned day = 1; day <= dayCount; ++day)
        parsed.days[day - 1] = readDay(table, month / std::chrono::day(day));
    if (std::ranges::none_of(parsed.days, [](auto const& day) { return day.has_value(); })) co_return;
    parsed.city = city;
    parsed.month = month;
    *result = std::move(parsed);
}

std::chrono::sys_days prayerDate(unsigned city, std::chrono::system_clock::time_point now) {
    if (!city || city > prayerLocations.size()) return {};
    return floor<days>(now + hours(prayerLocations[city - 1].utcOffset));
}

std::optional<PrayerSchedule> prayerSchedule(PrayerMonth const& month, sys_days date, PrayerMonth const& nextMonth) {
    if (!month.city || month.city > prayerLocations.size()) return std::nullopt;
    const year_month_day today{date};
    const auto todayMonth = today.year() / today.month();
    if (todayMonth != month.month) return std::nullopt;
    const auto& times = month.days[unsigned(today.day()) - 1];
    if (!times) return std::nullopt;
    const year_month_day tomorrow{date + days(1)};
    const auto tomorrowMonth = tomorrow.year() / tomorrow.month();
    const auto& source = tomorrowMonth == todayMonth ? month : nextMonth;
    std::optional<int> nextSubuh;
    if (source.city == month.city && source.month == tomorrowMonth) {
        const auto& next = source.days[unsigned(tomorrow.day()) - 1];
        if (next) nextSubuh = next->front();
    }
    const auto& city = prayerLocations[month.city - 1];
    return PrayerSchedule{city.name, date, *times, city.utcOffset, nextSubuh, city.zone()};
}

PrayerProgress prayerProgress(PrayerSchedule const& schedule, system_clock::time_point now) {
    const auto local = now + hours(schedule.utcOffset);
    const auto localDate = floor<days>(local);
    const auto elapsed = duration_cast<duration<double, minutes::period>>(local - localDate).count();
    const auto next = localDate == schedule.date ? std::ranges::upper_bound(schedule.times, elapsed) : schedule.times.end();
    if (next != schedule.times.end()) {
        return {elapsed, static_cast<size_t>(next - schedule.times.begin()), *next,
                ceil<minutes>(schedule.date + minutes(*next) - local), false};
    }
    if (!schedule.tomorrowSubuh || localDate > schedule.date + days(1))
        return {elapsed, prayerNames.size(), 0, minutes::zero(), false};
    const auto nextDate = schedule.date + days(1);
    const auto remaining = std::max(minutes::zero(), ceil<minutes>(nextDate + minutes(*schedule.tomorrowSubuh) - local));
    return {elapsed, 0, *schedule.tomorrowSubuh, remaining, nextDate > localDate};
}

std::wstring prayerTimeText(int minute) {
    wchar_t result[6]{};
    swprintf_s(result, L"%02d:%02d", minute / 60, minute % 60);
    return result;
}

}
