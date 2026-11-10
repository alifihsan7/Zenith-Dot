#include "lyrics.hpp"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Web.Http.Filters.h>
#include <winrt/Windows.Web.Http.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string_view>

namespace zenith {
namespace {

using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::Http;

std::wstring urlEncode(std::wstring_view value) {
    constexpr wchar_t hex[] = L"0123456789ABCDEF";
    std::wstring result;
    for (const unsigned char byte : winrt::to_string(value)) {
        if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
            result += static_cast<wchar_t>(byte);
        } else {
            result += L'%';
            result += hex[byte >> 4];
            result += hex[byte & 15];
        }
    }
    return result;
}

HttpClient client() {
    const Filters::HttpBaseProtocolFilter filter;
    filter.AllowAutoRedirect(false);
    filter.CookieUsageBehavior(Filters::HttpCookieUsageBehavior::NoCookies);
    filter.CacheControl().ReadBehavior(Filters::HttpCacheReadBehavior::NoCache);
    filter.CacheControl().WriteBehavior(Filters::HttpCacheWriteBehavior::NoCache);
    return HttpClient(filter);
}

JsonObject jsonBody(winrt::hstring const& body) {
    JsonObject result{nullptr};
    if (body.size() <= 262'144) JsonObject::TryParse(body, result);
    return result;
}

std::wstring field(JsonObject const& object, winrt::hstring const& key) {
    const auto value = object ? object.GetNamedValue(key, nullptr) : nullptr;
    return value && value.ValueType() == JsonValueType::String ? std::wstring(value.GetString()) : std::wstring{};
}

std::vector<LyricLine> timedLines(std::wstring_view lrc) {
    constexpr size_t maxLines = 4096;
    constexpr size_t textLimit = 131'072;
    std::vector<LyricLine> lines;
    size_t textSize = 0;
    while (!lrc.empty() && lines.size() < maxLines) {
        const auto newline = lrc.find(L'\n');
        auto row = lrc.substr(0, newline);
        if (newline == std::wstring_view::npos) lrc = {};
        else lrc.remove_prefix(newline + 1);
        if (!row.empty() && row.back() == L'\r') row.remove_suffix(1);

        std::vector<double> stamps;
        while (row.starts_with(L"[") && stamps.size() < maxLines - lines.size()) {
            const auto close = row.find(L']');
            const auto colon = row.find(L':');
            if (close == std::wstring_view::npos || colon == 0 || colon >= close) break;
            const auto minutes = row.substr(1, colon - 1);
            const auto seconds = row.substr(colon + 1, close - colon - 1);
            if (minutes.empty() || minutes.size() > 3 || seconds.empty() || seconds.size() > 6 ||
                minutes.find_first_not_of(L"0123456789") != std::wstring_view::npos ||
                seconds.find_first_not_of(L"0123456789.") != std::wstring_view::npos) break;
            wchar_t* end = nullptr;
            const std::wstring value(seconds);
            const double second = std::wcstod(value.c_str(), &end);
            if (end != value.c_str() + value.size() || second >= 60) break;
            const double minute = std::wcstol(std::wstring(minutes).c_str(), nullptr, 10);
            stamps.push_back(60 * minute + second);
            row.remove_prefix(close + 1);
        }
        const auto start = row.find_first_not_of(L" \t");
        const auto lyric = start == std::wstring_view::npos ? std::wstring_view{} : row.substr(start);
        for (const double stamp : stamps) {
            if (lyric.size() > textLimit - textSize) break;
            lines.push_back({stamp, std::wstring(lyric)});
            textSize += lyric.size();
        }
    }
    if (std::ranges::all_of(lines, [](LyricLine const& line) { return line.text.empty(); })) return {};
    std::ranges::stable_sort(lines, {}, &LyricLine::seconds);
    return lines;
}

}

Lyrics::Lyrics(std::function<void()> changed) : changed_(std::move(changed)) {}

void Lyrics::cancel() {
    ++revision_;
    if (request_) request_.Cancel();
    request_ = nullptr;
}

void Lyrics::stop() {
    stopped_ = true;
    changed_ = {};
    cancel();
}

void Lyrics::update(MediaState const& media, bool open) {
    if (stopped_) return;
    const bool trackChanged = appId_ != media.appId || title_ != media.title || artist_ != media.artist ||
                              album_ != media.album;
    const auto duration = std::isfinite(media.duration) ?
        static_cast<unsigned>(std::lround(std::clamp(media.duration, 0.0, 86'400.0))) : 0u;
    const bool durationChanged = (duration == 0) != (duration_ == 0) ||
        std::abs(static_cast<int>(duration) - static_cast<int>(duration_)) > 2;
    if (trackChanged || durationChanged) {
        appId_ = media.appId;
        title_ = media.title;
        artist_ = media.artist;
        album_ = media.album;
        duration_ = duration;
        cancel();
        view_.status = LyricsStatus::idle;
        view_.lines.clear();
    }
    if (!open && open_ && (view_.status == LyricsStatus::loading || view_.status == LyricsStatus::error)) {
        cancel();
        view_.status = LyricsStatus::idle;
    }
    open_ = open;
    if (open && view_.status == LyricsStatus::idle) {
        if (title_.empty() || artist_.empty() || duration_ == 0) {
            view_.status = LyricsStatus::unavailable;
        } else {
            auto path = L"/api/get?track_name=" + urlEncode(title_) + L"&artist_name=" + urlEncode(artist_);
            if (!album_.empty()) path += L"&album_name=" + urlEncode(album_);
            path += L"&duration=" + std::to_wstring(duration_);
            view_.status = LyricsStatus::loading;
            request_ = lookup(revision_, std::move(path));
        }
    }
}

Lyrics::Action Lyrics::lookup(unsigned revision, std::wstring path) {
    const auto lifetime = shared_from_this();
    const auto cancellation = co_await winrt::get_cancellation_token();
    cancellation.enable_propagation();
    const winrt::apartment_context ui;
    co_await winrt::resume_background();
    auto status = LyricsStatus::error;
    std::vector<LyricLine> lines;
    try {
        const auto http = client();
        const auto response = co_await http.GetAsync(Uri(L"https://lrclib.net" + path));
        if (response.StatusCode() == HttpStatusCode::NotFound) {
            status = LyricsStatus::unavailable;
        } else if (response.IsSuccessStatusCode()) {
            const auto body = jsonBody(co_await response.Content().ReadAsStringAsync());
            if (body) {
                lines = timedLines(field(body, L"syncedLyrics"));
                status = lines.empty() ? LyricsStatus::unavailable : LyricsStatus::ready;
            }
        }
    } catch (winrt::hresult_error const&) {
        // The view below distinguishes unavailable results from service errors.
    }
    co_await ui;
    if (stopped_ || revision != revision_) co_return;
    view_.status = status;
    view_.lines = std::move(lines);
    ++view_.revision;
    changed_();
}

}
