#include "prayer.hpp"
#include "resource.h"

#include <dwmapi.h>
#include <algorithm>
#include <numeric>
#include <vector>

namespace zenith {
namespace {

constexpr COLORREF backgroundColor = RGB(15, 18, 24);
constexpr COLORREF fieldColor = RGB(24, 29, 38);
constexpr COLORREF focusColor = RGB(43, 65, 89);
constexpr COLORREF foregroundColor = RGB(240, 244, 250);
constexpr COLORREF mutedColor = RGB(165, 178, 196);
constexpr COLORREF accentColor = RGB(145, 202, 255);
constexpr COLORREF selectedColor = RGB(31, 48, 69);

struct Picker {
    unsigned current;
    std::vector<unsigned> order;
    HBRUSH background = CreateSolidBrush(backgroundColor);
    HBRUSH field = CreateSolidBrush(fieldColor);
    HBRUSH focusedField = CreateSolidBrush(selectedColor);

    ~Picker() {
        DeleteObject(background);
        DeleteObject(field);
        DeleteObject(focusedField);
    }
};

int px(HWND window, int value) {
    return MulDiv(value, GetDpiForWindow(window), 96);
}

void rounded(HDC dc, RECT rect, int radius, COLORREF fill) {
    const auto brush = SelectObject(dc, GetStockObject(DC_BRUSH));
    const auto pen = SelectObject(dc, GetStockObject(NULL_PEN));
    SetDCBrushColor(dc, fill);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, pen);
    SelectObject(dc, brush);
}

void drawSearch(HWND window, HDC dc) {
    const auto edit = GetDlgItem(window, IDC_PRAYER_SEARCH);
    RECT bounds{};
    GetWindowRect(edit, &bounds);
    MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&bounds), 2);
    bounds.left -= px(window, 33);
    bounds.right += px(window, 10);
    InflateRect(&bounds, 0, px(window, 6));
    const bool focused = GetFocus() == edit;
    rounded(dc, bounds, px(window, 14), focused ? selectedColor : fieldColor);

    const int x = bounds.left + px(window, 16);
    const int y = (bounds.top + bounds.bottom) / 2 - px(window, 1);
    const int r = px(window, 4);
    const auto pen = SelectObject(dc, GetStockObject(DC_PEN));
    const auto brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    SetDCPenColor(dc, focused ? accentColor : mutedColor);
    Ellipse(dc, x - r, y - r, x + r + 1, y + r + 1);
    MoveToEx(dc, x + r - 1, y + r - 1, nullptr);
    LineTo(dc, x + px(window, 7), y + px(window, 7));
    SelectObject(dc, brush);
    SelectObject(dc, pen);
}

void drawAction(HWND window, DRAWITEMSTRUCT const& item, HBRUSH background) {
    FillRect(item.hDC, &item.rcItem, background);
    const bool primary = item.CtlID == IDOK;
    const bool enabled = !(item.itemState & ODS_DISABLED);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    POINT pointer{};
    GetCursorPos(&pointer);
    const bool hot = enabled && WindowFromPoint(pointer) == item.hwndItem;
    const auto fill = primary && enabled ? pressed ? RGB(119, 170, 220) : accentColor
                                        : focused ? focusColor : hot || pressed ? selectedColor : fieldColor;
    const auto face = focused && primary && enabled ? foregroundColor : fill;
    rounded(item.hDC, item.rcItem, px(window, 13), face);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, primary && enabled ? backgroundColor : enabled ? foregroundColor : mutedColor);
    RECT label = item.rcItem;
    DrawTextW(item.hDC, primary ? L"Select" : L"Cancel", -1, &label, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

bool contains(std::wstring_view text, std::wstring_view term) {
    return term.empty() || FindNLSStringEx(LOCALE_NAME_INVARIANT, FIND_FROMSTART | NORM_IGNORECASE,
        text.data(), static_cast<int>(text.size()), term.data(), static_cast<int>(term.size()), nullptr,
        nullptr, nullptr, 0) >= 0;
}

void updateList(HWND window, Picker const& picker) {
    wchar_t search[96]{};
    GetDlgItemTextW(window, IDC_PRAYER_SEARCH, search, static_cast<int>(std::size(search)));
    const auto list = GetDlgItem(window, IDC_PRAYER_RESULTS);
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    if (contains(L"Off", search)) {
        const auto row = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Off"));
        if (row >= 0) SendMessageW(list, LB_SETITEMDATA, row, 0);
    }
    for (const auto index : picker.order) {
        const auto& location = prayerLocations[index - 1];
        if (!contains(location.name, search) && !contains(location.province, search) && !contains(location.zone(), search)) continue;
        const auto row = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(location.name));
        if (row >= 0) {
            SendMessageW(list, LB_SETITEMDATA, row, index);
            if (!search[0] && index == picker.current) SendMessageW(list, LB_SETCURSEL, row, 0);
        }
    }
    const auto count = SendMessageW(list, LB_GETCOUNT, 0, 0);
    const bool available = count > 0;
    const std::wstring status = !available ? L"No matching locations" :
        search[0] ? std::to_wstring(count) + (count == 1 ? L" result" : L" results") :
        std::to_wstring(prayerLocations.size()) + L" locations";
    SetDlgItemTextW(window, IDC_PRAYER_STATUS, status.c_str());
    if (available && SendMessageW(list, LB_GETCURSEL, 0, 0) == LB_ERR)
        SendMessageW(list, LB_SETCURSEL, 0, 0);
    EnableWindow(GetDlgItem(window, IDOK), available);
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, TRUE);
}

INT_PTR CALLBACK pickerDialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* picker = reinterpret_cast<Picker*>(GetWindowLongPtrW(window, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG: {
        picker = reinterpret_cast<Picker*>(lparam);
        SetWindowLongPtrW(window, DWLP_USER, lparam);
        const BOOL dark = TRUE;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        SendDlgItemMessageW(window, IDC_PRAYER_SEARCH, EM_SETLIMITTEXT, 95, 0);
        SendDlgItemMessageW(window, IDC_PRAYER_SEARCH, EM_SETCUEBANNER, TRUE,
                            reinterpret_cast<LPARAM>(L"e.g. Bandung, Aceh, WITA"));
        SendMessageW(window, DM_SETDEFID, IDOK, 0);
        updateList(window, *picker);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        drawSearch(window, dc);
        EndPaint(window, &paint);
        return TRUE;
    }
    case WM_CTLCOLORDLG:
        if (picker) return reinterpret_cast<INT_PTR>(picker->background);
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (picker) {
            const auto dc = reinterpret_cast<HDC>(wparam);
            const bool label = message == WM_CTLCOLORSTATIC;
            const auto control = reinterpret_cast<HWND>(lparam);
            const bool results = message == WM_CTLCOLORLISTBOX;
            SetTextColor(dc, label && GetDlgCtrlID(control) >= IDC_PRAYER_STATUS ? mutedColor : foregroundColor);
            const bool focused = !label && !results && GetFocus() == control;
            SetBkColor(dc, label || results ? backgroundColor : focused ? selectedColor : fieldColor);
            return reinterpret_cast<INT_PTR>(label || results ? picker->background : focused ? picker->focusedField : picker->field);
        }
        break;
    case WM_MEASUREITEM:
        if (wparam == IDC_PRAYER_RESULTS) {
            reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->itemHeight = MulDiv(42, GetDpiForWindow(window), 96);
            return TRUE;
        }
        break;
    case WM_DRAWITEM: {
        const auto& item = *reinterpret_cast<DRAWITEMSTRUCT const*>(lparam);
        if (!picker) break;
        if (item.CtlID == IDOK || item.CtlID == IDCANCEL) {
            drawAction(window, item, picker->background);
            return TRUE;
        }
        if (item.CtlID != IDC_PRAYER_RESULTS || item.itemID == static_cast<UINT>(-1)) break;
        const auto index = static_cast<size_t>(item.itemData);
        const bool selected = (item.itemState & ODS_SELECTED) != 0;
        FillRect(item.hDC, &item.rcItem, picker->background);
        auto tile = item.rcItem;
        InflateRect(&tile, -px(window, 2), -px(window, 2));
        const bool focused = (item.itemState & ODS_FOCUS) != 0;
        if (selected || focused) rounded(item.hDC, tile, px(window, 12), focused ? focusColor : selectedColor);
        SetBkMode(item.hDC, TRANSPARENT);
        auto label = item.rcItem;
        InflateRect(&label, -MulDiv(10, GetDpiForWindow(window), 96), 0);
        auto name = index ? std::wstring(prayerLocations[index - 1].name) : L"Off";
        if (index == picker->current) {
            RECT mark = label;
            mark.right = mark.left + px(window, 18);
            SetTextColor(item.hDC, accentColor);
            DrawTextW(item.hDC, L"\u2713", -1, &mark, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            label.left = mark.right;
        } else {
            label.left += px(window, 18);
        }
        SetTextColor(item.hDC, foregroundColor);
        auto city = label;
        if (index) {
            city.right -= MulDiv(54, GetDpiForWindow(window), 96);
            city.bottom = city.top + MulDiv(24, GetDpiForWindow(window), 96);
        }
        DrawTextW(item.hDC, name.c_str(), -1, &city, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (index) {
            SetTextColor(item.hDC, accentColor);
            DrawTextW(item.hDC, prayerLocations[index - 1].zone(), -1, &label, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
            city.top = city.bottom - MulDiv(2, GetDpiForWindow(window), 96);
            city.bottom = label.bottom;
            SetTextColor(item.hDC, mutedColor);
            DrawTextW(item.hDC, prayerLocations[index - 1].province, -1, &city,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_PRAYER_SEARCH && (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == EN_KILLFOCUS)) {
            InvalidateRect(window, nullptr, FALSE);
            InvalidateRect(GetDlgItem(window, IDC_PRAYER_SEARCH), nullptr, TRUE);
        }
        if (LOWORD(wparam) == IDC_PRAYER_SEARCH && HIWORD(wparam) == EN_CHANGE && picker) {
            updateList(window, *picker);
            return TRUE;
        }
        if (LOWORD(wparam) == IDOK || (LOWORD(wparam) == IDC_PRAYER_RESULTS && HIWORD(wparam) == LBN_DBLCLK)) {
            const auto list = GetDlgItem(window, IDC_PRAYER_RESULTS);
            const auto row = SendMessageW(list, LB_GETCURSEL, 0, 0);
            if (row != LB_ERR) EndDialog(window, SendMessageW(list, LB_GETITEMDATA, row, 0) + 1);
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL) {
            EndDialog(window, 0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(window, 0);
        return TRUE;
    }
    return FALSE;
}

}

std::optional<unsigned> choosePrayerLocation(HWND owner, unsigned current) {
    Picker picker{current, std::vector<unsigned>(prayerLocations.size())};
    std::iota(picker.order.begin(), picker.order.end(), 1u);
    const auto name = [](unsigned id) {
        std::wstring_view label = prayerLocations[id - 1].name;
        if (label.starts_with(L"Kota ") || label.starts_with(L"Kab. ")) label.remove_prefix(5);
        return label;
    };
    std::stable_sort(picker.order.begin(), picker.order.end(), [&](unsigned a, unsigned b) { return name(a) < name(b); });
    const auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_PRAYER_LOCATION), owner,
                                        pickerDialog, reinterpret_cast<LPARAM>(&picker));
    if (result <= 0 || result > static_cast<INT_PTR>(prayerLocations.size() + 1)) return std::nullopt;
    return static_cast<unsigned>(result - 1);
}

}
