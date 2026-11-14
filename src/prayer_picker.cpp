#include "prayer.hpp"
#include "resource.h"

#include <dwmapi.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <cwctype>
#include <string>
#include <numeric>
#include <optional>
#include <vector>

namespace zenith {
namespace {

#pragma comment(lib, "uxtheme.lib")

// Same graphite palette as the Settings window.
constexpr COLORREF backgroundColor = RGB(14, 15, 17);
constexpr COLORREF cardColor = RGB(26, 27, 30);
constexpr COLORREF hoverColor = RGB(34, 35, 39);
constexpr COLORREF pressColor = RGB(46, 47, 52);
constexpr COLORREF lineColor = RGB(38, 39, 43);
constexpr COLORREF chipColor = RGB(38, 50, 63);
constexpr COLORREF foregroundColor = RGB(245, 246, 248);
constexpr COLORREF mutedColor = RGB(164, 170, 180);
constexpr COLORREF accentColor = RGB(145, 202, 255);

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

constexpr wchar_t hoverProperty[] = L"Zenula.HoverRow";

// The row under the pointer, stored on the list as index + 1 (0 means none).
int hoveredRow(HWND list) {
    return static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(list, hoverProperty))) - 1;
}

void redrawRow(HWND list, int row) {
    RECT area{};
    if (row >= 0 && SendMessageW(list, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&area)) != LB_ERR)
        InvalidateRect(list, &area, FALSE);
}

void hover(HWND list, std::optional<POINT> client) {
    int row = -1;
    if (client) {
        const auto hit = SendMessageW(list, LB_ITEMFROMPOINT, 0, MAKELPARAM(client->x, client->y));
        if (HIWORD(hit) == 0) row = LOWORD(hit);
    }
    const int previous = hoveredRow(list);
    if (row == previous) return;
    SetPropW(list, hoverProperty, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(row + 1)));
    redrawRow(list, previous);
    redrawRow(list, row);
}

// A single click on a row chooses it; the dialog does not need a separate Select button.
LRESULT CALLBACK resultsInput(HWND list, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    if (message == WM_NCDESTROY) RemovePropW(list, hoverProperty);
    const auto result = DefSubclassProc(list, message, wparam, lparam);
    if (message == WM_LBUTTONUP) {
        const auto hit = SendMessageW(list, LB_ITEMFROMPOINT, 0, lparam);
        if (HIWORD(hit) == 0) PostMessageW(GetParent(list), WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
    }
    if (message == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT leave{sizeof(leave), TME_LEAVE, list, 0};
        TrackMouseEvent(&leave);
        hover(list, POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
    }
    if (message == WM_MOUSELEAVE) hover(list, std::nullopt);
    // Scrolling moves a different row under a pointer that stays still.
    if (message == WM_MOUSEWHEEL || message == WM_VSCROLL) {
        POINT pointer{};
        GetCursorPos(&pointer);
        RECT client{};
        GetClientRect(list, &client);
        ScreenToClient(list, &pointer);
        hover(list, PtInRect(&client, pointer) ? std::optional<POINT>(pointer) : std::nullopt);
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS) InvalidateRect(list, nullptr, FALSE);
    return result;
}

int px(HWND window, int value) {
    return MulDiv(value, GetDpiForWindow(window), 96);
}

struct Picker {
    unsigned current;
    std::vector<unsigned> order;
    HBRUSH background = CreateSolidBrush(backgroundColor);
    HBRUSH card = CreateSolidBrush(cardColor);
    HFONT name{};
    HFONT detail{};

    void fonts(HWND window) {
        if (name) DeleteObject(name);
        if (detail) DeleteObject(detail);
        const auto font = [&](int size, int weight) {
            return CreateFontW(-px(window, size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        };
        name = font(13, FW_SEMIBOLD);
        detail = font(12, FW_NORMAL);
    }

    ~Picker() {
        DeleteObject(background);
        DeleteObject(card);
        if (name) DeleteObject(name);
        if (detail) DeleteObject(detail);
    }
};

void rounded(HDC dc, RECT rect, int radius, COLORREF fill) {
    const auto brush = SelectObject(dc, GetStockObject(DC_BRUSH));
    const auto pen = SelectObject(dc, GetStockObject(NULL_PEN));
    SetDCBrushColor(dc, fill);
    RoundRect(dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1, radius, radius);
    SelectObject(dc, pen);
    SelectObject(dc, brush);
}

void ring(HDC dc, RECT rect, int radius, int width, COLORREF ink) {
    const auto pen = CreatePen(PS_INSIDEFRAME, width, ink);
    const auto previousPen = SelectObject(dc, pen);
    const auto brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, brush);
    SelectObject(dc, previousPen);
    DeleteObject(pen);
}

RECT bounds(HWND window, int id) {
    RECT rect{};
    GetWindowRect(GetDlgItem(window, id), &rect);
    MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&rect), 2);
    return rect;
}

RECT searchField(HWND window) {
    auto field = bounds(window, IDC_PRAYER_SEARCH);
    field.left -= px(window, 30);
    field.right += px(window, 10);
    const int middle = (field.top + field.bottom) / 2;
    field.top = middle - px(window, 18);
    field.bottom = middle + px(window, 18);
    return field;
}

void paintDialog(HWND window, HDC dc) {
    RECT client{};
    GetClientRect(window, &client);
    const auto fill = CreateSolidBrush(backgroundColor);
    FillRect(dc, &client, fill);
    DeleteObject(fill);

    const auto edit = GetDlgItem(window, IDC_PRAYER_SEARCH);
    const auto field = searchField(window);
    const bool focused = GetFocus() == edit;
    rounded(dc, field, px(window, 16), cardColor);
    if (focused) ring(dc, field, px(window, 16), px(window, 2), accentColor);

    // Magnifier
    const int x = field.left + px(window, 17);
    const int y = (field.top + field.bottom) / 2 - px(window, 1);
    const int r = px(window, 5);
    const auto pen = CreatePen(PS_SOLID, std::max(1, px(window, 1)), focused ? accentColor : mutedColor);
    const auto previousPen = SelectObject(dc, pen);
    const auto brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, x - r, y - r, x + r + 1, y + r + 1);
    MoveToEx(dc, x + r - 1, y + r - 1, nullptr);
    LineTo(dc, x + px(window, 8), y + px(window, 8));
    SelectObject(dc, brush);
    SelectObject(dc, previousPen);
    DeleteObject(pen);

    auto list = bounds(window, IDC_PRAYER_RESULTS);
    InflateRect(&list, px(window, 4), px(window, 4));
    rounded(dc, list, px(window, 20), cardColor);

    const auto cancel = bounds(window, IDCANCEL);
    const RECT line{0, cancel.top - px(window, 12), client.right, cancel.top - px(window, 11)};
    const auto hairline = CreateSolidBrush(lineColor);
    FillRect(dc, &line, hairline);
    DeleteObject(hairline);
}

void drawAction(HWND window, DRAWITEMSTRUCT const& item, HBRUSH background) {
    FillRect(item.hDC, &item.rcItem, background);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0 && !(item.itemState & ODS_NOFOCUSRECT);
    POINT pointer{};
    GetCursorPos(&pointer);
    const bool hot = WindowFromPoint(pointer) == item.hwndItem;
    rounded(item.hDC, item.rcItem, px(window, 12), pressed || hot ? pressColor : hoverColor);
    if (focused) ring(item.hDC, item.rcItem, px(window, 12), px(window, 2), accentColor);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, foregroundColor);
    RECT label = item.rcItem;
    DrawTextW(item.hDC, L"Cancel", -1, &label, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
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
    RemovePropW(list, hoverProperty);
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
        picker->fonts(window);
        const BOOL dark = TRUE;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        if (const auto icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZENITH_DOT)))
            SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        SendDlgItemMessageW(window, IDC_PRAYER_SEARCH, EM_SETLIMITTEXT, 95, 0);
        SendDlgItemMessageW(window, IDC_PRAYER_SEARCH, EM_SETCUEBANNER, TRUE,
                            reinterpret_cast<LPARAM>(L"Search city, province, or WIB / WITA / WIT"));
        SendMessageW(window, DM_SETDEFID, IDOK, 0);
        // Dark scrollbar for the results, and a click selects; Enter still picks the highlighted row.
        const auto list = GetDlgItem(window, IDC_PRAYER_RESULTS);
        SetWindowTheme(list, L"DarkMode_Explorer", nullptr);
        SetWindowSubclass(list, resultsInput, 1, 0);
        updateList(window, *picker);
        return TRUE;
    }
    case WM_DPICHANGED:
        if (picker) picker->fonts(window);
        InvalidateRect(window, nullptr, TRUE);
        break;
    case WM_ERASEBKGND:
        SetWindowLongPtrW(window, DWLP_MSGRESULT, 1);
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        paintDialog(window, dc);
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
            SetTextColor(dc, label ? mutedColor : foregroundColor);
            SetBkColor(dc, label ? backgroundColor : cardColor);
            return reinterpret_cast<INT_PTR>(label ? picker->background : picker->card);
        }
        break;
    case WM_MEASUREITEM:
        if (wparam == IDC_PRAYER_RESULTS) {
            reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->itemHeight = px(window, 46);
            return TRUE;
        }
        break;
    case WM_DRAWITEM: {
        const auto& item = *reinterpret_cast<DRAWITEMSTRUCT const*>(lparam);
        if (!picker) break;
        if (item.CtlID == IDCANCEL) {
            drawAction(window, item, picker->background);
            return TRUE;
        }
        if (item.CtlID != IDC_PRAYER_RESULTS || item.itemID == static_cast<UINT>(-1)) break;
        const auto index = static_cast<size_t>(item.itemData);
        const bool current = index == picker->current;
        const bool selected = (item.itemState & ODS_SELECTED) != 0;
        const bool focused = selected && GetFocus() == item.hwndItem;
        FillRect(item.hDC, &item.rcItem, picker->card);
        if (item.itemID > 0) {
            const RECT line{item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.top + 1};
            const auto hairline = CreateSolidBrush(lineColor);
            FillRect(item.hDC, &line, hairline);
            DeleteObject(hairline);
        }
        auto tile = item.rcItem;
        InflateRect(&tile, -px(window, 2), -px(window, 2));
        // Current location: the blue chip. Keyboard row (Enter picks it): a step lighter than the pointer's row.
        const bool hot = static_cast<int>(item.itemID) == hoveredRow(item.hwndItem);
        if (current || selected || hot)
            rounded(item.hDC, tile, px(window, 12), current ? chipColor : selected ? pressColor : hoverColor);
        if (focused) ring(item.hDC, tile, px(window, 12), px(window, 2), accentColor);
        SetBkMode(item.hDC, TRANSPARENT);
        auto label = item.rcItem;
        InflateRect(&label, -px(window, 12), 0);
        if (current) {
            RECT mark = label;
            mark.right = mark.left + px(window, 18);
            SetTextColor(item.hDC, accentColor);
            DrawTextW(item.hDC, L"✓", -1, &mark, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }
        label.left += px(window, 22);
        const auto previousFont = SelectObject(item.hDC, picker->name);
        auto name = label;
        name.right -= px(window, 48);
        name.top += px(window, 5);
        name.bottom = (item.rcItem.top + item.rcItem.bottom) / 2 + px(window, 2);
        SetTextColor(item.hDC, foregroundColor);
        DrawTextW(item.hDC, index ? prayerLocations[index - 1].name : L"Off", -1, &name,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(item.hDC, picker->detail);
        SetTextColor(item.hDC, mutedColor);
        auto second = name;
        second.top = name.bottom - px(window, 1);
        second.bottom = item.rcItem.bottom - px(window, 5);
        const auto province = index ? titleCase(prayerLocations[index - 1].province) : std::wstring(L"Hide prayer times");
        DrawTextW(item.hDC, province.c_str(), -1, &second, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (index) DrawTextW(item.hDC, prayerLocations[index - 1].zone(), -1, &label, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(item.hDC, previousFont);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_PRAYER_SEARCH && (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == EN_KILLFOCUS)) {
            InvalidateRect(window, nullptr, FALSE);
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
