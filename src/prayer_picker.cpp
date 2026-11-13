#include "prayer.hpp"
#include "resource.h"

#include <dwmapi.h>
#include <algorithm>
#include <numeric>
#include <vector>

namespace zenith {
namespace {

struct Picker {
    unsigned current;
    std::vector<unsigned> order;
    HBRUSH background = CreateSolidBrush(RGB(17, 20, 25));
    HBRUSH field = CreateSolidBrush(RGB(24, 28, 34));

    ~Picker() {
        DeleteObject(background);
        DeleteObject(field);
    }
};

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
    const bool available = SendMessageW(list, LB_GETCOUNT, 0, 0) > 0;
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
        updateList(window, *picker);
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
            SetTextColor(dc, RGB(240, 244, 250));
            SetBkColor(dc, label ? RGB(17, 20, 25) : RGB(24, 28, 34));
            return reinterpret_cast<INT_PTR>(label ? picker->background : picker->field);
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
        if (!picker || item.CtlID != IDC_PRAYER_RESULTS || item.itemID == static_cast<UINT>(-1)) break;
        const auto index = static_cast<size_t>(item.itemData);
        const bool selected = (item.itemState & ODS_SELECTED) != 0;
        const auto brush = CreateSolidBrush(selected ? RGB(46, 63, 81) : RGB(24, 28, 34));
        FillRect(item.hDC, &item.rcItem, brush);
        DeleteObject(brush);
        SetBkMode(item.hDC, TRANSPARENT);
        auto label = item.rcItem;
        InflateRect(&label, -MulDiv(10, GetDpiForWindow(window), 96), 0);
        auto name = index ? std::wstring(prayerLocations[index - 1].name) : L"Off";
        if (index == picker->current) name.insert(0, L"\u2713  ");
        SetTextColor(item.hDC, RGB(240, 244, 250));
        auto city = label;
        if (index) {
            city.right -= MulDiv(54, GetDpiForWindow(window), 96);
            city.bottom = city.top + MulDiv(24, GetDpiForWindow(window), 96);
        }
        DrawTextW(item.hDC, name.c_str(), -1, &city, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (index) {
            SetTextColor(item.hDC, RGB(173, 198, 224));
            DrawTextW(item.hDC, prayerLocations[index - 1].zone(), -1, &label, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
            city.top = city.bottom - MulDiv(2, GetDpiForWindow(window), 96);
            city.bottom = label.bottom;
            DrawTextW(item.hDC, prayerLocations[index - 1].province, -1, &city,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        if (item.itemState & ODS_FOCUS) DrawFocusRect(item.hDC, &item.rcItem);
        return TRUE;
    }
    case WM_COMMAND:
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
