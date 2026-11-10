#include "lyrics.hpp"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.h>
#include <winrt/Windows.Web.Http.Filters.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <span>
#include <string_view>
#include <utility>

namespace zenith {
namespace {

using namespace std::chrono_literals;
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

bool transientNetworkError(winrt::hresult code) {
    using namespace winrt::Windows::Web;
    switch (WebError::GetStatus(code.value)) {
    case WebErrorStatus::ServerUnreachable:
    case WebErrorStatus::Timeout:
    case WebErrorStatus::ConnectionAborted:
    case WebErrorStatus::ConnectionReset:
    case WebErrorStatus::Disconnected:
    case WebErrorStatus::CannotConnect:
    case WebErrorStatus::HostNameNotResolved:
        return true;
    default:
        return false;
    }
}

std::optional<TimeSpan> retryDelay(HttpResponseMessage const& response, TimeSpan delay) {
    switch (static_cast<int>(response.StatusCode())) {
    case 408: case 429: case 500: case 502: case 503: case 504:
        break;
    default:
        return std::nullopt;
    }
    if (const auto retry = response.Headers().RetryAfter()) {
        if (const auto delta = retry.Delta()) delay = std::max(delay, delta.Value());
        if (const auto date = retry.Date()) delay = std::max(delay, date.Value() - winrt::clock::now());
    }
    if (delay > 8s) return std::nullopt;
    return delay;
}

IAsyncOperation<HttpResponseMessage> getWithRetry(HttpClient http, Uri uri) {
    const auto cancellation = co_await winrt::get_cancellation_token();
    cancellation.enable_propagation();
    constexpr TimeSpan backoff[]{1s, 3s};
    for (size_t attempt = 0; ; ++attempt) {
        if (cancellation()) throw winrt::hresult_canceled();
        HttpResponseMessage response{nullptr};
        const bool last = attempt == std::size(backoff);
        try {
            response = co_await http.GetAsync(uri);
        } catch (winrt::hresult_error const& error) {
            if (last || !transientNetworkError(error.code())) throw;
        }
        if (last) co_return response;
        const auto delay = response ? retryDelay(response, backoff[attempt]) : backoff[attempt];
        if (!delay) co_return response;
        response = nullptr;
        co_await winrt::resume_after(*delay);
    }
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

bool equalText(std::wstring_view first, std::wstring_view second) {
    if (first.empty() || second.empty()) return first.empty() && second.empty();
    return CompareStringOrdinal(first.data(), static_cast<int>(first.size()),
                                second.data(), static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
}

std::wstring cleanMetadata(std::wstring_view value, bool artist = false) {
    std::wstring result;
    for (auto ch : value) {
        if (ch == L'\u2019') ch = L'\'';
        if (ch == L'\u2013' || ch == L'\u2014') ch = L'-';
        if (ch == L'\t' || ch == L'\u00a0') ch = L' ';
        if (ch != L' ' || (!result.empty() && result.back() != L' ')) result += ch;
    }
    if (!result.empty() && result.back() == L' ') result.pop_back();
    constexpr std::wstring_view titleSuffixes[]{L" (Official Music Video)", L" [Official Music Video]",
        L" (Official Video)", L" [Official Video]", L" (Official Audio)", L" [Official Audio]"};
    constexpr std::wstring_view artistSuffixes[]{L" - Topic"};
    const std::span suffixes = artist ? std::span<const std::wstring_view>(artistSuffixes) :
                                      std::span<const std::wstring_view>(titleSuffixes);
    for (const auto suffix : suffixes) {
        if (result.size() > suffix.size() && equalText(std::wstring_view(result).substr(result.size() - suffix.size()), suffix)) {
            result.resize(result.size() - suffix.size());
            break;
        }
    }
    return result;
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
        if (const auto start = row.find_first_not_of(L"\ufeff \t"); start != std::wstring_view::npos) {
            row.remove_prefix(start);
        }
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

void findMatchingLyrics(JsonArray const& records, std::wstring_view title, std::wstring_view artist,
                        std::wstring_view album, unsigned duration, LyricsView& result) {
    struct Match {
        JsonObject record;
        double durationGap;
        bool otherAlbum;
    };
    std::vector<Match> matches;
    for (const auto& value : records) {
        if (value.ValueType() != JsonValueType::Object) continue;
        const auto record = value.GetObject();
        const auto length = record.GetNamedValue(L"duration", nullptr);
        if (!length || length.ValueType() != JsonValueType::Number) continue;
        const double gap = std::abs(length.GetNumber() - duration);
        if (!std::isfinite(gap) || gap > 4 || !equalText(cleanMetadata(field(record, L"trackName")), title) ||
            !equalText(cleanMetadata(field(record, L"artistName"), true), artist)) continue;
        matches.push_back({record, gap, !equalText(field(record, L"albumName"), album)});
    }
    std::ranges::sort(matches, {}, [](Match const& match) {
        return std::pair{match.durationGap, match.otherAlbum};
    });
    for (const auto& match : matches) {
        result.lines = timedLines(field(match.record, L"syncedLyrics"));
        if (!result.lines.empty()) return;
        if (result.plain.empty()) result.plain = field(match.record, L"plainLyrics");
    }
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

void Lyrics::retry() {
    if (open_ && view_.status == LyricsStatus::error) view_.status = LyricsStatus::idle;
}

void Lyrics::update(MediaState const& media, bool panelOpen, bool pillActive) {
    if (stopped_) return;
    const bool open = panelOpen || pillActive;
    const bool trackChanged = appId_ != media.appId || title_ != media.title || artist_ != media.artist ||
                              album_ != media.album;
    const auto duration = std::isfinite(media.duration) ?
        static_cast<unsigned>(std::lround(std::clamp(media.duration, 0.0, 86'400.0))) : 0u;
    const bool durationChanged = duration != 0 && (duration_ == 0 ||
        std::abs(static_cast<int>(duration) - static_cast<int>(duration_)) > 2);
    if (trackChanged || durationChanged) {
        appId_ = media.appId;
        title_ = media.title;
        artist_ = media.artist;
        album_ = media.album;
        duration_ = duration;
        cancel();
        view_.status = LyricsStatus::idle;
        view_.lines.clear();
        view_.plain.clear();
        retryAfter_ = {};
    }
    if (!open && open_ && (view_.status == LyricsStatus::loading || view_.status == LyricsStatus::error)) {
        cancel();
        view_.status = LyricsStatus::idle;
    }
    if (open && (!open_ || (panelOpen && !panelOpen_)) && view_.status == LyricsStatus::unavailable &&
        std::chrono::steady_clock::now() >= retryAfter_) view_.status = LyricsStatus::idle;
    if (panelOpen && !panelOpen_ && view_.status == LyricsStatus::error) view_.status = LyricsStatus::idle;
    panelOpen_ = panelOpen;
    open_ = open;
    if (open && view_.status == LyricsStatus::idle) {
        if (title_.empty() || artist_.empty() || duration_ == 0) {
            view_.status = LyricsStatus::unavailable;
        } else {
            view_.status = LyricsStatus::loading;
            request_ = lookup(revision_, title_, artist_, album_, duration_);
        }
    }
}

Lyrics::Action Lyrics::lookup(unsigned revision, std::wstring title, std::wstring artist, std::wstring album,
                              unsigned duration) {
    const auto lifetime = shared_from_this();
    const auto cancellation = co_await winrt::get_cancellation_token();
    cancellation.enable_propagation();
    const winrt::apartment_context ui;
    co_await winrt::resume_background();
    LyricsView result;
    result.status = LyricsStatus::error;
    try {
        const auto http = client();
        if (duration > 3600) {
            result.status = LyricsStatus::unavailable;
        } else {
            auto path = L"/api/get?track_name=" + urlEncode(title) + L"&artist_name=" + urlEncode(artist);
            if (!album.empty()) path += L"&album_name=" + urlEncode(album);
            path += L"&duration=" + std::to_wstring(duration);
            const auto response = co_await getWithRetry(http, Uri(L"https://lrclib.net" + path));
            if (response.StatusCode() == HttpStatusCode::NotFound) {
                result.status = LyricsStatus::unavailable;
            } else if (response.IsSuccessStatusCode()) {
                const auto body = jsonBody(co_await response.Content().ReadAsStringAsync());
                if (body) {
                    result.status = LyricsStatus::unavailable;
                    result.lines = timedLines(field(body, L"syncedLyrics"));
                    if (result.lines.empty()) result.plain = field(body, L"plainLyrics");
                }
            }
        }
        if (result.status == LyricsStatus::unavailable && result.lines.empty()) {
            auto searchTitle = cleanMetadata(title);
            const auto searchArtist = cleanMetadata(artist, true);
            const auto artistPrefix = searchArtist + L" - ";
            if (!searchArtist.empty() && searchTitle.size() > artistPrefix.size() &&
                equalText(std::wstring_view(searchTitle).substr(0, artistPrefix.size()), artistPrefix)) {
                searchTitle.erase(0, artistPrefix.size());
            }
            const auto search = co_await getWithRetry(http, Uri(L"https://lrclib.net/api/search?track_name=" +
                urlEncode(searchTitle) + L"&artist_name=" + urlEncode(searchArtist)));
            result.status = LyricsStatus::error;
            if (search.IsSuccessStatusCode()) {
                const auto body = co_await search.Content().ReadAsStringAsync();
                JsonArray records{nullptr};
                if (body.size() <= 1'048'576 && JsonArray::TryParse(body, records)) {
                    findMatchingLyrics(records, searchTitle, searchArtist, album, duration, result);
                    result.status = LyricsStatus::unavailable;
                }
            }
        }
    } catch (winrt::hresult_error const&) {
        result.status = LyricsStatus::error;
    }
    co_await ui;
    if (stopped_ || revision != revision_) co_return;
    if (!result.lines.empty()) {
        result.status = LyricsStatus::ready;
        result.plain.clear();
    } else if (result.plain.find_first_not_of(L" \t\r\n\ufeff") != std::wstring::npos) {
        result.status = LyricsStatus::plain;
    }
    result.revision = view_.revision + 1;
    view_ = std::move(result);
    if (view_.status == LyricsStatus::unavailable) retryAfter_ = std::chrono::steady_clock::now() + 30s;
    changed_();
}

}
