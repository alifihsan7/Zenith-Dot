#include "tasks.hpp"

#include <windows.h>
#include <shlobj.h>

#include <winrt/base.h>

#include <algorithm>
#include <fstream>
#include <memory>

namespace zenith {

namespace {

std::wstring trimmed(std::wstring text) {
    std::ranges::replace_if(text, [](wchar_t c) { return c == L'\r' || c == L'\n' || c == L'\t'; }, L' ');
    const auto first = text.find_first_not_of(L' ');
    if (first == std::wstring::npos) return {};
    const auto last = text.find_last_not_of(L' ');
    return text.substr(first, last - first + 1);
}

}

void TaskList::load() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder))) return;
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> release(folder, CoTaskMemFree);
    file_ = std::filesystem::path(folder) / L"Zenith Dot" / L"tasks.txt";
    std::ifstream input(file_, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (size <= 0 || size > 1'000'000) return;
    std::string bytes(static_cast<size_t>(size), '\0');
    if (!input.seekg(0).read(bytes.data(), size)) return;
    std::wstring text;
    try {
        text = std::wstring(winrt::to_hstring(bytes));
    } catch (winrt::hresult_error const&) {
        view_.saveFailed = true;
        return;
    }
    for (size_t start = 0; start < text.size() && view_.items.size() < maxTasks;) {
        const auto newline = text.find(L'\n', start);
        auto line = text.substr(start, newline == std::wstring::npos ? std::wstring::npos : newline - start);
        start = newline == std::wstring::npos ? text.size() : newline + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        bool done = false;
        if (line.size() >= 4 && line[0] == L'[' && line[2] == L']' && line[3] == L' ') {
            done = line[1] == L'x' || line[1] == L'X';
            line.erase(0, 4);
        }
        line = trimmed(std::move(line));
        if (!line.empty()) view_.items.push_back({line.substr(0, maxLength), done});
    }
}

bool TaskList::add(std::wstring text) {
    text = trimmed(std::move(text));
    if (text.empty() || view_.items.size() >= maxTasks) return false;
    view_.items.push_back({text.substr(0, maxLength), false});
    save();
    return true;
}

bool TaskList::toggle(size_t index) {
    if (index >= view_.items.size()) return false;
    view_.items[index].done = !view_.items[index].done;
    save();
    return true;
}

bool TaskList::remove(size_t index) {
    if (index >= view_.items.size()) return false;
    view_.items.erase(view_.items.begin() + static_cast<std::ptrdiff_t>(index));
    save();
    return true;
}

bool TaskList::clearDone() {
    const auto removed = std::erase_if(view_.items, [](Task const& task) { return task.done; });
    if (!removed) return false;
    save();
    return true;
}

void TaskList::save() {
    view_.saveFailed = true;
    if (file_.empty()) return;
    std::wstring text;
    for (const auto& task : view_.items) {
        text += task.done ? L"[x] " : L"[ ] ";
        text += task.text;
        text += L"\r\n";
    }
    std::error_code error;
    std::filesystem::create_directories(file_.parent_path(), error);
    if (error) return;
    auto pending = file_;
    pending += L".tmp";
    try {
        const auto bytes = winrt::to_string(text);
        std::ofstream output(pending, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (output && MoveFileExW(pending.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            view_.saveFailed = false;
        }
    } catch (winrt::hresult_error const&) {
        // The list stays in memory; the next change tries to save again.
    }
}

}
