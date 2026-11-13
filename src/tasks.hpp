#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace zenith {

struct Task {
    std::wstring text;
    bool done = false;
};

struct TaskView {
    std::vector<Task> items;
    bool saveFailed = false;
};

// The Tasks list next to the Scratchpad note, kept in its own file as "[ ] text" / "[x] text" lines.
class TaskList {
public:
    static constexpr size_t maxTasks = 200;
    static constexpr size_t maxLength = 240;

    void load();
    bool add(std::wstring text);
    bool toggle(size_t index);
    bool remove(size_t index);
    bool clearDone();
    TaskView const& view() const { return view_; }

private:
    void save();

    TaskView view_;
    std::filesystem::path file_;
};

}
