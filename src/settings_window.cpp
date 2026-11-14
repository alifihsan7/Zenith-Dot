#include "settings.hpp"
#include "browser_tabs.hpp"
#include "resource.h"
#include "notifications.hpp"
#include "prayer.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>
#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace zenith {
namespace {

constexpr wchar_t title[] = L"Zenula Settings";
constexpr wchar_t repository[] = L"https://github.com/alifihsan7/Zenula";

// Graphite from the island's shell and rows. Zenula blue only marks what is on or chosen.
constexpr COLORREF background = RGB(14, 15, 17);
constexpr COLORREF railColor = RGB(18, 19, 21);
constexpr COLORREF cardColor = RGB(26, 27, 30);
constexpr COLORREF hoverColor = RGB(34, 35, 39);
constexpr COLORREF pressColor = RGB(46, 47, 52);
constexpr COLORREF lineColor = RGB(38, 39, 43);
constexpr COLORREF chipColor = RGB(38, 50, 63);
constexpr COLORREF stageColor = RGB(8, 9, 10);
constexpr COLORREF ink = RGB(245, 246, 248);
constexpr COLORREF muted = RGB(164, 170, 180);   // 7.4:1 on a card
constexpr COLORREF quiet = RGB(128, 134, 143);   // 4.7:1 on a card: Off switch outline and disabled text
constexpr COLORREF faint = RGB(70, 72, 78);      // disabled arrow
constexpr COLORREF accent = RGB(145, 202, 255);
constexpr COLORREF albumSample = RGB(201, 129, 74);

constexpr int clientWidth = 860;
constexpr int clientHeight = 560;
constexpr int railWidth = 196;
constexpr int padX = 28;
constexpr int padTop = 22;
constexpr int rowHeight = 54;
constexpr int navTop = 64;
constexpr int navHeight = 36;

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
// The island's tiers: hover, toggles, then highlights, slides, and crossfades.
constexpr Clock::duration fastMotion = 120ms;
constexpr Clock::duration baseMotion = 180ms;
constexpr Clock::duration moveMotion = 260ms;
constexpr UINT_PTR motionTimer = 1;

// "JAWA BARAT" -> "Jawa Barat"; short all-caps words (DKI, DI) stay as acronyms.
std::wstring titleCase(std::wstring_view text) {
    std::wstring result(text);
    for (size_t start = 0; start < result.size();) {
        auto end = result.find(L' ', start);
        if (end == std::wstring::npos) end = result.size();
        if (end - start > 3) {
            for (size_t i = start + 1; i < end; ++i) result[i] = static_cast<wchar_t>(towlower(result[i]));
        }
        start = end + 1;
    }
    return result;
}

void notifyOverlay() {
    if (const auto overlay = FindWindowW(overlayWindowClass, nullptr)) {
        PostMessageW(overlay, settingsChangedMessage, 0, 0);
    }
}

struct Page {
    int id;
    wchar_t const* label;   // with its Alt mnemonic
    wchar_t const* name;
    wchar_t glyph;
};

constexpr Page pages[]{
    {IDC_SETTINGS_GENERAL, L"&General", L"General", 0xE713},
    {IDC_SETTINGS_ISLAND, L"&Island", L"Island", 0xE790},
    {IDC_SETTINGS_FEATURES, L"&Features", L"Features", 0xE71D},
    {IDC_SETTINGS_ALERTS, L"&Alerts && privacy", L"Alerts & privacy", 0xEA8F},
    {IDC_SETTINGS_PRAYER, L"&Prayer", L"Prayer", 0xE707},
};

size_t pageIndex(int id) {
    return static_cast<size_t>(std::ranges::find(pages, id, &Page::id) - std::begin(pages));
}

struct Toggle {
    int id;
    bool Settings::*field;
    bool (Settings::*save)() const;
};

constexpr Toggle toggles[]{
    {IDC_HIDE_FULLSCREEN, &Settings::hideInFullscreen, &Settings::saveDisplay},
    {IDC_DOT_ONLY, &Settings::dotOnly, &Settings::saveDisplay},
    {IDC_SHOW_WAVEFORM, &Settings::showWaveform, &Settings::saveIsland},
    {IDC_PILL_LYRICS, &Settings::showPillLyrics, &Settings::saveIsland},
    {IDC_AUTO_CYCLE, &Settings::autoCycle, &Settings::saveTimer},
    {IDC_ADD_COPIED_TEXT, &Settings::autoAddText, &Settings::saveClipboard},
    {IDC_ADD_SCREENSHOTS, &Settings::autoAddScreenshots, &Settings::saveClipboard},
    {IDC_CLIPBOARD_BUBBLE, &Settings::showClipboardBubble, &Settings::saveClipboard},
    {IDC_CLIPBOARD_PREVIEWS, &Settings::showClipboardPreviews, &Settings::saveClipboard},
    {IDC_CAMERA_INDICATOR, &Settings::showCameraIndicator, &Settings::savePrivacy},
    {IDC_MICROPHONE_INDICATOR, &Settings::showMicrophoneIndicator, &Settings::savePrivacy},
    {IDC_PRAYER_REMINDERS, &Settings::prayerReminders, &Settings::saveNotifications},
};

constexpr int featureCount = static_cast<int>(featureOrder.size());
bool isFeatureSwitch(int id) { return id >= IDC_ENABLE_MEDIA && id < IDC_ENABLE_MEDIA + featureCount; }
bool isFeatureRow(int id) { return id >= IDC_FEATURE_MEDIA && id < IDC_FEATURE_MEDIA + featureCount; }
bool isMoveUp(int id) { return id >= IDC_FEATURE_MOVE_UP && id < IDC_FEATURE_MOVE_UP + featureCount; }
bool isMoveDown(int id) { return id >= IDC_FEATURE_MOVE_DOWN && id < IDC_FEATURE_MOVE_DOWN + featureCount; }
bool isSwitch(int id) {
    return id == IDC_START_WITH_WINDOWS || id == IDC_FEATURE_ENABLED || isFeatureSwitch(id) ||
        std::ranges::find(toggles, id, &Toggle::id) != std::end(toggles);
}
int tryId(SettingsTest test) { return IDC_SETTINGS_TRY + static_cast<int>(test); }
bool isTry(int id) { return id > IDC_SETTINGS_TRY && validSettingsTest(static_cast<unsigned>(id - IDC_SETTINGS_TRY)); }

constexpr wchar_t const* featureNames[]{L"Now Playing", L"Timers", L"Scratchpad", L"Drop Zone", L"System"};
constexpr wchar_t const* widthChoices[]{L"Fit to title", L"147 px minimum"};
constexpr wchar_t const* sizeChoices[]{L"Compact", L"Default", L"Large"};
constexpr wchar_t const* accentChoices[]{L"Album artwork", L"Zenula blue", L"Custom"};
constexpr wchar_t const* pauseChoices[]{L"Now", L"3 s", L"5 s", L"Keep title"};
constexpr wchar_t const* focusChoices[]{L"15 min", L"25 min", L"45 min"};
constexpr wchar_t const* breakChoices[]{L"5 min", L"10 min", L"15 min"};
constexpr wchar_t const* quietChoices[]{L"Off", L"Follow Windows", L"On"};
constexpr wchar_t const* earlyChoices[]{L"On time", L"5 min", L"10 min", L"15 min"};
constexpr unsigned earlyMinutes[]{0, 5, 10, 15};
constexpr int choiceControls[]{IDC_PILL_WIDTH, IDC_ISLAND_SIZE, IDC_ACCENT_MODE, IDC_PAUSE_DELAY,
    IDC_FOCUS_DURATION, IDC_BREAK_DURATION, IDC_QUIET_MODE, IDC_PRAYER_EARLY};
constexpr int choicesPerRow = 4;

size_t choiceRow(int control) {
    return static_cast<size_t>(std::ranges::find(choiceControls, control) - std::begin(choiceControls));
}
int choiceButtonId(size_t row, size_t option) {
    return IDC_SETTINGS_CHOICE + static_cast<int>(row * choicesPerRow + option);
}
bool isChoiceButton(int id) {
    return id >= IDC_SETTINGS_CHOICE && id < choiceButtonId(std::size(choiceControls), 0);
}
int choiceRowId(int id) { return choiceControls[(id - IDC_SETTINGS_CHOICE) / choicesPerRow]; }
unsigned choiceIndex(int id) { return static_cast<unsigned>((id - IDC_SETTINGS_CHOICE) % choicesPerRow); }

std::span<wchar_t const* const> choices(int id) {
    switch (id) {
    case IDC_QUIET_MODE: return quietChoices;
    case IDC_PRAYER_EARLY: return earlyChoices;
    case IDC_PILL_WIDTH: return widthChoices;
    case IDC_ISLAND_SIZE: return sizeChoices;
    case IDC_ACCENT_MODE: return accentChoices;
    case IDC_PAUSE_DELAY: return pauseChoices;
    case IDC_FOCUS_DURATION: return focusChoices;
    case IDC_BREAK_DURATION: return breakChoices;
    default: return {};
    }
}

D2D1_RECT_F rectF(RECT value) {
    return D2D1::RectF(static_cast<float>(value.left), static_cast<float>(value.top),
                       static_cast<float>(value.right), static_cast<float>(value.bottom));
}

D2D1_COLOR_F color(COLORREF value) {
    return D2D1::ColorF(GetRValue(value) / 255.0f, GetGValue(value) / 255.0f, GetBValue(value) / 255.0f);
}

D2D1_COLOR_F mix(COLORREF from, COLORREF to, float amount) {
    const auto a = color(from);
    const auto b = color(to);
    return D2D1::ColorF(std::lerp(a.r, b.r, amount), std::lerp(a.g, b.g, amount), std::lerp(a.b, b.b, amount));
}

COLORREF hexColor(unsigned value) { return RGB(value >> 16, value >> 8, value); }

float progress(Clock::time_point start, Clock::duration duration, Clock::time_point now) {
    if (start == Clock::time_point{} || now - start >= duration) return 1;
    const float t = std::chrono::duration<float>(now - start) / std::chrono::duration<float>(duration);
    const float remaining = 1 - t;
    return 1 - remaining * remaining * remaining;
}

// One eased value. It only moves after a change, then rests at its target.
struct Motion {
    float from = 0;
    float to = 0;
    Clock::time_point start{};
    Clock::duration length = moveMotion;

    float value(Clock::time_point now) const { return std::lerp(from, to, progress(start, length, now)); }
    void set(float target, bool animate, Clock::time_point now) {
        from = animate ? value(now) : target;
        to = target;
        start = animate ? now : Clock::time_point{};
    }
};

enum class Font { heading, title, body, label, button, segment, nav, navActive, brand, icon, glyph, count };

class Canvas {
public:
    Canvas();
    void setDpi(UINT dpi);
    bool begin(HDC dc, RECT area, COLORREF clear);
    void end();
    float opacity() const { return opacity_; }
    void opacity(float value) { opacity_ = value; }
    float offset() const { return offset_; }
    void place(float dy);
    void zoom(float scale, D2D1_POINT_2F center);
    void clip(D2D1_RECT_F area) { target_->PushAxisAlignedClip(area, D2D1_ANTIALIAS_MODE_ALIASED); }
    void unclip() { target_->PopAxisAlignedClip(); }
    void fill(D2D1_RECT_F area, D2D1_COLOR_F value);
    void rounded(D2D1_RECT_F area, float radius, D2D1_COLOR_F value);
    void outline(D2D1_RECT_F area, float radius, D2D1_COLOR_F value, float width);
    void circle(D2D1_POINT_2F center, float radius, D2D1_COLOR_F value);
    void text(D2D1_RECT_F area, std::wstring_view value, Font font, D2D1_COLOR_F ink,
              DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING);
    float measure(std::wstring_view value, Font font) const;

private:
    D2D1_COLOR_F faded(D2D1_COLOR_F value) const { value.a *= opacity_; return value; }
    IDWriteTextFormat* format(Font font) const { return fonts_[static_cast<size_t>(font)].get(); }

    winrt::com_ptr<ID2D1Factory> graphics_;
    winrt::com_ptr<IDWriteFactory> writing_;
    winrt::com_ptr<ID2D1DCRenderTarget> target_;
    winrt::com_ptr<ID2D1SolidColorBrush> brush_;
    std::array<winrt::com_ptr<IDWriteTextFormat>, static_cast<size_t>(Font::count)> fonts_;
    std::wstring iconFamily_ = L"Segoe MDL2 Assets";
    float opacity_ = 1;
    float offset_ = 0;
};

Canvas::Canvas() {
    winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, graphics_.put()));
    winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                             reinterpret_cast<IUnknown**>(writing_.put())));
    winrt::com_ptr<IDWriteFontCollection> system;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(writing_->GetSystemFontCollection(system.put())) &&
        SUCCEEDED(system->FindFamilyName(L"Segoe Fluent Icons", &index, &exists)) && exists) {
        iconFamily_ = L"Segoe Fluent Icons";
    }
    setDpi(96);
}

void Canvas::setDpi(UINT dpi) {
    const auto create = [&](Font font, wchar_t const* family, float size, DWRITE_FONT_WEIGHT weight) {
        winrt::com_ptr<IDWriteTextFormat> next;
        if (FAILED(writing_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size * dpi / 96.0f, L"", next.put()))) return;
        next->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        next->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        winrt::com_ptr<IDWriteInlineObject> ellipsis;
        if (SUCCEEDED(writing_->CreateEllipsisTrimmingSign(next.get(), ellipsis.put()))) {
            const DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            next->SetTrimming(&trim, ellipsis.get());
        }
        fonts_[static_cast<size_t>(font)] = std::move(next);
    };
    constexpr auto normal = DWRITE_FONT_WEIGHT_NORMAL;
    constexpr auto strong = DWRITE_FONT_WEIGHT_SEMI_BOLD;
    create(Font::heading, L"Segoe UI", 22, strong);
    create(Font::title, L"Segoe UI", 13.5f, strong);
    create(Font::body, L"Segoe UI", 12, normal);
    create(Font::label, L"Segoe UI", 12, strong);
    create(Font::button, L"Segoe UI", 12.5f, normal);
    create(Font::segment, L"Segoe UI", 12.5f, strong);
    create(Font::nav, L"Segoe UI", 13, normal);
    create(Font::navActive, L"Segoe UI", 13, strong);
    create(Font::brand, L"Segoe UI", 14, strong);
    create(Font::icon, iconFamily_.c_str(), 16, normal);
    create(Font::glyph, iconFamily_.c_str(), 11, normal);
}

bool Canvas::begin(HDC dc, RECT area, COLORREF clear) {
    if (!target_) {
        // Software rendering: a hardware target loads the GPU driver, which alone costs tens of MB,
        // and this small window is copied to a GDI DC either way.
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96, 96);
        if (FAILED(graphics_->CreateDCRenderTarget(&properties, target_.put()))) return false;
        if (FAILED(target_->CreateSolidColorBrush(color(clear), brush_.put()))) {
            target_ = nullptr;
            return false;
        }
        target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
    if (FAILED(target_->BindDC(dc, &area))) return false;
    target_->BeginDraw();
    target_->Clear(color(clear));
    opacity_ = 1;
    offset_ = 0;
    return true;
}

void Canvas::end() {
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    if (FAILED(target_->EndDraw())) {
        brush_ = nullptr;
        target_ = nullptr;
    }
}

void Canvas::place(float dy) {
    offset_ = dy;
    target_->SetTransform(D2D1::Matrix3x2F::Translation(0, dy));
}

void Canvas::zoom(float scale, D2D1_POINT_2F center) {
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, center) * D2D1::Matrix3x2F::Translation(0, offset_));
}

void Canvas::fill(D2D1_RECT_F area, D2D1_COLOR_F value) {
    brush_->SetColor(faded(value));
    target_->FillRectangle(area, brush_.get());
}

void Canvas::rounded(D2D1_RECT_F area, float radius, D2D1_COLOR_F value) {
    brush_->SetColor(faded(value));
    target_->FillRoundedRectangle(D2D1::RoundedRect(area, radius, radius), brush_.get());
}

void Canvas::outline(D2D1_RECT_F area, float radius, D2D1_COLOR_F value, float width) {
    brush_->SetColor(faded(value));
    const float inset = width / 2;
    const auto inner = D2D1::RectF(area.left + inset, area.top + inset, area.right - inset, area.bottom - inset);
    target_->DrawRoundedRectangle(D2D1::RoundedRect(inner, radius - inset, radius - inset), brush_.get(), width);
}

void Canvas::circle(D2D1_POINT_2F center, float radius, D2D1_COLOR_F value) {
    brush_->SetColor(faded(value));
    target_->FillEllipse(D2D1::Ellipse(center, radius, radius), brush_.get());
}

void Canvas::text(D2D1_RECT_F area, std::wstring_view value, Font font, D2D1_COLOR_F paint, DWRITE_TEXT_ALIGNMENT align) {
    const auto format = this->format(font);
    if (!format || value.empty()) return;
    format->SetTextAlignment(align);
    brush_->SetColor(faded(paint));
    target_->DrawText(value.data(), static_cast<UINT32>(value.size()), format, area, brush_.get(),
                      D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

float Canvas::measure(std::wstring_view value, Font font) const {
    const auto format = this->format(font);
    winrt::com_ptr<IDWriteTextLayout> layout;
    if (!format || FAILED(writing_->CreateTextLayout(value.data(), static_cast<UINT32>(value.size()), format,
                                                     10000, 1000, layout.put()))) return 0;
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);
    return std::ceil(metrics.widthIncludingTrailingWhitespace);
}

// What the window draws for one page, top to bottom. Items with an id own an invisible
// button that gives the spot keyboard focus, Tab order, and a name for screen readers.
enum class Draw { heading, crumb, label, note, card, row, hit, toggle, track, choice, button, tryButton, icon,
                  chevron, preview, swatch, value };

struct Item {
    Draw draw;
    RECT rect{};
    RECT area{};             // row: its text; proxies: the clickable bounds
    std::wstring text;
    std::wstring name;       // accessible name of the proxy
    int id = 0;
    int click = 0;           // row: the control a click on the row operates
    bool enabled = true;
    bool first = false;      // row: first and last in its card
    bool last = false;
    int number = 0;          // row: feature position; track and choice: choice row
    wchar_t glyph = 0;
};

struct Part {
    Draw draw;
    int id = 0;
    std::wstring text;
    bool enabled = true;
    wchar_t glyph = 0;
    bool on = false;         // toggle state for its accessible name
};

struct Row {
    std::wstring title;
    std::wstring detail;
    bool enabled = true;
    int click = 0;
    int hit = 0;
    int number = 0;
    bool chevron = false;
    bool indent = false;
    int height = rowHeight;
};

class Builder {
public:
    Builder(Canvas const& canvas, UINT dpi, int left, int right) : canvas_(canvas), dpi_(dpi), left_(left),
        right_(right), y_(px(padTop)) {}

    int px(int value) const { return MulDiv(value, dpi_, 96); }
    std::vector<Item>& items() { return items_; }

    void heading(std::wstring text, Part const* crumb = nullptr, Part const* action = nullptr) {
        RECT area{left_, y_, right_, y_ + px(32)};
        if (crumb) {
            const int width = static_cast<int>(canvas_.measure(crumb->text, Font::heading)) + px(8);
            Item item{Draw::crumb, {area.left - px(4), area.top, area.left + width, area.bottom}};
            item.area = item.rect;
            item.text = crumb->text;
            item.name = L"Back to Features";
            item.id = crumb->id;
            items_.push_back(std::move(item));
            area.left += width + px(4);
        }
        if (action) {
            const auto size = this->size(*action);
            RECT button{area.right - size.cx, area.top + (px(32) - size.cy) / 2, area.right, 0};
            button.bottom = button.top + size.cy;
            items_.push_back({Draw::heading, area, area, std::move(text)});
            emit(*action, button, L"");
        } else {
            items_.push_back({Draw::heading, area, area, std::move(text)});
        }
        y_ += px(48);
        afterHeading_ = true;
    }

    void label(std::wstring text) {
        if (!afterHeading_) y_ += px(16);
        items_.push_back({Draw::label, {left_ + px(2), y_, right_, y_ + px(16)}, {}, std::move(text)});
        y_ += px(22);
        afterHeading_ = false;
    }

    void note(std::wstring text) {
        y_ += px(10);
        items_.push_back({Draw::note, {left_ + px(2), y_, right_, y_ + px(16)}, {}, std::move(text)});
        y_ += px(16);
        afterHeading_ = false;
    }

    void open() {
        card_ = items_.size();
        items_.push_back({Draw::card, {left_, y_, right_, y_}});
        firstRow_ = true;
        afterHeading_ = false;
    }

    void close() {
        items_[card_].rect.bottom = y_;
        for (auto i = items_.size(); i-- > card_;) {
            if (items_[i].draw == Draw::row) {
                items_[i].last = true;
                break;
            }
        }
    }

    void row(Row const& spec, std::vector<Part> parts = {}) {
        const RECT bounds{left_, y_, right_, y_ + px(spec.height)};
        const int middle = (bounds.top + bounds.bottom) / 2;
        int x = bounds.right - px(14);
        RECT chevron{};
        if (spec.chevron) {
            chevron = {x - px(12), bounds.top, x, bounds.bottom};
            x -= px(12) + px(10);
        }
        std::vector<RECT> places(parts.size());
        for (size_t i = parts.size(); i-- > 0;) {
            const auto size = this->size(parts[i]);
            places[i] = {x - size.cx, middle - size.cy / 2, x, middle - size.cy / 2 + size.cy};
            x -= size.cx + px(10);
        }
        int textLeft = bounds.left + px(spec.indent ? 32 : 14);
        if (spec.number) textLeft += px(26);
        Item item{Draw::row, bounds, {textLeft, bounds.top, x, bounds.bottom}, spec.title};
        item.name = spec.detail;
        item.click = spec.click;
        item.enabled = spec.enabled;
        item.first = firstRow_;
        item.number = spec.number;
        items_.push_back(item);
        firstRow_ = false;
        if (spec.hit) {
            Item hit{Draw::hit, bounds, {bounds.left, bounds.top, x, bounds.bottom}};
            hit.id = spec.hit;
            hit.name = spec.title + L", " + spec.detail;
            items_.push_back(std::move(hit));
        }
        for (size_t i = 0; i < parts.size(); ++i) emit(parts[i], places[i], spec.title);
        if (spec.chevron) items_.push_back({Draw::chevron, chevron});
        y_ = bounds.bottom;
    }

    void preview(Part const& action) {
        const RECT stage{left_, y_, right_, y_ + px(112)};
        items_.push_back({Draw::preview, stage});
        const auto size = this->size(action);
        emit(action, {stage.right - px(12) - size.cx, stage.bottom - px(12) - size.cy, stage.right - px(12),
                      stage.bottom - px(12)}, L"");
        y_ = stage.bottom;
        afterHeading_ = false;
    }

private:
    SIZE size(Part const& part) const {
        const auto measure = [&](std::wstring_view text, Font font) { return static_cast<int>(canvas_.measure(text, font)); };
        switch (part.draw) {
        case Draw::toggle: return {px(40), px(20)};
        case Draw::tryButton: return {measure(L"Try", Font::button) + px(16), px(28)};
        case Draw::button: return {measure(part.text, Font::button) + px(24), px(28)};
        case Draw::icon: return {px(26), px(26)};
        case Draw::swatch: return {px(16), px(16)};
        case Draw::value: return {measure(part.text, Font::body), px(16)};
        case Draw::track: {
            int width = px(6);
            for (const auto option : choices(part.id)) width += measure(option, Font::segment) + px(24);
            return {width, px(32)};
        }
        default: return {};
        }
    }

    void emit(Part const& part, RECT place, std::wstring const& owner) {
        Item item{part.draw, place, place, part.text};
        item.id = part.id;
        item.enabled = part.enabled;
        item.glyph = part.glyph;
        switch (part.draw) {
        case Draw::toggle:
            InflateRect(&item.area, px(4), px(10));
            item.name = owner + (part.on ? L", on" : L", off");
            break;
        case Draw::tryButton:
            item.name = L"Try " + owner;
            break;
        case Draw::track: {
            const auto row = choiceRow(part.id);
            item.number = static_cast<int>(row);
            item.id = 0;
            items_.push_back(item);
            int x = place.left + px(3);
            const auto values = choices(part.id);
            for (size_t i = 0; i < values.size(); ++i) {
                const int width = static_cast<int>(canvas_.measure(values[i], Font::segment)) + px(24);
                Item option{Draw::choice, {x, place.top + px(3), x + width, place.bottom - px(3)}};
                option.area = option.rect;
                option.text = values[i];
                option.name = owner + L": " + values[i];
                option.id = choiceButtonId(row, i);
                option.enabled = part.enabled;
                option.number = item.number;
                items_.push_back(std::move(option));
                x += width;
            }
            return;
        }
        default:
            item.name = part.text;
            break;
        }
        items_.push_back(std::move(item));
    }

    Canvas const& canvas_;
    UINT dpi_;
    int left_;
    int right_;
    int y_;
    size_t card_ = 0;
    bool firstRow_ = true;
    bool afterHeading_ = false;
    std::vector<Item> items_;
};

// Dark dialogs in the Settings style (defined further down).
bool ask(HWND owner, wchar_t const* heading, wchar_t const* body, wchar_t const* action);
void tell(HWND owner, wchar_t const* heading, wchar_t const* body);

struct View {
    int page = IDC_SETTINGS_GENERAL;
    int detail = 0;
    bool operator==(View const&) const = default;
};

class SettingsWindow {
public:
    static INT_PTR CALLBACK dialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

private:
    static LRESULT CALLBACK proxyProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                      UINT_PTR id, DWORD_PTR data);
    void initialize(HWND window);
    void setDpi(UINT dpi);
    void resize(RECT const* suggested);
    void sync();
    void relayout();
    std::vector<Item> layout(View view) const;
    void content(Builder& builder, View view) const;
    std::wstring summary(Feature feature) const;
    void show(View view);
    bool command(int id);
    bool commandChoice(int id);
    void toggle(int id);
    void move(Feature feature, int step, int via);
    void reset();
    void runTest(SettingsTest test);
    void completeNotificationAccess(NotificationAccess access);
    unsigned currentChoice(int id) const;
    bool switchOn(int id) const;
    Feature feature(int detail) const { return static_cast<Feature>(detail - IDC_FEATURE_MEDIA); }
    HWND proxy(int id) const;
    HWND makeProxy(int id, bool group);
    void pointer(std::optional<POINT> client);
    int rowAt(POINT client) const;
    void kick(Clock::duration length);
    float hot(int id, Clock::time_point now) const;
    bool focused(int id) const;
    void paint(HDC dc);
    void drawRail(Clock::time_point now);
    void drawView(std::vector<Item> const& items, bool live, Clock::time_point now);
    void drawItem(Item const& item, std::vector<Item> const& items, bool live, Clock::time_point now);
    void drawPreview(RECT stage, Clock::time_point now);
    void focusRing(RECT area, float radius);
    int px(int value) const { return MulDiv(value, dpi_, 96); }
    float pxf(float value) const { return value * dpi_ / 96.0f; }

    HWND window_{};
    HICON icon_{};
    Canvas canvas_;
    UINT dpi_ = 96;
    View view_{};
    std::vector<Item> leaving_;   // the page fading out, laid out once
    Settings settings_;
    bool animate_ = true;
    bool startupOn_ = false;
    std::wstring startup_;
    Feature lastMoved_ = Feature::media;
    std::vector<Item> items_;
    std::map<int, HWND> proxies_;
    std::map<int, Motion> switches_;
    std::array<Motion, std::size(choiceControls)> chips_{};
    Motion page_{1, 1};
    Motion nav_{};
    Motion scale_{1, 1};
    Motion width_{};
    int hot_ = 0;
    int hotFrom_ = 0;
    Clock::time_point hotStart_{};
    int pressedRow_ = 0;
    Clock::time_point motionUntil_{};
    winrt::Windows::Foundation::IAsyncAction notificationRequest_{nullptr};
    bool packaged_ = false;
};

void SettingsWindow::setDpi(UINT dpi) {
    dpi_ = dpi;
    canvas_.setDpi(dpi);
}

void SettingsWindow::resize(RECT const* suggested) {
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE));
    const auto extended = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_EXSTYLE));
    RECT frame{0, 0, px(clientWidth), px(clientHeight)};
    AdjustWindowRectExForDpi(&frame, style, FALSE, extended, dpi_);
    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;
    if (suggested) {
        SetWindowPos(window_, nullptr, suggested->left, suggested->top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor);
    const auto& work = monitor.rcWork;
    SetWindowPos(window_, nullptr, work.left + (work.right - work.left - width) / 2,
                 work.top + (work.bottom - work.top - height) / 2, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

HWND SettingsWindow::makeProxy(int id, bool group) {
    const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window_, GWLP_HINSTANCE));
    const auto button = CreateWindowExW(0, L"Button", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW | (group ? WS_GROUP : 0),
        0, 0, 0, 0, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    if (button) {
        SetWindowSubclass(button, proxyProc, 0, reinterpret_cast<DWORD_PTR>(this));
        proxies_[id] = button;
    }
    return button;
}

HWND SettingsWindow::proxy(int id) const {
    const auto found = proxies_.find(id);
    return found == proxies_.end() ? nullptr : found->second;
}

void SettingsWindow::initialize(HWND window) {
    window_ = window;
    packaged_ = hasPackageIdentity();
    setDpi(GetDpiForWindow(window));
    BOOL motion = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &motion, 0);
    animate_ = motion != FALSE;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    icon_ = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZENITH_DOT));
    if (icon_) SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon_));
    startup_ = startupShortcut();
    resize(nullptr);
    // The pages form one group, so the arrow keys move between them.
    for (const auto& page : pages) {
        const auto button = makeProxy(page.id, &page == pages);
        SetWindowTextW(button, page.label);
    }
    nav_.set(0, false, {});
    sync();
}

void SettingsWindow::sync() {
    settings_ = Settings::load();
    startupOn_ = !startup_.empty() && startupEnabled(startup_);
    const auto now = Clock::now();
    const auto settle = [&](Motion& motion, float target) {
        if (motion.to != target) motion.set(target, false, now);
    };
    for (const auto& item : toggles) settle(switches_[item.id], switchOn(item.id) ? 1.0f : 0.0f);
    for (int i = 0; i < featureCount; ++i) settle(switches_[IDC_ENABLE_MEDIA + i], switchOn(IDC_ENABLE_MEDIA + i));
    settle(switches_[IDC_START_WITH_WINDOWS], startupOn_);
    settle(switches_[IDC_FEATURE_ENABLED], switchOn(IDC_FEATURE_ENABLED));
    for (size_t row = 0; row < std::size(choiceControls); ++row)
        settle(chips_[row], static_cast<float>(currentChoice(choiceControls[row])));
    settle(scale_, settings_.islandScale());
    settle(width_, settings_.fixedPillMinimum ? 1.0f : 0.0f);
    relayout();
}

bool SettingsWindow::switchOn(int id) const {
    if (id == IDC_START_WITH_WINDOWS) return startupOn_;
    if (id == IDC_FEATURE_ENABLED) return view_.detail && settings_.enabled(feature(view_.detail));
    if (isFeatureSwitch(id)) return settings_.enabled(static_cast<Feature>(id - IDC_ENABLE_MEDIA));
    const auto toggle = std::ranges::find(toggles, id, &Toggle::id);
    return toggle != std::end(toggles) && settings_.*(toggle->field);
}

unsigned SettingsWindow::currentChoice(int id) const {
    switch (id) {
    case IDC_QUIET_MODE: return static_cast<unsigned>(settings_.quietMode);
    case IDC_PRAYER_EARLY: {
        // A lead set to some other value (an older build allowed any minute) highlights nothing.
        const auto match = std::ranges::find(earlyMinutes, settings_.prayerLeadMinutes);
        return match == std::end(earlyMinutes) ? static_cast<unsigned>(choicesPerRow)
                                               : static_cast<unsigned>(match - std::begin(earlyMinutes));
    }
    case IDC_PILL_WIDTH: return settings_.fixedPillMinimum;
    case IDC_ISLAND_SIZE: return settings_.islandSize;
    case IDC_ACCENT_MODE: return settings_.accentMode;
    case IDC_PAUSE_DELAY: return settings_.compactPause;
    case IDC_FOCUS_DURATION: return settings_.focusPreset;
    case IDC_BREAK_DURATION: return settings_.breakPreset;
    default: return 0;
    }
}

std::wstring SettingsWindow::summary(Feature feature) const {
    if (!settings_.enabled(feature)) return L"Hidden from the island";
    const auto onOff = [](bool value) { return value ? L" on" : L" off"; };
    switch (feature) {
    case Feature::media:
        return std::wstring(L"Visualizer") + onOff(settings_.showWaveform) + L" · Lyrics" + onOff(settings_.showPillLyrics) +
            L" · " + std::array{L"Compact right away", L"Compact after 3 s", L"Compact after 5 s", L"Keeps the title"}[settings_.compactPause];
    case Feature::timers:
        return L"Focus " + std::to_wstring(settings_.focusMinutes()) + L" min · Break " +
            std::to_wstring(settings_.breakMinutes()) + L" min" + (settings_.autoCycle ? L" · Cycles" : L"");
    case Feature::scratch: return L"Notes and Tasks";
    case Feature::drop:
        return std::wstring(L"Screenshots") + onOff(settings_.autoAddScreenshots) + L" · Copied text" + onOff(settings_.autoAddText);
    default: return L"Sound, brightness, and battery";
    }
}

void SettingsWindow::content(Builder& b, View view) const {
    const auto part = [&](Draw draw, int id, std::wstring text = {}, bool enabled = true) {
        return Part{draw, id, std::move(text), enabled, 0, isSwitch(id) && switchOn(id)};
    };
    const auto toggle = [&](int id, bool enabled = true) { return part(Draw::toggle, id, {}, enabled); };
    const auto tryIt = [&](SettingsTest test, bool enabled = true) { return part(Draw::tryButton, tryId(test), {}, enabled); };
    const auto choice = [&](int id, bool enabled = true) { return part(Draw::track, id, {}, enabled); };

    if (view.detail) {
        const auto which = feature(view.detail);
        const bool shown = settings_.enabled(which);
        const auto position = std::ranges::find(settings_.features, which) - settings_.features.begin();
        const Part crumb{Draw::crumb, IDC_FEATURE_BACK, L"Features ›"};
        b.heading(featureNames[static_cast<size_t>(which)], &crumb);
        b.open();
        b.row({.title = L"Show in the island", .detail = L"Position " + std::to_wstring(position + 1) + L" of " +
            std::to_wstring(settings_.features.size()) + L" in the feature bar.", .click = IDC_FEATURE_ENABLED},
            {toggle(IDC_FEATURE_ENABLED)});
        b.close();
        if (which == Feature::scratch || which == Feature::system) {
            b.note(std::wstring(featureNames[static_cast<size_t>(which)]) + L" has no other settings.");
            return;
        }
        if (!shown) {
            b.note(L"Turn it on to change its options.");
            return;
        }
        if (which == Feature::media) {
            b.label(L"Pill");
            b.open();
            b.row({.title = L"Spectrum visualizer", .detail = L"Show the player’s audio spectrum during playback.",
                   .click = IDC_SHOW_WAVEFORM}, {tryIt(SettingsTest::waveform), toggle(IDC_SHOW_WAVEFORM)});
            b.row({.title = L"Lyrics in the pill", .detail = L"Show the current line while the Lyrics view is active.",
                   .click = IDC_PILL_LYRICS}, {tryIt(SettingsTest::lyrics), toggle(IDC_PILL_LYRICS)});
            b.row({.title = L"Compact after pausing"}, {tryIt(SettingsTest::pause), choice(IDC_PAUSE_DELAY)});
            b.close();
        } else if (which == Feature::timers) {
            b.label(L"Defaults");
            b.open();
            b.row({.title = L"Focus length"}, {tryIt(SettingsTest::focus), choice(IDC_FOCUS_DURATION)});
            b.row({.title = L"Break length"}, {tryIt(SettingsTest::resting), choice(IDC_BREAK_DURATION)});
            b.row({.title = L"Cycle Focus and Break", .detail = L"Start the next phase automatically.",
                   .click = IDC_AUTO_CYCLE}, {tryIt(SettingsTest::cycle), toggle(IDC_AUTO_CYCLE)});
            b.close();
        } else {
            b.label(L"Clipboard");
            b.open();
            b.row({.title = L"Add copied text", .detail = L"Keep copied text in Drop Zone for this session.",
                   .click = IDC_ADD_COPIED_TEXT}, {tryIt(SettingsTest::copiedText), toggle(IDC_ADD_COPIED_TEXT)});
            b.row({.title = L"Add screenshots", .detail = L"Add screenshots and other clipboard images.",
                   .click = IDC_ADD_SCREENSHOTS}, {tryIt(SettingsTest::screenshot), toggle(IDC_ADD_SCREENSHOTS)});
            b.close();
        }
        return;
    }

    switch (view.page) {
    case IDC_SETTINGS_GENERAL: {
        const bool canStart = !startup_.empty();
        b.heading(L"General");
        b.label(L"Startup");
        b.open();
        b.row({.title = L"Start with Windows", .detail = canStart ? L"Launch Zenula when you sign in."
            : L"Not available for this copy of Zenula.", .enabled = canStart, .click = IDC_START_WITH_WINDOWS},
            {toggle(IDC_START_WITH_WINDOWS, canStart)});
        b.close();
        b.label(L"Behavior");
        b.open();
        b.row({.title = L"Dot only", .detail = L"Keep the island as a quiet dot while you work. Hover to open it.",
               .click = IDC_DOT_ONLY}, {toggle(IDC_DOT_ONLY)});
        b.row({.title = L"Hide in fullscreen", .detail = L"Hide the island over fullscreen apps and games.",
               .click = IDC_HIDE_FULLSCREEN}, {toggle(IDC_HIDE_FULLSCREEN)});
        b.close();
        b.label(L"Reset");
        b.open();
        b.row({.title = L"Reset settings", .detail = L"Restore the defaults. Your prayer location and startup stay."},
              {part(Draw::button, IDC_SETTINGS_RESET, L"Reset…")});
        b.close();
        b.label(L"About");
        b.open();
        b.row({.title = L"Zenula " ZENITH_VERSION_TEXT, .detail = L"Settings are stored for this Windows user only."},
              {part(Draw::button, IDC_SETTINGS_GITHUB, L"View on GitHub")});
        b.row({.detail = L"Lyrics from LRCLIB · Prayer times from SIHAT Kemenag RI", .height = 40});
        b.close();
        break;
    }
    case IDC_SETTINGS_ISLAND: {
        b.heading(L"Island");
        b.preview(part(Draw::button, tryId(SettingsTest::appearance), L"Preview in island"));
        b.label(L"Shape");
        b.open();
        b.row({.title = L"Size"}, {choice(IDC_ISLAND_SIZE)});
        b.row({.title = L"Pill width", .detail = L"Short titles keep a steady pill."}, {choice(IDC_PILL_WIDTH)});
        b.close();
        b.label(L"Color");
        b.open();
        b.row({.title = L"Accent"}, {choice(IDC_ACCENT_MODE)});
        if (settings_.accentMode == 2) {
            wchar_t hex[16]{};
            swprintf_s(hex, L"#%06X", settings_.customAccent);
            b.row({.title = L"Custom color", .indent = true},
                  {part(Draw::swatch, 0), part(Draw::value, 0, hex), part(Draw::button, IDC_CUSTOM_ACCENT, L"Change…")});
        }
        b.close();
        break;
    }
    case IDC_SETTINGS_FEATURES: {
        const auto action = part(Draw::button, tryId(SettingsTest::order), L"Preview order");
        b.heading(L"Features", nullptr, &action);
        b.open();
        for (size_t index = 0; index < settings_.features.size(); ++index) {
            const auto which = static_cast<int>(settings_.features[index]);
            const std::wstring name = featureNames[which];
            auto up = part(Draw::icon, IDC_FEATURE_MOVE_UP + which, L"Move " + name + L" up", index > 0);
            up.glyph = 0xE70E;
            auto down = part(Draw::icon, IDC_FEATURE_MOVE_DOWN + which, L"Move " + name + L" down",
                             index + 1 < settings_.features.size());
            down.glyph = 0xE70D;
            b.row({.title = name, .detail = summary(static_cast<Feature>(which)), .click = IDC_FEATURE_MEDIA + which,
                   .hit = IDC_FEATURE_MEDIA + which, .number = static_cast<int>(index) + 1, .chevron = true},
                  {up, down, toggle(IDC_ENABLE_MEDIA + which)});
        }
        b.close();
        b.note(L"The island’s feature bar follows this order.");
        break;
    }
    case IDC_SETTINGS_ALERTS: {
        const bool bubble = settings_.showClipboardBubble;
        b.heading(L"Alerts & privacy");
        b.open();
        b.row({.title = L"Do Not Disturb", .detail = L"Silences alerts. Camera and mic status stay visible."},
              {choice(IDC_QUIET_MODE)});
        b.close();
        b.label(L"Clipboard");
        b.open();
        b.row({.title = L"Copied bubble", .detail = L"Show a bubble when you copy something.", .click = IDC_CLIPBOARD_BUBBLE},
              {tryIt(SettingsTest::copied), toggle(IDC_CLIPBOARD_BUBBLE)});
        b.row({.title = L"Text preview", .detail = L"Include a short snippet in the bubble.", .enabled = bubble,
               .click = IDC_CLIPBOARD_PREVIEWS, .indent = true}, {toggle(IDC_CLIPBOARD_PREVIEWS, bubble)});
        b.close();
        b.label(L"Indicators and notifications");
        b.open();
        b.row({.title = L"Camera in use", .detail = L"Show when an app is using the camera.", .click = IDC_CAMERA_INDICATOR},
              {tryIt(SettingsTest::camera), toggle(IDC_CAMERA_INDICATOR)});
        b.row({.title = L"Microphone in use", .detail = L"Show when an app is using the microphone.",
               .click = IDC_MICROPHONE_INDICATOR}, {tryIt(SettingsTest::microphone), toggle(IDC_MICROPHONE_INDICATOR)});
        std::vector<Part> parts;
        if (packaged_) parts.push_back(part(Draw::button, IDC_APP_NOTIFICATIONS,
            settings_.showAppNotifications ? L"Turn off" : L"Connect", !notificationRequest_));
        parts.push_back(tryIt(SettingsTest::notification));
        b.row({.title = L"Windows app notifications", .detail = packaged_
            ? L"Show the app and title of incoming notifications." : L"Available in the MSIX build. Try shows a sample alert."},
            std::move(parts));
        b.close();
        break;
    }
    default: {
        const auto city = settings_.prayerCity;
        b.heading(L"Prayer");
        b.open();
        if (city) {
            const auto& place = prayerLocations[city - 1];
            b.row({.title = place.name, .detail = titleCase(place.province) + L" · " + place.zone()},
                  {part(Draw::button, IDC_PRAYER_CITY, L"Change location…")});
        } else {
            b.row({.title = L"Prayer times are off", .detail = L"Choose a location to see prayer times in the island."},
                  {part(Draw::button, IDC_PRAYER_CITY, L"Choose location…")});
        }
        b.close();
        b.label(L"Reminders");
        b.open();
        b.row({.title = L"Prayer reminders", .detail = L"Show a bubble when it’s time to pray.", .enabled = city != 0,
               .click = IDC_PRAYER_REMINDERS}, {toggle(IDC_PRAYER_REMINDERS, city != 0)});
        const bool early = city && settings_.prayerReminders;
        b.row({.title = L"Remind me", .enabled = early}, {choice(IDC_PRAYER_EARLY, early)});
        b.close();
        b.note(L"Times come from SIHAT Kemenag RI for " + std::to_wstring(prayerLocations.size()) + L" regencies and cities.");
        break;
    }
    }
}

std::vector<Item> SettingsWindow::layout(View view) const {
    RECT client{};
    GetClientRect(window_, &client);
    Builder builder(canvas_, dpi_, px(railWidth + padX), client.right - px(padX));
    content(builder, view);
    return std::move(builder.items());
}

void SettingsWindow::relayout() {
    items_ = layout(view_);
    const auto focus = GetFocus();
    std::vector<HWND> shown;
    // Nav first, then the page in reading order: the z-order is the Tab order.
    HWND after = nullptr;
    for (size_t i = 0; i < std::size(pages); ++i) {
        const auto button = proxy(pages[i].id);
        const int top = px(navTop + navHeight * static_cast<int>(i));
        // Only the open page is a Tab stop; the arrow keys move between pages.
        const auto style = GetWindowLongPtrW(button, GWL_STYLE);
        const auto wanted = pages[i].id == view_.page ? style | WS_TABSTOP : style & ~static_cast<LONG_PTR>(WS_TABSTOP);
        if (wanted != style) SetWindowLongPtrW(button, GWL_STYLE, wanted);
        SetWindowPos(button, after ? after : HWND_TOP, px(10), top, px(railWidth - 20), px(navHeight),
                     SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_SHOWWINDOW);
        shown.push_back(button);
        after = button;
    }
    for (const auto& item : items_) {
        if (!item.id) continue;
        auto button = proxy(item.id);
        if (!button) {
            const bool group = item.draw != Draw::choice || choiceIndex(item.id) == 0;
            button = makeProxy(item.id, group);
            if (!button) continue;
        }
        SetWindowPos(button, after, item.area.left, item.area.top, item.area.right - item.area.left,
                     item.area.bottom - item.area.top, SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_SHOWWINDOW);
        std::wstring name = item.name;
        if (item.draw == Draw::choice && choiceIndex(item.id) == currentChoice(choiceRowId(item.id))) name += L", selected";
        // A literal & would turn the next letter into a mnemonic.
        for (size_t at = 0; (at = name.find(L'&', at)) != std::wstring::npos; at += 2) name.insert(at, 1, L'&');
        wchar_t current[160]{};
        GetWindowTextW(button, current, ARRAYSIZE(current));
        if (name != current) SetWindowTextW(button, name.c_str());
        EnableWindow(button, item.enabled);
        shown.push_back(button);
        after = button;
    }
    for (const auto& entry : proxies_) {
        if (std::ranges::find(shown, entry.second) == shown.end()) ShowWindow(entry.second, SW_HIDE);
    }
    if (focus && GetParent(focus) == window_ && (!IsWindowVisible(focus) || !IsWindowEnabled(focus))) {
        SetFocus(proxy(view_.page));
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void SettingsWindow::kick(Clock::duration length) {
    InvalidateRect(window_, nullptr, FALSE);
    if (!animate_) return;
    motionUntil_ = std::max(motionUntil_, Clock::now() + length);
    SetTimer(window_, motionTimer, 16, nullptr);
}

void SettingsWindow::show(View view) {
    if (view == view_) return;
    const auto now = Clock::now();
    leaving_ = animate_ ? std::move(items_) : std::vector<Item>{};
    view_ = view;
    page_ = {0, 1, animate_ ? now : Clock::time_point{}, moveMotion};
    nav_.set(static_cast<float>(pageIndex(view.page)), animate_, now);
    switches_[IDC_FEATURE_ENABLED].set(switchOn(IDC_FEATURE_ENABLED), false, now);
    relayout();
    kick(moveMotion);
}

void SettingsWindow::runTest(SettingsTest test) {
    const auto target = view_.detail ? feature(view_.detail) : test == SettingsTest::order ? lastMoved_ : Feature::media;
    if (const auto overlay = FindWindowW(overlayWindowClass, nullptr)) {
        DWORD_PTR accepted = 0;
        if (SendMessageTimeoutW(overlay, settingsTestMessage, static_cast<WPARAM>(test), static_cast<LPARAM>(target),
                                SMTO_ABORTIFHUNG, 1500, &accepted) && accepted) return;
        tell(window_, L"Restart Zenula to try this", L"The Zenula that’s running is an older build without this test.");
        return;
    }
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    const auto args = L"--test=" + std::to_wstring(static_cast<unsigned>(test)) + L"," +
        std::to_wstring(static_cast<unsigned>(target));
    if (!length || length >= executable.size() ||
        reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", executable.c_str(), args.c_str(),
                                               nullptr, SW_SHOWNOACTIVATE)) <= 32) {
        tell(window_, L"Couldn’t run the test", L"Zenula didn’t start. Try again.");
    }
}

bool SettingsWindow::commandChoice(int id) {
    if (id == IDC_CUSTOM_ACCENT) {
        COLORREF swatches[16]{};
        CHOOSECOLORW picker{sizeof(picker)};
        picker.hwndOwner = window_;
        picker.rgbResult = hexColor(settings_.customAccent);
        picker.lpCustColors = swatches;
        picker.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (!ChooseColorW(&picker)) return true;
        auto updated = Settings::load();
        updated.customAccent = (GetRValue(picker.rgbResult) << 16) | (GetGValue(picker.rgbResult) << 8) |
            GetBValue(picker.rgbResult);
        if (updated.saveAccent()) notifyOverlay();
        else tell(window_, L"Couldn’t save this color", L"The change wasn’t written for this Windows user. Try again.");
        sync();
        return true;
    }
    if (!isChoiceButton(id)) return false;
    const auto index = choiceIndex(id);
    id = choiceRowId(id);
    if (index >= choices(id).size() || index == currentChoice(id)) return true;
    auto updated = Settings::load();
    switch (id) {
    case IDC_QUIET_MODE: updated.quietMode = static_cast<QuietMode>(index); break;
    case IDC_PRAYER_EARLY: updated.prayerLeadMinutes = earlyMinutes[index]; break;
    case IDC_PILL_WIDTH: updated.fixedPillMinimum = index != 0; break;
    case IDC_ISLAND_SIZE: updated.islandSize = index; break;
    case IDC_ACCENT_MODE: updated.accentMode = index; break;
    case IDC_PAUSE_DELAY: updated.compactPause = index; break;
    case IDC_FOCUS_DURATION: updated.focusPreset = index; break;
    case IDC_BREAK_DURATION: updated.breakPreset = index; break;
    }
    const bool saved = id == IDC_QUIET_MODE || id == IDC_PRAYER_EARLY
        ? updated.saveNotifications() : updated.saveIsland();
    if (!saved) {
        tell(window_, L"Couldn’t save this setting", L"The change wasn’t written for this Windows user. Try again.");
        return true;
    }
    const auto now = Clock::now();
    auto& chip = chips_[choiceRow(id)];
    // With nothing chosen before, the chip appears in place instead of sliding in from nowhere.
    const bool slide = animate_ && chip.to < static_cast<float>(choices(id).size());
    chip.set(static_cast<float>(index), slide, now);
    settings_ = updated;
    scale_.set(settings_.islandScale(), animate_, now);
    width_.set(settings_.fixedPillMinimum ? 1.0f : 0.0f, animate_, now);
    relayout();
    kick(moveMotion);
    notifyOverlay();
    return true;
}

void SettingsWindow::completeNotificationAccess(NotificationAccess access) {
    notificationRequest_ = nullptr;
    if (access != NotificationAccess::Allowed) {
        relayout();
        tell(window_, L"Notifications are blocked",
             L"Allow Zenula in Windows Settings under Privacy & security > Notifications.");
        return;
    }
    auto updated = Settings::load();
    updated.showAppNotifications = true;
    if (updated.saveNotifications()) notifyOverlay();
    else tell(window_, L"Couldn’t save this setting", L"The change wasn’t written for this Windows user. Try again.");
    sync();
}

void SettingsWindow::toggle(int id) {
    const bool next = !switchOn(id);
    bool saved = false;
    if (id == IDC_START_WITH_WINDOWS) {
        saved = setStartupEnabled(startup_, next);
        if (saved) startupOn_ = next;
    } else {
        auto updated = Settings::load();
        if (id == IDC_FEATURE_ENABLED || isFeatureSwitch(id)) {
            const auto which = id == IDC_FEATURE_ENABLED ? feature(view_.detail) : static_cast<Feature>(id - IDC_ENABLE_MEDIA);
            const auto bit = 1u << static_cast<unsigned>(which);
            if (next) updated.enabledFeatures |= bit;
            else updated.enabledFeatures &= ~bit;
            saved = updated.saveFeatures();
        } else {
            const auto& item = *std::ranges::find(toggles, id, &Toggle::id);
            updated.*(item.field) = next;
            saved = (updated.*(item.save))();
        }
        if (saved) settings_ = updated;
    }
    if (!saved) {
        sync();
        tell(window_, L"Couldn’t save this setting", L"The change wasn’t written for this Windows user. Try again.");
        return;
    }
    switches_[id].set(next, animate_, Clock::now());
    if (id != IDC_START_WITH_WINDOWS) notifyOverlay();
    relayout();
    kick(baseMotion);
}

void SettingsWindow::move(Feature which, int step, int via) {
    auto updated = Settings::load();
    const auto current = std::ranges::find(updated.features, which) - updated.features.begin();
    const auto next = current + step;
    if (next < 0 || next >= static_cast<ptrdiff_t>(updated.features.size())) return;
    std::swap(updated.features[current], updated.features[next]);
    if (!updated.saveFeatures()) {
        tell(window_, L"Couldn’t save the feature order", L"The change wasn’t written for this Windows user. Try again.");
        sync();
        return;
    }
    settings_ = updated;
    lastMoved_ = which;
    notifyOverlay();
    relayout();
    // At the top or bottom the arrow turns off, so focus stays on the row being moved.
    if (const auto button = proxy(via); GetFocus() == button && !IsWindowEnabled(button))
        SetFocus(proxy(IDC_FEATURE_MEDIA + static_cast<int>(which)));
}

// A dark Cancel / action question in the Settings style. Focus starts on Cancel,
// and Enter presses whichever button has focus.
struct Question {
    wchar_t const* heading;
    wchar_t const* body;
    wchar_t const* action = nullptr;   // nullptr: a notice with one OK button
    HBRUSH fill = CreateSolidBrush(background);
    HFONT strong{};
    HFONT normal{};

    ~Question() {
        DeleteObject(fill);
        if (strong) DeleteObject(strong);
        if (normal) DeleteObject(normal);
    }
};

void roundedGdi(HDC dc, RECT area, int radius, COLORREF value) {
    const auto brush = SelectObject(dc, GetStockObject(DC_BRUSH));
    const auto pen = SelectObject(dc, GetStockObject(NULL_PEN));
    SetDCBrushColor(dc, value);
    RoundRect(dc, area.left, area.top, area.right + 1, area.bottom + 1, radius, radius);
    SelectObject(dc, pen);
    SelectObject(dc, brush);
}

LRESULT CALLBACK enterPresses(HWND button, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR) {
    if (message == WM_GETDLGCODE && lparam && wparam == VK_RETURN) return DLGC_BUTTON | DLGC_WANTMESSAGE;
    if (message == WM_KEYDOWN && wparam == VK_RETURN) {
        SendMessageW(GetParent(button), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(button), BN_CLICKED),
                     reinterpret_cast<LPARAM>(button));
        return 0;
    }
    if (message == WM_CHAR && wparam == VK_RETURN) return 0;
    if (message == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
    }
    // Hover repaints the button once on enter and once on leave; nothing runs in between.
    if (message == WM_MOUSEMOVE && !GetPropW(button, L"Zenula.Hot")) {
        SetPropW(button, L"Zenula.Hot", reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT leave{sizeof(leave), TME_LEAVE, button, 0};
        TrackMouseEvent(&leave);
        InvalidateRect(button, nullptr, FALSE);
    }
    if (message == WM_MOUSELEAVE) {
        RemovePropW(button, L"Zenula.Hot");
        InvalidateRect(button, nullptr, FALSE);
    }
    if (message == WM_NCDESTROY) {
        RemovePropW(button, L"Zenula.Hot");
        RemoveWindowSubclass(button, enterPresses, id);
    }
    return DefSubclassProc(button, message, wparam, lparam);
}

INT_PTR CALLBACK questionProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* question = reinterpret_cast<Question*>(GetWindowLongPtrW(window, DWLP_USER));
    const auto scale = [&](int value) { return MulDiv(value, GetDpiForWindow(window), 96); };
    switch (message) {
    case WM_INITDIALOG: {
        question = reinterpret_cast<Question*>(lparam);
        SetWindowLongPtrW(window, DWLP_USER, lparam);
        const BOOL dark = TRUE;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        const auto font = [&](int size, int weight) {
            return CreateFontW(-scale(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        };
        question->strong = font(15, FW_SEMIBOLD);
        question->normal = font(13, FW_NORMAL);
        SetDlgItemTextW(window, IDC_CONFIRM_TITLE, question->heading);
        SetDlgItemTextW(window, IDC_CONFIRM_BODY, question->body);
        SendDlgItemMessageW(window, IDC_CONFIRM_TITLE, WM_SETFONT, reinterpret_cast<WPARAM>(question->strong), FALSE);
        SendDlgItemMessageW(window, IDC_CONFIRM_BODY, WM_SETFONT, reinterpret_cast<WPARAM>(question->normal), FALSE);
        for (const int id : {IDCANCEL, IDOK}) SetWindowSubclass(GetDlgItem(window, id), enterPresses, 0, 0);
        if (!question->action) ShowWindow(GetDlgItem(window, IDCANCEL), SW_HIDE);
        SetFocus(GetDlgItem(window, question->action ? IDCANCEL : IDOK));
        return FALSE;
    }
    case WM_CTLCOLORDLG:
        if (question) return reinterpret_cast<INT_PTR>(question->fill);
        break;
    case WM_CTLCOLORSTATIC:
        if (question) {
            const auto dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, GetDlgCtrlID(reinterpret_cast<HWND>(lparam)) == IDC_CONFIRM_TITLE ? ink : muted);
            SetBkColor(dc, background);
            return reinterpret_cast<INT_PTR>(question->fill);
        }
        break;
    case WM_DRAWITEM: {
        if (!question) break;
        const auto& item = *reinterpret_cast<DRAWITEMSTRUCT const*>(lparam);
        FillRect(item.hDC, &item.rcItem, question->fill);
        const bool action = item.CtlID == IDOK;
        const bool pressed = (item.itemState & ODS_SELECTED) != 0;
        const bool focused = (item.itemState & ODS_FOCUS) != 0 && !(item.itemState & ODS_NOFOCUSRECT);
        auto face = item.rcItem;
        // The focus ring sits outside a 3 px gap, so it also reads on the blue button.
        if (focused) {
            roundedGdi(item.hDC, face, scale(14), action ? ink : accent);
            InflateRect(&face, -scale(3), -scale(3));
            roundedGdi(item.hDC, face, scale(10), background);
            InflateRect(&face, -scale(1), -scale(1));
        }
        const bool hot = GetPropW(item.hwndItem, L"Zenula.Hot") != nullptr;
        const COLORREF fill = action ? (pressed ? RGB(119, 170, 220) : hot ? RGB(178, 218, 255) : accent)
                                     : (pressed ? RGB(58, 59, 65) : hot ? pressColor : hoverColor);
        roundedGdi(item.hDC, face, scale(10), fill);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, action ? background : ink);
        const auto previous = SelectObject(item.hDC, question->normal);
        RECT label = item.rcItem;
        DrawTextW(item.hDC, !action ? L"Cancel" : question->action ? question->action : L"OK", -1, &label,
                  DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        SelectObject(item.hDC, previous);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL) {
            EndDialog(window, LOWORD(wparam) == IDOK);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(window, 0);
        return TRUE;
    }
    return FALSE;
}

bool ask(HWND owner, wchar_t const* heading, wchar_t const* body, wchar_t const* action) {
    Question question{heading, body, action};
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CONFIRM), owner, questionProc,
                           reinterpret_cast<LPARAM>(&question)) == 1;
}

void tell(HWND owner, wchar_t const* heading, wchar_t const* body) {
    ask(owner, heading, body, nullptr);
}

void SettingsWindow::reset() {
    if (!ask(window_, L"Reset all settings?",
             L"Your prayer location, Start with Windows, and the Windows notification connection stay as they are. "
             L"This can’t be undone.", L"Reset")) return;
    const auto current = Settings::load();
    Settings defaults;
    defaults.prayerCity = current.prayerCity;
    defaults.showAppNotifications = current.showAppNotifications;
    bool saved = true;
    for (const auto save : {&Settings::saveClipboard, &Settings::savePrivacy, &Settings::saveTimer, &Settings::saveDisplay,
                            &Settings::saveIsland, &Settings::saveFeatures, &Settings::saveAccent, &Settings::saveNotifications})
        saved = (defaults.*save)() && saved;
    notifyOverlay();
    sync();
    if (!saved) tell(window_, L"Couldn’t reset every setting", L"Some settings kept their old values. Try again.");
}

bool SettingsWindow::command(int id) {
    if (std::ranges::find(pages, id, &Page::id) != std::end(pages)) {
        show({id, 0});
        return true;
    }
    if (isFeatureRow(id)) {
        show({IDC_SETTINGS_FEATURES, id});
        SetFocus(proxy(IDC_FEATURE_BACK));
        return true;
    }
    if (id == IDC_FEATURE_BACK) {
        const auto previous = view_.detail;
        show({IDC_SETTINGS_FEATURES, 0});
        if (previous) SetFocus(proxy(previous));
        return true;
    }
    if (isMoveUp(id)) {
        move(static_cast<Feature>(id - IDC_FEATURE_MOVE_UP), -1, id);
        return true;
    }
    if (isMoveDown(id)) {
        move(static_cast<Feature>(id - IDC_FEATURE_MOVE_DOWN), 1, id);
        return true;
    }
    if (isTry(id)) {
        runTest(static_cast<SettingsTest>(id - IDC_SETTINGS_TRY));
        return true;
    }
    if (id == IDC_PRAYER_CITY) {
        const auto city = choosePrayerLocation(window_, settings_.prayerCity);
        if (city && *city != settings_.prayerCity) {
            auto updated = Settings::load();
            updated.prayerCity = *city;
            if (updated.saveNotifications()) notifyOverlay();
            else tell(window_, L"Couldn’t save the prayer location", L"The change wasn’t written for this Windows user. Try again.");
            sync();
        }
        return true;
    }
    if (id == IDC_APP_NOTIFICATIONS && packaged_ && !notificationRequest_) {
        if (!settings_.showAppNotifications) {
            notificationRequest_ = requestNotificationAccess(window_);
            relayout();
        } else {
            auto updated = Settings::load();
            updated.showAppNotifications = false;
            if (updated.saveNotifications()) notifyOverlay();
            else tell(window_, L"Couldn’t save this setting", L"The change wasn’t written for this Windows user. Try again.");
            sync();
        }
        return true;
    }
    if (id == IDC_SETTINGS_GITHUB) {
        ShellExecuteW(window_, L"open", repository, nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }
    if (id == IDC_SETTINGS_RESET) {
        reset();
        return true;
    }
    if (commandChoice(id)) return true;
    if (isSwitch(id)) {
        toggle(id);
        return true;
    }
    return false;
}

int SettingsWindow::rowAt(POINT client) const {
    for (const auto& item : items_) {
        if (item.draw == Draw::row && item.click && item.enabled && PtInRect(&item.rect, client)) {
            const auto target = proxy(item.click);
            return target && IsWindowEnabled(target) ? item.click : 0;
        }
    }
    return 0;
}

void SettingsWindow::pointer(std::optional<POINT> client) {
    int next = 0;
    if (client) {
        const auto child = ChildWindowFromPointEx(window_, *client, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
        if (child && child != window_) next = GetDlgCtrlID(child);
        // A switch lights its whole row, so the row and its switch share one hover.
        if (!next || isSwitch(next)) {
            if (const auto row = rowAt(*client)) next = row;
        }
    }
    if (next == hot_) return;
    hotFrom_ = hot_;
    hot_ = next;
    hotStart_ = animate_ ? Clock::now() : Clock::time_point{};
    kick(fastMotion);
}

float SettingsWindow::hot(int id, Clock::time_point now) const {
    if (!id) return 0;
    const float p = progress(hotStart_, fastMotion, now);
    return id == hot_ ? p : id == hotFrom_ ? 1 - p : 0;
}

bool SettingsWindow::focused(int id) const {
    const auto button = proxy(id);
    if (!button || GetFocus() != button) return false;
    return (SendMessageW(window_, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0;
}

void SettingsWindow::focusRing(RECT area, float radius) {
    InflateRect(&area, px(2), px(2));
    canvas_.outline(rectF(area), radius + pxf(2), color(accent), pxf(2));
}

void SettingsWindow::paint(HDC dc) {
    RECT client{};
    GetClientRect(window_, &client);
    if (!canvas_.begin(dc, client, background)) return;
    const auto now = Clock::now();
    drawRail(now);
    const D2D1_RECT_F content = D2D1::RectF(pxf(railWidth), 0, static_cast<float>(client.right), static_cast<float>(client.bottom));
    canvas_.clip(content);
    const float p = page_.value(now);
    // The page that leaves drifts up as it fades; the next one rises 6 px into place.
    if (p < 1) {
        canvas_.opacity(1 - p);
        canvas_.place(-pxf(6) * p);
        drawView(leaving_, false, now);
    }
    canvas_.opacity(p);
    canvas_.place(pxf(6) * (1 - p));
    drawView(items_, true, now);
    canvas_.opacity(1);
    canvas_.place(0);
    canvas_.unclip();
    canvas_.end();
    if (icon_) DrawIconEx(dc, px(20), px(18), icon_, px(28), px(28), 0, nullptr, DI_NORMAL);
}

void SettingsWindow::drawRail(Clock::time_point now) {
    RECT client{};
    GetClientRect(window_, &client);
    canvas_.fill(D2D1::RectF(0, 0, pxf(railWidth), static_cast<float>(client.bottom)), color(railColor));
    canvas_.text(D2D1::RectF(pxf(58), pxf(18), pxf(railWidth - 10), pxf(46)), L"Zenula", Font::brand, color(ink));
    const float top = pxf(navTop) + nav_.value(now) * pxf(navHeight);
    canvas_.rounded(D2D1::RectF(pxf(10), top, pxf(railWidth - 10), top + pxf(navHeight)), pxf(6), color(hoverColor));
    for (size_t i = 0; i < std::size(pages); ++i) {
        const auto& page = pages[i];
        const bool current = page.id == view_.page;
        const RECT area{px(10), px(navTop + navHeight * static_cast<int>(i)), px(railWidth - 10),
                        px(navTop + navHeight * static_cast<int>(i + 1))};
        const float lit = current ? 1 : hot(page.id, now);
        if (!current && lit > 0) canvas_.rounded(rectF(area), pxf(6), mix(railColor, cardColor, lit));
        const wchar_t glyph[]{page.glyph, 0};
        canvas_.text(D2D1::RectF(static_cast<float>(area.left) + pxf(10), static_cast<float>(area.top),
                                 static_cast<float>(area.left) + pxf(30), static_cast<float>(area.bottom)),
                     glyph, Font::icon, current ? color(accent) : mix(muted, ink, lit), DWRITE_TEXT_ALIGNMENT_CENTER);
        canvas_.text(D2D1::RectF(static_cast<float>(area.left) + pxf(42), static_cast<float>(area.top),
                                 static_cast<float>(area.right), static_cast<float>(area.bottom)),
                     page.name, current ? Font::navActive : Font::nav, mix(muted, ink, lit));
        if (focused(page.id)) focusRing(area, pxf(6));
    }
    canvas_.text(D2D1::RectF(pxf(20), static_cast<float>(client.bottom) - pxf(40), pxf(railWidth - 10),
                             static_cast<float>(client.bottom) - pxf(18)),
                 L"Version " ZENITH_VERSION_TEXT, Font::body, color(quiet));
}

void SettingsWindow::drawView(std::vector<Item> const& items, bool live, Clock::time_point now) {
    for (const auto& item : items) drawItem(item, items, live, now);
}

void SettingsWindow::drawItem(Item const& item, std::vector<Item> const& items, bool live, Clock::time_point now) {
    const auto r = rectF(item.rect);
    const float lit = live ? hot(item.id, now) : 0;
    const auto button = live ? proxy(item.id) : nullptr;
    const bool pressed = button && (SendMessageW(button, BM_GETSTATE, 0, 0) & BST_PUSHED);
    switch (item.draw) {
    case Draw::heading:
        canvas_.text(r, item.text, Font::heading, color(ink));
        break;
    case Draw::crumb:
        canvas_.text(D2D1::RectF(r.left + pxf(4), r.top, r.right, r.bottom), item.text, Font::heading, mix(muted, ink, lit));
        break;
    case Draw::label:
        canvas_.text(r, item.text, Font::label, color(muted));
        break;
    case Draw::note:
        canvas_.text(r, item.text, Font::body, color(muted));
        break;
    case Draw::card:
        canvas_.rounded(r, pxf(10), color(cardColor));
        break;
    case Draw::row: {
        const float rowLit = live && item.enabled ? hot(item.click, now) : 0;
        if (rowLit > 0) {
            const auto fill = mix(cardColor, hoverColor, rowLit);
            const float mid = (r.top + r.bottom) / 2;
            if (item.first || item.last) canvas_.rounded(r, pxf(10), fill);
            if (!item.first) canvas_.fill(D2D1::RectF(r.left, r.top, r.right, item.last ? mid : r.bottom), fill);
            if (!item.last) canvas_.fill(D2D1::RectF(r.left, item.first ? mid : r.top, r.right, r.bottom), fill);
        }
        if (!item.first) canvas_.fill(D2D1::RectF(r.left, r.top, r.right, r.top + pxf(1)), color(lineColor));
        auto text = rectF(item.area);
        if (item.number) {
            canvas_.text(D2D1::RectF(text.left - pxf(26), r.top, text.left, r.bottom), std::to_wstring(item.number),
                         Font::body, color(muted));
        }
        const auto titleInk = color(item.enabled ? ink : quiet);
        const auto detailInk = color(item.enabled ? muted : quiet);
        if (item.text.empty()) {
            canvas_.text(text, item.name, Font::body, detailInk);
        } else if (item.name.empty()) {
            canvas_.text(text, item.text, Font::title, titleInk);
        } else {
            const float mid = (r.top + r.bottom) / 2;
            canvas_.text(D2D1::RectF(text.left, r.top + pxf(7), text.right, mid + pxf(1)), item.text, Font::title, titleInk);
            canvas_.text(D2D1::RectF(text.left, mid, text.right, r.bottom - pxf(7)), item.name, Font::body, detailInk);
        }
        break;
    }
    case Draw::hit: {
        if (live && focused(item.id)) {
            RECT ring = item.rect;
            InflateRect(&ring, -px(2), -px(2));
            focusRing(ring, pxf(8));
        }
        break;
    }
    case Draw::toggle: {
        const auto found = switches_.find(item.id);
        const float on = found != switches_.end() ? found->second.value(now) : (switchOn(item.id) ? 1.0f : 0.0f);
        const float saved = canvas_.opacity();
        if (!item.enabled) canvas_.opacity(saved * 0.45f);
        if (on > 0) canvas_.rounded(r, (r.bottom - r.top) / 2, mix(cardColor, accent, on));
        canvas_.outline(r, (r.bottom - r.top) / 2, mix(quiet, accent, on), pxf(1.5f));
        const float radius = pxf(6);
        const float x = std::lerp(r.left + pxf(3) + radius, r.right - pxf(3) - radius, on);
        canvas_.circle(D2D1::Point2F(x, (r.top + r.bottom) / 2), radius, mix(muted, background, on));
        canvas_.opacity(saved);
        if (live && focused(item.id)) focusRing(item.rect, (r.bottom - r.top) / 2);
        break;
    }
    case Draw::track: {
        canvas_.rounded(r, pxf(8), color(background));
        std::vector<D2D1_RECT_F> options;
        for (const auto& option : items)
            if (option.draw == Draw::choice && option.number == item.number) options.push_back(rectF(option.rect));
        const float at = chips_[static_cast<size_t>(item.number)].value(now);
        if (options.empty() || at < 0 || at > static_cast<float>(options.size() - 1)) break;
        const auto low = static_cast<size_t>(std::floor(at));
        const auto high = std::min(low + 1, options.size() - 1);
        const float t = at - static_cast<float>(low);
        const auto chip = D2D1::RectF(std::lerp(options[low].left, options[high].left, t), options[low].top,
                                      std::lerp(options[low].right, options[high].right, t), options[low].bottom);
        canvas_.rounded(chip, pxf(6), color(item.enabled ? chipColor : cardColor));
        break;
    }
    case Draw::choice: {
        const float at = chips_[static_cast<size_t>(item.number)].value(now);
        const float chosen = std::max(0.0f, 1 - std::abs(at - static_cast<float>(choiceIndex(item.id))));
        if (item.enabled && lit * (1 - chosen) > 0) canvas_.rounded(r, pxf(6), mix(background, cardColor, lit * (1 - chosen)));
        const auto label = item.enabled ? mix(ink, accent, chosen) : color(quiet);
        canvas_.text(r, item.text, Font::segment, label, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (live && focused(item.id)) focusRing(item.rect, pxf(6));
        break;
    }
    case Draw::button: {
        const auto face = pressed ? color(pressColor) : mix(hoverColor, pressColor, lit * 0.6f);
        canvas_.rounded(r, pxf(6), item.enabled ? face : color(cardColor));
        canvas_.text(r, item.text, Font::button, color(item.enabled ? ink : quiet), DWRITE_TEXT_ALIGNMENT_CENTER);
        if (live && focused(item.id)) focusRing(item.rect, pxf(6));
        break;
    }
    case Draw::tryButton: {
        if (item.enabled && (lit > 0 || pressed)) canvas_.rounded(r, pxf(6), pressed ? color(pressColor) : mix(cardColor, hoverColor, lit));
        canvas_.text(r, L"Try", Font::button, color(item.enabled ? accent : quiet), DWRITE_TEXT_ALIGNMENT_CENTER);
        if (live && focused(item.id)) focusRing(item.rect, pxf(6));
        break;
    }
    case Draw::icon: {
        if (item.enabled && (lit > 0 || pressed)) canvas_.rounded(r, pxf(6), pressed ? color(pressColor) : mix(cardColor, hoverColor, lit));
        const wchar_t glyph[]{item.glyph, 0};
        canvas_.text(r, glyph, Font::glyph, item.enabled ? mix(muted, ink, lit) : color(faint), DWRITE_TEXT_ALIGNMENT_CENTER);
        if (live && focused(item.id)) focusRing(item.rect, pxf(6));
        break;
    }
    case Draw::chevron:
        canvas_.text(r, L"\xE76C", Font::glyph, color(muted), DWRITE_TEXT_ALIGNMENT_CENTER);
        break;
    case Draw::preview:
        drawPreview(item.rect, now);
        break;
    case Draw::swatch:
        canvas_.rounded(r, pxf(4), color(hexColor(settings_.customAccent)));
        break;
    case Draw::value:
        canvas_.text(r, item.text, Font::body, color(muted));
        break;
    }
}

// A still picture of the pill at the chosen size, width, and accent. It only redraws on a change.
void SettingsWindow::drawPreview(RECT stage, Clock::time_point now) {
    const auto area = rectF(stage);
    canvas_.rounded(area, pxf(10), color(stageColor));
    const auto art = settings_.accentMode == 0 ? albumSample : settings_.accentMode == 1 ? accent
        : hexColor(settings_.customAccent);
    constexpr wchar_t song[] = L"Now Playing";
    const float textWidth = canvas_.measure(song, Font::label);
    const float natural = pxf(6 + 22 + 8) + textWidth + pxf(8 + 14 + 12);
    const float width = std::lerp(natural, std::max(natural, pxf(147)), width_.value(now));
    const auto center = D2D1::Point2F((area.left + area.right) / 2, (area.top + area.bottom) / 2 - pxf(4));
    const auto pill = D2D1::RectF(center.x - width / 2, center.y - pxf(16), center.x + width / 2, center.y + pxf(16));
    canvas_.zoom(scale_.value(now), center);
    canvas_.rounded(pill, pxf(16), D2D1::ColorF(0, 0, 0));
    canvas_.outline(pill, pxf(16), color(RGB(29, 30, 33)), pxf(1));
    const auto cover = D2D1::RectF(pill.left + pxf(6), center.y - pxf(11), pill.left + pxf(28), center.y + pxf(11));
    canvas_.rounded(cover, pxf(6), color(art));
    canvas_.text(D2D1::RectF(cover.right + pxf(8), pill.top, cover.right + pxf(8) + textWidth + pxf(2), pill.bottom),
                 song, Font::label, color(ink));
    constexpr float bars[]{5, 10, 7, 12};
    float x = pill.right - pxf(12) - pxf(14);
    for (const float bar : bars) {
        canvas_.rounded(D2D1::RectF(x, center.y + pxf(6) - pxf(bar), x + pxf(2), center.y + pxf(6)), pxf(1), color(art));
        x += pxf(4);
    }
    canvas_.place(canvas_.offset());
}

LRESULT CALLBACK SettingsWindow::proxyProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                           UINT_PTR id, DWORD_PTR data) {
    auto& self = *reinterpret_cast<SettingsWindow*>(data);
    switch (message) {
    case WM_PAINT: {
        // The window draws every control itself; this button only holds focus and input.
        ValidateRect(window, nullptr);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETFOCUS: {
        // Like tabs: arrowing onto a page opens it.
        const int control = GetDlgCtrlID(window);
        const bool arrow = (GetKeyState(VK_UP) | GetKeyState(VK_DOWN) | GetKeyState(VK_LEFT) | GetKeyState(VK_RIGHT)) & 0x8000;
        if (arrow && std::ranges::find(pages, control, &Page::id) != std::end(pages) && control != self.view_.page)
            PostMessageW(self.window_, WM_COMMAND, MAKEWPARAM(control, BN_CLICKED), reinterpret_cast<LPARAM>(window));
        break;
    }
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_GETDLGCODE:
        // Enter presses the focused control. Plain DLGC_BUTTON keeps the dialog from restyling it.
        if (lparam && wparam == VK_RETURN) {
            const auto msg = reinterpret_cast<MSG const*>(lparam);
            if (msg->message == WM_KEYDOWN || msg->message == WM_CHAR) return DLGC_BUTTON | DLGC_WANTMESSAGE;
        }
        return DLGC_BUTTON;
    case WM_KEYDOWN:
        if (wparam == VK_RETURN) {
            if ((lparam & (1 << 30)) == 0)
                SendMessageW(GetParent(window), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(window), BN_CLICKED),
                             reinterpret_cast<LPARAM>(window));
            return 0;
        }
        break;
    case WM_CHAR:
        if (wparam == VK_RETURN) return 0;
        break;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT leave{sizeof(leave), TME_LEAVE, window, 0};
        TrackMouseEvent(&leave);
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        MapWindowPoints(window, self.window_, &point, 1);
        self.pointer(point);
        break;
    }
    case WM_MOUSELEAVE: {
        POINT point{};
        GetCursorPos(&point);
        const auto under = WindowFromPoint(point);
        ScreenToClient(self.window_, &point);
        if (under == self.window_ || GetParent(under) == self.window_) self.pointer(point);
        else self.pointer(std::nullopt);
        break;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(window, proxyProc, id);
        break;
    }
    const auto result = DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
        message == WM_KEYDOWN || message == WM_KEYUP || message == BM_SETSTATE || message == WM_ENABLE ||
        message == WM_CAPTURECHANGED)
        InvalidateRect(self.window_, nullptr, FALSE);
    return result;
}

INT_PTR CALLBACK SettingsWindow::dialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrW(window, DWLP_USER, lparam);
        reinterpret_cast<SettingsWindow*>(lparam)->initialize(window);
        return TRUE;
    }
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, DWLP_USER));
    if (!self) return FALSE;
    switch (message) {
    case notificationAccessMessage:
        self->completeNotificationAccess(static_cast<NotificationAccess>(wparam));
        return TRUE;
    case WM_DESTROY:
        if (self->notificationRequest_) self->notificationRequest_.Cancel();
        break;
    case WM_CLOSE:
        EndDialog(window, 0);
        return TRUE;
    case WM_ACTIVATE:
        if (LOWORD(wparam) != WA_INACTIVE) self->sync();
        break;
    case WM_DPICHANGED:
        self->setDpi(HIWORD(wparam));
        self->resize(reinterpret_cast<RECT const*>(lparam));
        self->relayout();
        return TRUE;
    case WM_SETTINGCHANGE: {
        BOOL motion = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &motion, 0);
        self->animate_ = motion != FALSE;
        break;
    }
    case WM_UPDATEUISTATE:
        InvalidateRect(window, nullptr, FALSE);
        break;
    case WM_TIMER:
        if (wparam == motionTimer) {
            InvalidateRect(window, nullptr, FALSE);
            if (Clock::now() >= self->motionUntil_) {
                KillTimer(window, motionTimer);
                self->leaving_ = {};
            }
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT leave{sizeof(leave), TME_LEAVE, window, 0};
        TrackMouseEvent(&leave);
        self->pointer(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return TRUE;
    }
    case WM_MOUSELEAVE: {
        POINT point{};
        GetCursorPos(&point);
        const auto under = WindowFromPoint(point);
        ScreenToClient(window, &point);
        if (under == window || GetParent(under) == window) self->pointer(point);
        else self->pointer(std::nullopt);
        return TRUE;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        self->pressedRow_ = self->rowAt({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return TRUE;
    case WM_LBUTTONUP: {
        const auto row = self->rowAt({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        if (row && row == self->pressedRow_) self->command(row);
        self->pressedRow_ = 0;
        return TRUE;
    }
    case WM_SETCURSOR:
        if (reinterpret_cast<HWND>(wparam) == window && LOWORD(lparam) == HTCLIENT) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(window, &point);
            SetCursor(LoadCursorW(nullptr, self->rowAt(point) ? IDC_HAND : IDC_ARROW));
            SetWindowLongPtrW(window, DWLP_MSGRESULT, TRUE);
            return TRUE;
        }
        break;
    case WM_ERASEBKGND:
        SetWindowLongPtrW(window, DWLP_MSGRESULT, 1);
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        self->paint(BeginPaint(window, &ps));
        EndPaint(window, &ps);
        return TRUE;
    }
    case WM_DRAWITEM:
        // Owner-draw buttons ask to repaint on focus and press; the window draws them instead.
        InvalidateRect(window, nullptr, FALSE);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL) {
            EndDialog(window, 0);
            return TRUE;
        }
        if (HIWORD(wparam) == BN_CLICKED) return self->command(LOWORD(wparam));
        break;
    }
    return FALSE;
}

}

int runSettingsWindow(HINSTANCE instance) {
    const auto mutex = CreateMutexW(nullptr, FALSE, L"Local\\ZenithDot.Settings");
    const auto error = GetLastError();
    if (!mutex) return 1;
    winrt::handle singleInstance{mutex};
    if (error == ERROR_ALREADY_EXISTS) {
        if (const auto existing = FindWindowW(L"#32770", title)) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return 0;
    }
    SettingsWindow settings;
    const auto result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, SettingsWindow::dialog,
                                       reinterpret_cast<LPARAM>(&settings));
    if (result == -1) winrt::throw_last_error();
    return static_cast<int>(result);
}

}
