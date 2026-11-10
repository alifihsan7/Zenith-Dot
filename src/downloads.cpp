#include "downloads.hpp"

#include <shlobj.h>

namespace zenith {
namespace {

constexpr std::wstring_view partialSuffix = L".crdownload";

bool isPartial(std::wstring_view name) {
    if (name.size() <= partialSuffix.size()) return false;
    name.remove_prefix(name.size() - partialSuffix.size());
    return CompareStringOrdinal(name.data(), static_cast<int>(name.size()),
                                partialSuffix.data(), static_cast<int>(partialSuffix.size()), TRUE) == CSTR_EQUAL;
}

void CALLBACK reportChange(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WAIT, TP_WAIT_RESULT) {
    PostMessageW(static_cast<HWND>(context), DownloadMonitor::changed, 0, 0);
}

}

DownloadMonitor::~DownloadMonitor() {
    stop();
}

void DownloadMonitor::start(HWND window, std::wstring folder) {
    stop();
    if (folder.empty()) {
        PWSTR path = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) return;
        folder = path;
        CoTaskMemFree(path);
    }
    directory_ = CreateFileW(folder.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (directory_ == INVALID_HANDLE_VALUE) return;
    folder_ = std::move(folder);
    if (!folder_.ends_with(L'\\')) folder_ += L'\\';
    read_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    wait_ = CreateThreadpoolWait(reportChange, window, nullptr);
    if (!read_.hEvent || !wait_ || !arm()) stop();
}

void DownloadMonitor::stop() {
    if (wait_) {
        SetThreadpoolWait(wait_, nullptr, nullptr);
        WaitForThreadpoolWaitCallbacks(wait_, TRUE);
        CloseThreadpoolWait(wait_);
        wait_ = nullptr;
    }
    if (directory_ != INVALID_HANDLE_VALUE) {
        if (read_.hEvent) {
            CancelIoEx(directory_, &read_);
            DWORD ignored = 0;
            GetOverlappedResult(directory_, &read_, &ignored, TRUE);
        }
        CloseHandle(directory_);
        directory_ = INVALID_HANDLE_VALUE;
    }
    if (read_.hEvent) CloseHandle(read_.hEvent);
    read_ = {};
    pending_.clear();
    renamedFrom_.clear();
    clearCompleted();
}

bool DownloadMonitor::arm() {
    ResetEvent(read_.hEvent);
    if (!ReadDirectoryChangesW(directory_, changes_.data(), static_cast<DWORD>(changes_.size()), FALSE,
                              FILE_NOTIFY_CHANGE_FILE_NAME, nullptr, &read_, nullptr)) return false;
    SetThreadpoolWait(wait_, read_.hEvent, nullptr);
    return true;
}

bool DownloadMonitor::readChanges() {
    if (!read_.hEvent || WaitForSingleObject(read_.hEvent, 0) != WAIT_OBJECT_0) return false;
    DWORD bytes = 0;
    if (!GetOverlappedResult(directory_, &read_, &bytes, FALSE) && GetLastError() != ERROR_NOTIFY_ENUM_DIR) {
        stop();
        return true;
    }
    bool updated = false;
    if (!bytes) {
        // Lost rename pairs cannot establish which files finished.
        pending_.clear();
        renamedFrom_.clear();
        clearCompleted();
        updated = true;
    }
    for (size_t offset = 0; offset < bytes;) {
        const auto& event = *reinterpret_cast<FILE_NOTIFY_INFORMATION const*>(changes_.data() + offset);
        updated |= apply(event.Action, {event.FileName, event.FileNameLength / sizeof(wchar_t)});
        if (!event.NextEntryOffset) break;
        offset += event.NextEntryOffset;
    }
    if (!arm()) {
        stop();
        return true;
    }
    return updated;
}

bool DownloadMonitor::apply(DWORD action, std::wstring_view name) {
    if (action == FILE_ACTION_RENAMED_OLD_NAME) {
        renamedFrom_.clear();
        if (pending_.contains(name)) renamedFrom_ = name;
        return false;
    }
    const bool renamed = action == FILE_ACTION_RENAMED_NEW_NAME && !renamedFrom_.empty();
    if (renamed) pending_.erase(renamedFrom_);
    renamedFrom_.clear();
    if (action == FILE_ACTION_REMOVED) {
        const auto entry = pending_.find(name);
        if (entry == pending_.end()) return false;
        pending_.erase(entry);
        return true;
    }
    if (action != FILE_ACTION_ADDED && action != FILE_ACTION_RENAMED_NEW_NAME) return false;
    if (isPartial(name)) {
        if (pending_.empty() && !renamed) clearCompleted();
        return pending_.emplace(name).second || renamed;
    }
    if (!renamed) return false;
    completedName_ = name;
    ++completedCount_;
    return true;
}

std::wstring_view DownloadMonitor::pendingName() const {
    if (pending_.size() != 1) return {};
    return std::wstring_view(*pending_.begin()).substr(0, pending_.begin()->size() - partialSuffix.size());
}

void DownloadMonitor::clearCompleted() {
    completedName_.clear();
    completedCount_ = 0;
}

void DownloadMonitor::revealCompleted() const {
    if (!completedCount()) return;
    const auto path = folder_ + completedName_;
    PIDLIST_ABSOLUTE item = nullptr;
    if (SUCCEEDED(SHParseDisplayName(path.c_str(), nullptr, &item, 0, nullptr))) {
        SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
        CoTaskMemFree(item);
    }
}

}
