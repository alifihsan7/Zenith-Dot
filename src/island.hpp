#pragma once

#include "desktop.hpp"
#include "bluetooth.hpp"
#include "audio_spectrum.hpp"
#include "media.hpp"
#include "lyrics.hpp"
#include "scratchpad.hpp"
#include "tasks.hpp"
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
                      selectAudioOutput, soundSettings, notificationSettings, removeDrop, clearDrop, openDropImage, appVolume, appMute,
                      focusRhythm, prayerLocation, openSettings, overlapMode,
                      addTask, toggleTask, removeTask, clearDoneTasks,
                      setBrightness, openPrayerSearch };
    Kind kind = Kind::none;
    double seconds = 0;              // seek position; appVolume: level 0..1; appMute: 1 = on
    std::wstring appId;
    size_t index = 0;                // focusRhythm: the rhythm (3 = Custom)
    unsigned focusMinutes = 0;       // focusRhythm: Custom lengths and rounds
    unsigned breakMinutes = 0;
    unsigned rounds = 0;
};

// A short notice that splits off the pill as a liquid bubble (or shows inside the open panel).
struct IslandAlert {
    enum class Kind { copy, charging, battery, lowBattery, focusDone, breakDone, timerDone,
                      microphone, camera, screenCapture, capsLock, numLock, bluetooth, bluetoothAudio, prayer,
                      notification, connection, mediaStarted };
    Kind kind = Kind::copy;
    std::wstring title;
    std::wstring value;
    std::optional<unsigned> color;   // icon colour (0xRRGGBB); the accent when empty
    int priority = 1;                // a waiting alert is replaced only by one of equal or higher priority
    std::chrono::milliseconds hold{2600};
    std::optional<unsigned> battery = std::nullopt;   // drawn as a battery cell before value
    bool fromCompanion = false;
    struct DotOrigin {
        float width;
        float iconLeft;
    };
    std::optional<DotOrigin> dotOrigin;
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
    void setSpectrum(Spectrum const& spectrum) { spectrum_ = spectrum; }
    bool wantsAudioSpectrum() const;
    void setPower(PowerState const& power);
    void setSystem(SystemStats const& stats);
    void setNetwork(NetworkStatus const& status);
    void setBluetoothDevices(std::vector<ConnectedBluetoothDevice> devices);
    bool setBrightness(std::optional<float> level);
    void setPrayerSchedule(std::optional<PrayerSchedule> const& schedule);
    void setPrayerLoad(PrayerLoad state) { prayerLoad_ = state; }
    void setAudioOutput(std::wstring const& output);
    void setAudioOutputs(std::vector<AudioOutput> outputs);
    void setAppVolumes(std::vector<AppVolume> apps);
    // A command produced while dragging (an app volume slider), to apply before the button is released.
    std::optional<IslandCommand> takeLiveCommand();
    bool alertsQuiet() const;
    // Focus keeps the island quiet: other bubbles and prayer chimes wait while a Focus phase runs.
    bool focusQuiet() const;
    bool soundsQuiet() const { return alertsQuiet() || focusQuiet(); }
    // True once after Focus, Break, or a timer ends; the app plays the chime.
    bool takeFinishedCountdown();
    bool setWindowsNotificationsQuiet(std::optional<bool> quiet);
    void setSuppressed(bool suppressed);
    bool setDotOnly(bool on);
    bool setCaptureActivity(CaptureActivity const& activity);
    void setLyrics(LyricsView const& lyrics);
    void setScratchpad(ScratchpadView const& scratchpad);
    void setScratchEditing(bool editing);
    void setTasks(TaskView const& tasks);
    void setTaskInputOpen(bool open);
    void setDropItems(std::vector<DropPreview> items);
    void setDropHover(bool hovering);
    void setPopupOpen(bool open);
    void showDropZone(bool hold = false);
    std::optional<size_t> dropItemAt(POINT client) const;
    void closePanel();
    bool panelPinned() const;
    std::optional<RECT> scratchEditorRect() const;
    std::optional<RECT> taskInputRect() const;
    // Inline prayer location search: the Prayer panel swaps its timetable for a search field and results.
    std::optional<RECT> prayerSearchRect() const;
    bool prayerPicking() const { return prayerPicking_; }
    void setPrayerQuery(std::wstring query);
    void setPrayerSearchOpen(bool open) { prayerSearchOpen_ = open; }
    void movePrayerHighlight(int delta);
    std::optional<unsigned> highlightedPrayerLocation() const;
    void closePrayerPicker();
    // Scratchpad surfaces follow the active session's accent; the Win32 inputs on top use the same colors.
    COLORREF scratchEditorColor() const;
    COLORREF taskInputColor() const;
    COLORREF prayerSearchColor() const;
    D2D1_COLOR_F scratchSurface() const;
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
    enum class Hit { none, pill, activityDot, privacyBadge, previous, toggle, next, timeline, cover, sourcePicker, sourceMenu, source, navMedia, navTimers,
                     navScratch, navSystem, navPrayer, focusToggle, focusReset, timerView, focusAdjust, timerPreset,
                     timerToggle, timerReset, quickAdjust, timerDuration, lyrics, lyricLine, retryLyrics, lyricsDelay, scratchEdit,
                     navDrop, dropItem, removeDrop, clearDrop, dropImage, panelPin, audioPicker, notificationSettings,
                     appVolume, appMute, focusRhythm, focusSkip, prayerLocationPicker,
                     audioOutputMenu, audioOutputChoice, soundSettings, navHome, homeSettings, homeStartTimer, homeOverlapMode,
                     scratchMode, taskAdd, taskToggle, taskRemove, taskClearDone,
                     brightness, prayerResult, prayerSearchField };
    using Screen = Feature;
    Settings preferences_;
    std::vector<Screen> screens_{Screen::home, Screen::media, Screen::timers, Screen::scratch, Screen::drop, Screen::system};
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
    struct CaptureIndicator {
        IslandAlert::Kind kind;
        unsigned color;
        std::wstring app;
        bool shown = false;
        float from = 0;
        Clock::time_point start{};
    };
    void updateCaptureIndicators(Clock::time_point now);
    float captureProgress(Clock::time_point now, CaptureIndicator const& indicator) const;
    std::array<float, 3> captureAmounts(Clock::time_point now) const;
    PillMode activityCompanion() const;
    bool activitySwapActive(Clock::time_point now) const;
    void swapActivities(Clock::time_point now);
    void retirePausedActivity(PillMode mode, Clock::time_point now);
    void drawCountdownCheck(D2D1_POINT_2F center, D2D1_COLOR_F color, float scale = 1);
    float activityWidth(PillMode mode, Clock::time_point now) const;
    void drawActivityIndicator(Clock::time_point now, D2D1_RECT_F shape, PillMode mode, D2D1_COLOR_F color);
    void drawActivityDot(Clock::time_point now, D2D1_RECT_F shape, PillMode mode, bool interactive);
    void drawActivitySwap(Clock::time_point now, D2D1_RECT_F pill, D2D1_RECT_F dot);
    void drawActivityHandoff(Clock::time_point now, D2D1_RECT_F pill);
    void drawHandoffShell(D2D1_RECT_F retiring, D2D1_RECT_F incoming, float merge);
    void drawHandoffOutgoing(D2D1_RECT_F pill, float opacity);
    D2D1_RECT_F compactShape(Clock::time_point now, float amount) const;
    float privacyBadgeWidth(Clock::time_point now) const;
    D2D1_RECT_F privacyBadgeRect(Clock::time_point now, D2D1_RECT_F pill) const;
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
    void updatePill(Clock::time_point now, bool keepActivity = true);
    void updatePillLayout(Clock::time_point now, float workWidth);
    void updateTheme(Clock::time_point now);
    bool alertAllowed(IslandAlert::Kind kind) const;
    void setOpen(bool open, Clock::time_point now);
    void openFromCompact(Clock::time_point now);
    PillMode homeTimer() const;
    std::array<Screen, 4> homeRows() const;
    void showScreen(Screen screen, Clock::time_point now);
    void toggleFocus(Clock::time_point now);
    void selectTimerView(size_t index);
    void selectFocusPhase(bool resting);
    void adjustTimer(size_t index, int steps = 1);
    void setTimerDuration(int seconds);
    int timerDurationLimit() const;
    int phaseSeconds(bool resting) const;
    std::wstring phaseName(bool resting) const;
    bool lastRound() const;
    void skipBreak(Clock::time_point now);
    void endFocusSession();
    IslandCommand chooseRhythm(unsigned rhythm);
    void adjustRhythm(size_t index);
    IslandCommand rhythmCommand() const;
    void resizeTimers(Clock::time_point now);
    int sessionSecondsLeft(Clock::time_point now) const;
    void finishFocus(Clock::time_point now, bool fromCompanion);
    void completeCountdown(Clock::time_point now, PillMode mode, IslandAlert alert);
    D2D1_POINT_2F stagePoint(POINT client) const;
    std::optional<Target> targetAt(D2D1_POINT_2F point) const;
    float timelineFraction(D2D1_POINT_2F point) const;
    std::wstring title() const;
    void startAlert(IslandAlert alert, Clock::time_point now, bool update);
    void beginAlertClose(Clock::time_point now);
    void finishAlert(Clock::time_point now);
    void clearAlerts();
    float alertValue(Clock::time_point now) const;
    MotionSample alertMotion(Clock::time_point now) const;
    static MotionSample reverseSplitMotion(Clock::time_point start, Clock::time_point now);
    bool bubbleShowing() const;
    float inlineInset() const;
    float measure(std::wstring_view value, IDWriteTextFormat* format);

    void prepareSurface(int width, int height, bool animating);
    void prepareText();
    void releaseDeviceResources();
    void drawCompactContent(Clock::time_point now, D2D1_RECT_F shape, bool hideTitle, bool hideArt = false);
    void drawLyricsGlyph(D2D1_POINT_2F origin, D2D1_COLOR_F color);
    void drawLogo(D2D1_POINT_2F origin, float height);
    void drawSpectrumBars(D2D1_POINT_2F center, float spacing, float barWidth, float maxHeight, D2D1_COLOR_F color);
    void drawPillContent(Clock::time_point now, float width, bool hideTitle);
    void drawIsland(Clock::time_point now, float value, float amount, bool morphTitle);
    void drawHome(D2D1_RECT_F panel, Clock::time_point now, bool record);
    void drawScreen(Screen screen, D2D1_RECT_F panel, Clock::time_point now, float artOpacity, bool hideTitle, bool record);
    void drawMedia(D2D1_RECT_F panel, Clock::time_point now, float artOpacity, bool hideTitle, bool record);
    void drawSourceIcon(MediaSource const& source, D2D1_POINT_2F center);
    void drawSourceMenu(D2D1_RECT_F panel, float reveal);
    void drawAudioOutputMenu(D2D1_RECT_F panel, float reveal);
    void beginMenuReveal(D2D1_RECT_F menu, float reveal);
    void endMenuReveal(float reveal);
    void drawPickerSurface(D2D1_RECT_F rect, Hit hit);
    void drawPickerButton(D2D1_RECT_F rect, std::wstring_view label, Hit hit, bool record, bool expanded,
                          std::optional<bool> headphones = {});
    void drawLyrics(D2D1_RECT_F panel, Clock::time_point now, double position);
    void prepareLyricLayout(float width);
    void drawTimers(D2D1_RECT_F panel, Clock::time_point now, bool record);
    void drawScratchpad(D2D1_RECT_F panel, bool record);
    void drawTasks(float x, float top, float headerY, float statusLeft, float statusRight, bool record);
    void setScratchMode(bool tasks, Clock::time_point now);
    void drawDropZone(D2D1_RECT_F panel, bool record);
    void drawCaptureIndicators(D2D1_POINT_2F right, std::array<float, 3> const& amounts);
    void drawCenteredCaptureIndicators(D2D1_RECT_F shape, std::array<float, 3> const& amounts);
    float companionSlotOpacity(Clock::time_point now) const;
    void drawPrivacyBadge(Clock::time_point now, D2D1_RECT_F badge);
    void drawPrivacyDetails(D2D1_RECT_F details);
    void drawCompanionShell(D2D1_RECT_F shape, float pinch);
    void drawSystem(D2D1_RECT_F panel, bool record);
    void drawPrayerSchedule(D2D1_RECT_F panel, bool record);
    void drawAppVolume(AppVolume const& app, size_t index, D2D1_RECT_F row, bool record);
    float volumeFraction(D2D1_POINT_2F point) const;
    float navExpansion(Clock::time_point now) const;
    float navSelection(Clock::time_point now) const;
    float navHeightAt(Clock::time_point now) const;
    bool setNavExpanded(bool expanded, Clock::time_point now);
    void drawNav(D2D1_RECT_F panel, Clock::time_point now, bool record);
    void drawConnection(D2D1_RECT_F column, bool record);
    void drawAudioGlyph(D2D1_POINT_2F center, bool headphones, D2D1_COLOR_F color);
    void drawPickerCheck(D2D1_POINT_2F center);
    void drawPickerRow(D2D1_RECT_F row, std::wstring_view label, Hit hit, size_t index,
                       bool selected, std::optional<bool> headphones = {});
    void drawScreenGlyph(Screen screen, D2D1_RECT_F rect, D2D1_COLOR_F color);
    static std::wstring_view screenLabel(Screen screen);
    float trackRightSpace() const;
    void drawSharedTitle(D2D1_RECT_F compact, D2D1_RECT_F panel, float value);
    D2D1_RECT_F pillArtRect(D2D1_RECT_F pill) const;
    void drawArt(D2D1_RECT_F rect, float opacity);
    void drawFocusGlyph(D2D1_POINT_2F center, float size, D2D1_COLOR_F color);
    void drawTimerGlyph(D2D1_POINT_2F center, D2D1_COLOR_F color);
    void drawChipGlyph(D2D1_POINT_2F center, D2D1_COLOR_F color);
    void drawButton(D2D1_RECT_F rect, std::wstring_view label, Hit hit, bool record,
                    size_t index = 0, bool selected = false);
    void fillShell(D2D1_ROUNDED_RECT shape);
    void fillShell(ID2D1Geometry* shape);
    void drawSplit(Clock::time_point now, float amount, std::optional<D2D1_RECT_F> slot);
    D2D1_RECT_F closingDotShape(Clock::time_point now, D2D1_RECT_F pill) const;
    void drawBubbleToDot(Clock::time_point now, D2D1_RECT_F pill, D2D1_RECT_F bubble, bool fromSlot);
    void drawBubbleFromDot(Clock::time_point now, D2D1_RECT_F pill, D2D1_RECT_F bubble);
    void drawBubblePrimary(Clock::time_point now, D2D1_RECT_F pill, float opacity);
    void drawAlertContent(D2D1_RECT_F area, float opacity, float textOpacity,
                          std::optional<float> iconOrigin = std::nullopt);
    void drawAlertIcon(IslandAlert::Kind kind, D2D1_POINT_2F origin, D2D1_COLOR_F color, float size = 15);
    void drawBatteryCell(float left, float middle, unsigned level, D2D1_COLOR_F color, bool charging = false);
    void drawInlineAlert(D2D1_RECT_F panel);
    void fillPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color);
    void pushLayer(ID2D1Geometry* mask, float opacity);
    void popLayer();
    void text(std::wstring_view value, IDWriteTextFormat* format, D2D1_RECT_F rect, D2D1_COLOR_F color);
    void addTarget(D2D1_RECT_F rect, Hit hit, size_t index = 0);
    bool hovered(Hit hit, size_t index = 0) const;
    // 0..1 hover highlight that fades in and out; pressed states stay immediate.
    float hoverLevel(Hit hit, size_t index = 0) const;
    void setHovered(Hit hit, size_t index, Clock::time_point now);
    float shrinkToggleValue(Clock::time_point now);
    void setDotHover(bool hot, Clock::time_point now);
    float dotHover(Clock::time_point now) const;
    bool pressed(Hit hit, size_t index = 0) const;
    ID2D1SolidColorBrush* ink(D2D1_COLOR_F color);
    ID2D1Bitmap* bitmap(Image& image);
    Image* icon(std::wstring const& appId);

    // Data shown by the island.
    MediaState media_;
    PowerState power_;
    SystemStats system_;
    NetworkStatus network_;
    std::vector<ConnectedBluetoothDevice> bluetoothDevices_;
    size_t bluetoothOffset_ = 0;
    int bluetoothWheelRemainder_ = 0;
    D2D1_RECT_F bluetoothViewport_{};
    std::optional<PrayerSchedule> prayerSchedule_;
    std::wstring audioOutput_;
    std::vector<AudioOutput> audioOutputs_;
    bool audioPickerOpen_ = false;
    size_t audioOutputOffset_ = 0;
    int audioOutputWheelRemainder_ = 0;
    D2D1_RECT_F audioOutputMenuRect_{};
    std::vector<AppVolume> appVolumes_;
    size_t appOffset_ = 0;
    int appWheelRemainder_ = 0;
    D2D1_RECT_F appViewport_{};
    std::wstring volumeDragId_;          // the app whose slider is being dragged
    std::optional<float> brightness_;   // the island's display, 0..1
    // A new reading (monitor buttons, Windows) slides the slider; the user's own drag does not.
    float brightnessFrom_ = 0;
    Clock::time_point brightnessStart_{};
    float shownBrightness(Clock::time_point now) const;
    bool brightnessDrag_ = false;
    float brightnessDragValue_ = 0;
    void drawBrightness(float x, float top, float fullWidth, bool record);
    float volumeDragValue_ = 0;
    D2D1_RECT_F volumeTrack_{};
    std::optional<IslandCommand> liveCommand_;
    std::optional<bool> windowsNotificationsQuiet_;
    bool animate_ = true;
    bool suppressed_ = false;
    float visibilityFrom_ = 1;
    Clock::time_point visibilityStart_{};
    bool captureRecording_ = false;
    std::array<CaptureIndicator, 3> captureIndicators_{{
        {IslandAlert::Kind::screenCapture, 0xbdabff, {}},
        {IslandAlert::Kind::camera, 0x30d158, {}},
        {IslandAlert::Kind::microphone, 0xff9f0a, {}},
    }};

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
    struct ActivitySwap {
        Clock::time_point start{};
        PillMode incoming = PillMode::dot;
        PillMode outgoing = PillMode::dot;
        D2D1_RECT_F pill{};
        D2D1_RECT_F dot{};
        std::wstring title;
        bool retireOutgoing = false;
        bool outgoingCompleted = false;
    } activitySwap_;
    std::optional<D2D1_RECT_F> lastActivityDot_;
    D2D1_RECT_F retiringDot_{};
    PillMode retiringDotMode_ = PillMode::dot;
    Clock::time_point dotExitStart_{};
    struct CountdownCompletion {
        Clock::time_point start{};
        Clock::time_point until{};
        PillMode mode = PillMode::dot;
        std::wstring label;
    } countdownCompletion_;
    std::wstring pillTitle_;
    bool pillPlaying_ = false;
    Spectrum spectrum_{};
    Clock::time_point pausedAt_{};
    Clock::time_point pausedMediaRevealAt_{};
    bool compactPausedMediaAfterTimer_ = false;
    bool mediaIntroPending_ = false;
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
    float pillTrackRight_ = 0;
    D2D1_SIZE_F pillTrackSize_{};
    float pillTitleFrom_ = 1;
    bool pillTitleVisible_ = true;

    // Panel. panelFrom_ animates toward 1 (open) or 0 (closed) from panelStart_.
    bool open_ = false;
    bool panelOpened_ = false;
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
    PrayerLoad prayerLoad_ = PrayerLoad::loading;
    bool navExpanded_ = false;
    float navFrom_ = 0;
    Clock::time_point navStart_{};
    float navSelectionFrom_ = 0;
    Clock::time_point navSelectionStart_{};
    D2D1_RECT_F navHoverRect_{};
    size_t sourceOffset_ = 0;
    int sourceWheelRemainder_ = 0;
    D2D1_RECT_F sourceMenuRect_{};
    bool scratchEditing_ = false;
    // Scratchpad's second page: a task list with its own one-line input popup.
    bool scratchTasks_ = false;
    TaskView const* tasks_ = nullptr;
    size_t tasksOffset_ = 0;
    size_t tasksSeen_ = 0;
    int tasksWheelRemainder_ = 0;
    bool taskInputOpen_ = false;
    std::optional<D2D1_RECT_F> taskInput_;
    D2D1_RECT_F taskViewport_{};
    // Task list motion, like Drop Zone: kept rows slide, new rows fade in, removed rows fade out where
    // they were, and a row whose mark changed fills (or empties) its checkbox and draws its strike.
    struct TaskMotion {
        float fromOffset = 0;
        bool entering = false;
        bool checkChanged = false;
    };
    struct TaskGhost {
        Task task;
        float slotY = 0;
    };
    std::vector<Task> tasksShown_;
    std::vector<TaskMotion> taskMotion_;
    std::vector<TaskGhost> taskGhosts_;
    Clock::time_point taskMotionStart_{};
    // Segmented controls slide their highlight to a new choice; Scratchpad fades its content on a mode switch.
    struct SegmentSlide {
        size_t shown = static_cast<size_t>(-1);
        float from = 0;
        float to = 0;
        Clock::time_point start{};
        Clock::time_point drawnAt{};
    };
    SegmentSlide timerSlide_;
    SegmentSlide scratchSlide_;
    Clock::time_point scratchModeStart_{};
    float slidePosition(SegmentSlide& slide, size_t selected, Clock::time_point now);
    // Dropdown menus grow from their top edge on open and shrink back on close. A closing menu is
    // still drawn for a moment, but records no hit targets.
    struct MenuMotion {
        bool open = false;
        float from = 0;
        Clock::time_point start{};
        Clock::time_point drawnAt{};
    };
    MenuMotion sourceMenuMotion_;
    MenuMotion audioMenuMotion_;
    bool suppressTargets_ = false;
    D2D1_MATRIX_3X2_F menuTransform_{};
    float menuReveal(MenuMotion& motion, bool open, Clock::time_point now);
    // Rows in the App Volume and Bluetooth lists slide when their order changes and fade in when new.
    // Only a change of membership or order animates; the per-second refresh of volumes does not.
    struct RowMotion {
        std::vector<std::wstring> keys;
        std::vector<float> fromSlot;      // slot offset each row starts from
        std::vector<bool> entering;
        size_t offset = 0;
        Clock::time_point start{};
    };
    RowMotion appMotion_;
    RowMotion bluetoothMotion_;
    // Returns the rows that left the visible window (old index, slot) when membership changed.
    std::optional<std::vector<std::pair<size_t, float>>> updateRowMotion(RowMotion& motion, std::vector<std::wstring> keys,
                                                                         size_t offset, size_t visible);
    // Row top shift in pixels and opacity for the row at index, for the current frame.
    std::pair<float, float> rowMotion(RowMotion const& motion, size_t index, float rowHeight) const;
    float rowGhostOpacity(RowMotion const& motion) const;
    std::vector<std::pair<AppVolume, float>> appGhosts_;
    std::vector<std::pair<ConnectedBluetoothDevice, float>> bluetoothGhosts_;
    bool prayerPicking_ = false;
    bool prayerSearchOpen_ = false;
    std::wstring prayerQuery_;
    std::vector<unsigned> prayerResults_;    // location ids; 0 turns prayer times off
    size_t prayerResultOffset_ = 0;
    size_t prayerHighlight_ = 0;
    int prayerWheelRemainder_ = 0;
    std::optional<D2D1_RECT_F> prayerSearch_;
    D2D1_RECT_F prayerResultsViewport_{};
    Clock::time_point prayerModeStart_{};
    void openPrayerPicker(Clock::time_point now);
    void setPrayerPicking(bool picking, Clock::time_point now);
    void filterPrayerLocations();
    void drawPrayerPicker(D2D1_RECT_F panel, bool record);
    void drawTaskRow(Task const& task, size_t index, float top, float left, float width, float check, bool record);
    bool dropHover_ = false;
    bool popupOpen_ = false;
    std::vector<DropPreview> dropItems_;
    std::map<DropThumbnail const*, winrt::com_ptr<ID2D1Bitmap>> dropBitmaps_;
    size_t dropOffset_ = 0;
    // Row motion after the item list changes: kept rows slide from their old slot, new rows fade in,
    // and removed rows that were on screen fade out where they were.
    struct DropRowMotion {
        float fromOffset = 0;
        bool entering = false;
    };
    struct DropGhost {
        DropPreview item;
        float slotY = 0;
    };
    std::vector<DropRowMotion> dropMotion_;
    std::vector<DropGhost> dropGhosts_;
    Clock::time_point dropMotionStart_{};
    float dropRowsFrom_ = 0;   // visible rows before the last change
    // Home: activity rows slide to new places and new ones fade in; clock digits roll on change.
    std::vector<Screen> homeRowsShown_;
    std::vector<Screen> homeRowsFrom_;
    Clock::time_point homeRowsStart_{};
    std::wstring homeClock_;
    std::wstring homeClockFrom_;
    Clock::time_point homeClockStart_{};
    Clock::time_point homeDrawnAt_{};
    float dropFieldFrom_ = 1;
    Clock::time_point dropFieldStart_{};
    void drawDropRow(DropPreview const& item, size_t index, float rowY, float x, bool record);
    float dropFieldValue(Clock::time_point now) const;
    int dropWheelRemainder_ = 0;
    D2D1_RECT_F dropViewport_{};
    bool panelPinned_ = false;
    bool dotOnly_ = false;
    bool countdownFinished_ = false;
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
    Clock::time_point dotHoverStart_{};
    float dotHoverFrom_ = 0;
    bool dotHot_ = false;
    bool privacyDetailsOpen_ = false;
    size_t hoveredIndex_ = 0;
    Hit fadingHover_ = Hit::none;
    size_t fadingHoverIndex_ = 0;
    float fadingHoverFrom_ = 0;
    float hoverFrom_ = 0;
    Clock::time_point hoverStart_{};
    Clock::time_point drawNow_{};
    std::optional<bool> shrinkShown_;
    float shrinkFrom_ = 0;
    Clock::time_point shrinkStart_{};
    Hit pressed_ = Hit::none;
    size_t pressedIndex_ = 0;
    bool dragging_ = false;
    float dragFraction_ = 0;
    float timerDragY_ = 0;
    bool timerDragged_ = false;
    bool timerRepeated_ = false;
    Clock::time_point timerRepeatAt_{};
    int timerWheelRemainder_ = 0;
    size_t timerWheelUnit_ = 0;
    int timerDigitsFrom_ = 0;
    int timerDigitsTo_ = 0;
    Clock::time_point timerDigitsStart_{};
    bool buttonWasDown_ = false;
    int wheelRemainder_ = 0;
    Clock::time_point wheelAt_{};
    D2D1_RECT_F panelWheel_{};

    // Focus timer.
    bool breakPhase_ = false;
    bool quickView_ = true;   // Timers opens on Quick Timer
    int focusRound_ = 1;     // 1..preferences_.focusRounds() in the current session
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
    bool alertUsesCompanionSlot_ = false;
    bool alertFromOccludedDot_ = false;
    struct AlertDot {
        bool idle = false;
        PillMode mode = PillMode::dot;
    };
    std::optional<AlertDot> closingDot_;
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
    float timerDigitWidth_ = 7.25f;
    std::wstring measuredTimerLabel_;
    float timerLabelWidth_ = 0;
    winrt::com_ptr<IDWriteTextFormat> titleFont_;
    winrt::com_ptr<IDWriteTextFormat> mediaTitleFont_;
    winrt::com_ptr<IDWriteTextFormat> prayerTitleFont_;
    winrt::com_ptr<IDWriteTextFormat> prayerTimeFont_;
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
