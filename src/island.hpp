#pragma once

#include "desktop.hpp"
#include "media.hpp"
#include "lyrics.hpp"
#include "scratchpad.hpp"
#include "countdown.hpp"
#include "drop_zone.hpp"
#include "power.hpp"
#include "prayer.hpp"
#include "system.hpp"
#include "settings.hpp"

#include <d2d1.h>
#include <dwrite.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zenith {

class SettingsExample;

// Something a click on the island asks the app to do.
struct IslandCommand {
    enum class Kind { none, previous, toggle, next, seek, selectSource, openSource, retryLyrics, adjustLyricsDelay, editScratchpad,
                      audioPicker, notificationSettings, removeDrop, clearDrop, openDropImage, appVolume, appMute,
                      autoCycle };
    Kind kind = Kind::none;
    double seconds = 0;              // seek position; appVolume: level 0..1; appMute, autoCycle: 1 = on
    std::wstring appId;
    size_t index = 0;
};

// A short notice that splits off the pill as a liquid bubble (or shows inside the open panel).
struct IslandAlert {
    enum class Kind { copy, charging, battery, lowBattery, focusDone, breakDone, timerDone,
                      microphone, camera, capsLock, numLock, bluetooth, bluetoothAudio, prayer, notification, connection };
    Kind kind = Kind::copy;
    std::wstring title;
    std::wstring value;
    std::optional<unsigned> color;   // icon colour (0xRRGGBB); the accent when empty
    int priority = 1;                // a waiting alert is replaced only by one of equal or higher priority
    std::chrono::milliseconds hold{1800};
    std::optional<unsigned> battery = std::nullopt;   // drawn as a battery cell before value
};

// The island UI from preview V7: an idle dot, a pill while something is going on, and a panel on hover.
// Layout is in DIPs in "stage" space: x = 0 is the monitor's center line, y = 0 the work area's top.
class Island {
public:
    using Clock = std::chrono::steady_clock;

    Island() = default;
    ~Island();
    Island(Island const&) = delete;
    Island& operator=(Island const&) = delete;

    void setAnimate(bool animate);
    void setPreferences(Settings const& settings);
    void setMedia(MediaState const& media);
    void setPower(PowerState const& power);
    void setSystem(SystemStats const& stats);
    void setNetwork(NetworkStatus const& status);
    void setPrayerSchedule(std::optional<PrayerSchedule> const& schedule);
    void setAudioOutput(std::wstring const& output);
    void setAppVolumes(std::vector<AppVolume> apps);
    void setAutoCycle(bool enabled);
    // A command produced while dragging (an app volume slider), to apply before the button is released.
    std::optional<IslandCommand> takeLiveCommand();
    bool alertsQuiet() const;
    bool setWindowsNotificationsQuiet(std::optional<bool> quiet);
    void setSuppressed(bool suppressed);
    void setCaptureColors(std::optional<unsigned> microphone, std::optional<unsigned> camera);
    void setLyrics(LyricsView const& lyrics);
    void setScratchpad(ScratchpadView const& scratchpad);
    void setScratchEditing(bool editing);
    void setDropItems(std::vector<DropPreview> items);
    void setDropHover(bool hovering);
    void setPopupOpen(bool open);
    void showDropZone(bool hold = false);
    std::optional<size_t> dropItemAt(POINT client) const;
    void closePanel();
    bool panelPinned() const;
    std::optional<RECT> scratchEditorRect() const;
    bool wantsLyrics() const;
    bool wantsLyricPill() const;
    void notify(IslandAlert alert);

    // Runs time-based behavior: hover delays, closing, and timers. Call before draw.
    void tick();
    void draw(HWND window);
    // Milliseconds until the next frame is needed, or 0 while nothing moves.
    unsigned frameInterval() const;
    bool visible() const;
    bool wantsSystemStats() const;
    bool wantsPrayerClock() const;

    // Pointer input in window client pixels. The bool results say whether a redraw is needed.
    bool pointerMove(POINT client);
    bool pointerLeave();
    bool scroll(POINT client, int delta);
    bool pointerDown(POINT client);   // true: capture the mouse until pointerUp
    IslandCommand pointerUp(POINT client);
    void pointerCancel();
    bool overControl() const;

private:
    friend class SettingsExample;
    enum class Hit { none, pill, previous, toggle, next, timeline, cover, sourcePicker, sourceMenu, source, navMedia, navTimers,
                     navScratch, navSystem, navPrayer, focusToggle, focusReset, timerView, focusAdjust, timerPreset,
                     timerToggle, timerReset, quickAdjust, lyrics, lyricLine, retryLyrics, lyricsDelay, scratchEdit,
                     navDrop, dropItem, removeDrop, clearDrop, dropImage, panelPin, audioPicker, notificationSettings,
                     appVolume, appMute, autoCycle };
    using Screen = Feature;
    Settings preferences_;
    std::vector<Screen> screens_{featureOrder.begin(), featureOrder.end()};
    enum class PillMode { dot, media, focus, timer, preview, privacy };
    struct Target {
        D2D1_RECT_F rect;
        Hit hit;
        size_t index;
    };
    struct Image {
        unsigned width = 0;
        unsigned height = 0;
        std::vector<uint8_t> pixels;
        winrt::com_ptr<ID2D1Bitmap> bitmap;
    };
    struct PillText {
        std::wstring text;
        winrt::com_ptr<IDWriteTextLayout> layout;
        D2D1_SIZE_F textSize{};
    };

    float pillAmount(Clock::time_point now) const;
    struct MotionSample {
        float value;
        float velocity;
    };
    enum class Spring { shape, split };
    static MotionSample sampleSpring(Clock::time_point start, Clock::duration duration, Clock::time_point now,
                                     float from, float to, float velocity = 0, Spring spring = Spring::shape);
    MotionSample pillMotion(Clock::time_point now) const;
    float pillTransitionProgress(Clock::time_point now) const;
    bool pillLyricGrowing() const;
    float pillLyricOut(float progress) const;
    float pillLyricIn(float progress) const;
    float pillTitleOpacity(Clock::time_point now) const;
    D2D1_SIZE_F pillSize(Clock::time_point now) const;
    float captureWidth(Clock::time_point now) const;
    float captureProgress(Clock::time_point now, float from, bool active) const;
    D2D1_RECT_F compactShape(Clock::time_point now, float amount) const;
    std::optional<D2D1_RECT_F> mediaPrivacyShape(Clock::time_point now, D2D1_RECT_F anchor) const;
    float visibility(Clock::time_point now) const;
    float panelValue(Clock::time_point now) const;
    MotionSample panelMotion(Clock::time_point now) const;
    float panelHeight(Clock::time_point now) const;
    float panelWidthAt(Clock::time_point now) const;
    float lyricScroll(Clock::time_point now) const;
    bool scrollLyrics(int delta);
    void moveLyricScroll(float offset, Clock::time_point now);
    void updateLyricVisibility(bool wasVisible, Clock::time_point now);
    float screenHeight(Screen screen) const;
    D2D1_RECT_F panelRect(Clock::time_point now) const;
    bool moving(Clock::time_point now) const;
    void updatePill(Clock::time_point now);
    void updatePillLayout(Clock::time_point now, float workWidth);
    void updateTheme(Clock::time_point now);
    bool alertAllowed(IslandAlert::Kind kind) const;
    void setOpen(bool open, Clock::time_point now);
    void showScreen(Screen screen, Clock::time_point now);
    void toggleFocus(Clock::time_point now);
    void selectTimerView(size_t index);
    void selectFocusPhase(bool resting);
    void adjustFocus(int direction);
    int phaseMinutes(bool resting) const;
    std::wstring phaseName(bool resting) const;
    void finishFocus(Clock::time_point now);
    D2D1_POINT_2F stagePoint(POINT client) const;
    std::optional<Target> targetAt(D2D1_POINT_2F point) const;
    float timelineFraction(D2D1_POINT_2F point) const;
    std::wstring title() const;
    void startAlert(IslandAlert alert, Clock::time_point now, bool update);
    void finishAlert(Clock::time_point now);
    void clearAlerts();
    float alertValue(Clock::time_point now) const;
    MotionSample alertMotion(Clock::time_point now) const;
    bool bubbleShowing() const;
    float inlineInset() const;
    float measure(std::wstring_view value, IDWriteTextFormat* format);

    void prepareSurface(int width, int height, bool animating);
    void prepareText();
    void releaseDeviceResources();
    void drawCompactContent(Clock::time_point now, D2D1_RECT_F shape, bool hideTitle, bool hideArt = false);
    void drawLyricsGlyph(D2D1_POINT_2F origin, D2D1_COLOR_F color);
    void drawPillContent(Clock::time_point now, float width, bool hideTitle);
    void drawIsland(Clock::time_point now, float value, float amount, bool morphTitle);
    void drawScreen(Screen screen, D2D1_RECT_F panel, Clock::time_point now, float artOpacity, bool hideTitle, bool record);
    void drawMedia(D2D1_RECT_F panel, Clock::time_point now, float artOpacity, bool hideTitle, bool record);
    void drawSourceIcon(MediaSource const& source, D2D1_POINT_2F center);
    void drawSourceMenu(D2D1_RECT_F panel);
    void drawLyrics(D2D1_RECT_F panel, Clock::time_point now, double position);
    void prepareLyricLayout(float width);
    void drawTimers(D2D1_RECT_F panel, Clock::time_point now, bool record);
    void drawScratchpad(D2D1_RECT_F panel, bool record);
    void drawDropZone(D2D1_RECT_F panel, bool record);
    void drawCaptureIndicators(D2D1_POINT_2F right, float mic, float camera);
    void drawCenteredCaptureIndicators(Clock::time_point now, D2D1_RECT_F shape);
    void drawMediaPrivacy(Clock::time_point now, D2D1_RECT_F shape, float opacity);
    void drawSystem(D2D1_RECT_F panel, bool record);
    void drawPrayerSchedule(D2D1_RECT_F panel);
    void drawAppVolume(size_t index, D2D1_RECT_F row, bool record);
    float volumeFraction(D2D1_POINT_2F point) const;
    void drawNav(D2D1_RECT_F panel, bool record);
    void drawScreenGlyph(Screen screen, D2D1_RECT_F rect, D2D1_COLOR_F color);
    static std::wstring_view screenLabel(Screen screen);
    void drawSharedTitle(D2D1_RECT_F compact, D2D1_RECT_F panel, float capture, float value);
    D2D1_RECT_F pillArtRect(D2D1_RECT_F pill, float capture) const;
    void drawArt(D2D1_RECT_F rect, float opacity);
    void drawFocusGlyph(D2D1_POINT_2F center, float size, D2D1_COLOR_F color);
    void drawTimerGlyph(D2D1_POINT_2F center, D2D1_COLOR_F color);
    void drawChipGlyph(D2D1_POINT_2F center, D2D1_COLOR_F color);
    void drawButton(D2D1_RECT_F rect, std::wstring_view label, Hit hit, bool record,
                    size_t index = 0, bool selected = false);
    void fillShell(D2D1_ROUNDED_RECT shape);
    void fillShell(ID2D1Geometry* shape);
    void drawSplit(Clock::time_point now, float amount);
    void drawAlertContent(D2D1_RECT_F area, float opacity, float copyOpacity);
    void drawAlertIcon(IslandAlert::Kind kind, D2D1_POINT_2F origin, D2D1_COLOR_F color);
    void drawBatteryCell(float left, float middle, unsigned level, D2D1_COLOR_F color);
    void drawInlineAlert(D2D1_RECT_F panel);
    void fillPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color);
    void pushLayer(ID2D1Geometry* mask, float opacity);
    void popLayer();
    void text(std::wstring_view value, IDWriteTextFormat* format, D2D1_RECT_F rect, D2D1_COLOR_F color);
    void addTarget(D2D1_RECT_F rect, Hit hit, size_t index = 0);
    bool hovered(Hit hit, size_t index = 0) const;
    bool pressed(Hit hit, size_t index = 0) const;
    ID2D1SolidColorBrush* ink(D2D1_COLOR_F color);
    ID2D1Bitmap* bitmap(Image& image);
    Image* icon(std::wstring const& appId);

    // Data shown by the island.
    MediaState media_;
    PowerState power_;
    SystemStats system_;
    NetworkStatus network_;
    std::optional<PrayerSchedule> prayerSchedule_;
    std::wstring audioOutput_;
    std::vector<AppVolume> appVolumes_;
    size_t appOffset_ = 0;
    int appWheelRemainder_ = 0;
    D2D1_RECT_F appViewport_{};
    std::wstring volumeDragId_;          // the app whose slider is being dragged
    float volumeDragValue_ = 0;
    D2D1_RECT_F volumeTrack_{};
    std::optional<IslandCommand> liveCommand_;
    std::optional<bool> windowsNotificationsQuiet_;
    bool animate_ = true;
    bool suppressed_ = false;
    float visibilityFrom_ = 1;
    Clock::time_point visibilityStart_{};
    std::optional<unsigned> microphoneColor_;
    std::optional<unsigned> cameraColor_;
    float microphoneFrom_ = 0;
    float cameraFrom_ = 0;
    Clock::time_point captureStart_{};

    // Cover colours cross-fade from the *From_ values to theme_, starting at themeStart_.
    CoverTheme theme_;
    D2D1_COLOR_F accentFrom_ = D2D1::ColorF(CoverTheme{}.accent);
    D2D1_COLOR_F tintFrom_ = D2D1::ColorF(CoverTheme{}.tint);
    D2D1_COLOR_F accent_ = accentFrom_;
    D2D1_COLOR_F tint_ = tintFrom_;
    Clock::time_point themeStart_{};

    // Dot <-> pill. The content snapshot stays while the pill shrinks so it can fade out.
    PillMode pillMode_ = PillMode::dot;
    PillMode pillContent_ = PillMode::media;
    PillMode preferredPill_ = PillMode::media;
    std::wstring pillTitle_;
    bool pillPlaying_ = false;
    Clock::time_point pausedAt_{};
    float pillFrom_ = 0;
    float pillVelocity_ = 0;
    float pillTarget_ = 0;
    Clock::time_point pillStart_{};
    Clock::time_point previewUntil_{};
    PillText pillLyric_;
    PillText previousPillLyric_;
    float previousPillAlpha_ = 1;
    float previousPillOffset_ = 0;
    D2D1_SIZE_F pillSizeFrom_{};
    D2D1_SIZE_F pillSizeTarget_{};
    Clock::time_point pillTransitionStart_{};
    Clock::duration pillTransitionTime_{};
    float pillLyricMaxWidth_ = 0;
    PillText pillTrackTitle_;
    float pillTrackMaxWidth_ = 0;
    float pillTrackMinimum_ = 0;
    D2D1_SIZE_F pillTrackSize_{};
    float pillTitleFrom_ = 1;
    bool pillTitleVisible_ = true;

    // Panel. panelFrom_ animates toward 1 (open) or 0 (closed) from panelStart_.
    bool open_ = false;
    float panelFrom_ = 0;
    float panelVelocity_ = 0;
    Clock::time_point panelStart_{};
    D2D1_RECT_F closingPanel_{};
    bool shareTitle_ = false;
    Screen screen_ = Screen::media;
    Screen screenFrom_ = Screen::media;
    Clock::time_point screenStart_{};
    float heightFrom_ = 0;
    float widthFrom_ = 0;
    Clock::time_point widthStart_{};
    bool lyricsOpen_ = false;
    bool sourcePickerOpen_ = false;
    size_t sourceOffset_ = 0;
    int sourceWheelRemainder_ = 0;
    D2D1_RECT_F sourceMenuRect_{};
    bool scratchEditing_ = false;
    bool dropHover_ = false;
    bool popupOpen_ = false;
    std::vector<DropPreview> dropItems_;
    std::map<DropThumbnail const*, winrt::com_ptr<ID2D1Bitmap>> dropBitmaps_;
    size_t dropOffset_ = 0;
    int dropWheelRemainder_ = 0;
    D2D1_RECT_F dropViewport_{};
    bool panelPinned_ = false;
    ScratchpadView const* scratchpad_ = nullptr;
    std::optional<D2D1_RECT_F> scratchEditor_;
    winrt::com_ptr<IDWriteTextLayout> scratchLayout_;
    std::wstring scratchLayoutText_;
    float scratchScroll_ = 0;
    float scratchScrollLimit_ = 0;
    LyricsView const* lyrics_ = nullptr;
    float lyricsWidth_ = 0;
    float lyricScrollFrom_ = 0;
    float lyricScrollTo_ = 0;
    float lyricScrollShown_ = 0;
    Clock::time_point lyricScrollStart_{};
    Clock::time_point lyricFollowAt_{};
    Clock::duration lyricFollowLeft_{};
    D2D1_RECT_F lyricViewport_{};
    std::vector<DWRITE_TEXT_RANGE> lyricRanges_;
    std::ptrdiff_t activeLyric_ = -1;
    float activeLyricTop_ = 0;
    float activeLyricHeight_ = 0;
    float lyricContentHeight_ = 0;
    float lyricLayoutWidth_ = 0;
    unsigned lyricLayoutRevision_ = 0;
    winrt::com_ptr<IDWriteTextLayout> lyricLayout_;
    winrt::com_ptr<ID2D1SolidColorBrush> lyricActiveInk_;
    // After a screen change shrinks the panel, its old height still counts as "near" until the
    // pointer enters the new panel, so the pointer left behind on the nav doesn't close it.
    float keepHeight_ = 0;

    // Pointer.
    std::optional<Clock::time_point> openAt_;
    std::optional<Clock::time_point> closeAt_;
    Hit hovered_ = Hit::none;
    size_t hoveredIndex_ = 0;
    Hit pressed_ = Hit::none;
    size_t pressedIndex_ = 0;
    bool dragging_ = false;
    float dragFraction_ = 0;
    bool buttonWasDown_ = false;
    int wheelRemainder_ = 0;
    Clock::time_point wheelAt_{};
    D2D1_RECT_F panelWheel_{};

    // Focus timer.
    int focusMinutes_ = 25;
    int breakMinutes_ = 5;
    bool breakPhase_ = false;
    bool quickView_ = false;
    bool autoCycle_ = false;
    int focusRound_ = 1;     // 1..focusRounds while the automatic cycle is on
    Countdown focus_{25 * 60};
    Countdown quick_{60};

    // Alerts. A bubble opens (alertTo_ = 1), holds, then closes (alertTo_ = 0); while the panel is
    // open the alert shows inline instead until alertUntil_.
    std::optional<IslandAlert> alert_;
    std::optional<IslandAlert> queued_;
    bool alertInline_ = false;
    float alertFrom_ = 0;
    float alertVelocity_ = 0;
    float alertTo_ = 0;
    Clock::time_point alertStart_{};
    std::optional<Clock::time_point> alertHoldUntil_;
    Clock::time_point alertUntil_{};
    float alertWidth_ = 52;

    // Last frame: hit targets in stage DIPs and where the surface sits.
    std::vector<Target> targets_;
    D2D1_RECT_F timelineRect_{};
    bool recordPanel_ = false;
    float scale_ = 1;
    POINT origin_{};
    POINT anchor_{};

    winrt::com_ptr<ID2D1Factory> graphics_;
    winrt::com_ptr<ID2D1DCRenderTarget> target_;
    winrt::com_ptr<ID2D1SolidColorBrush> brush_;
    winrt::com_ptr<ID2D1StrokeStyle> roundStroke_;
    winrt::com_ptr<ID2D1StrokeStyle> dropDash_;
    std::array<winrt::com_ptr<ID2D1Layer>, 4> layers_;
    size_t layerDepth_ = 0;
    D2D1_MATRIX_3X2_F base_ = D2D1::Matrix3x2F::Identity();
    winrt::com_ptr<IDWriteFactory> fonts_;
    winrt::com_ptr<IDWriteTextFormat> pillTitleFont_;
    winrt::com_ptr<IDWriteTextFormat> timerFont_;
    winrt::com_ptr<IDWriteTextFormat> titleFont_;
    winrt::com_ptr<IDWriteTextFormat> headingCenterFont_;
    winrt::com_ptr<IDWriteTextFormat> bodyFont_;
    winrt::com_ptr<IDWriteTextFormat> lyricFont_;
    winrt::com_ptr<IDWriteTextFormat> pillLyricFont_;
    winrt::com_ptr<IDWriteTextFormat> scratchFont_;
    winrt::com_ptr<IDWriteTextFormat> bodyEndFont_;
    winrt::com_ptr<IDWriteTextFormat> strongEndFont_;
    winrt::com_ptr<IDWriteTextFormat> monoFont_;
    winrt::com_ptr<IDWriteTextFormat> monoEndFont_;
    winrt::com_ptr<IDWriteTextFormat> clockFont_;
    winrt::com_ptr<IDWriteTextFormat> buttonFont_;
    winrt::com_ptr<IDWriteTextFormat> glyphFont_;
    winrt::com_ptr<IDWriteTextFormat> letterFont_;
    winrt::com_ptr<IDWriteTextFormat> alertTitleFont_;
    winrt::com_ptr<IDWriteTextFormat> alertValueFont_;
    std::vector<winrt::com_ptr<IDWriteInlineObject>> ellipses_;
    HDC surface_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ originalBitmap_ = nullptr;
    SIZE bitmapSize_{};
    SIZE surfaceSize_{};

    std::shared_ptr<const Cover> coverSource_;
    Image cover_;
    std::map<std::wstring, Image> icons_;
};

}
