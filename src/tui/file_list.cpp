#include "diffsync/tui/file_list.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace diffsync {

namespace {

std::string formatBytes(uint64_t size) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double dsize = static_cast<double>(size);

    while (dsize >= 1024.0 && unit < 3) {
        dsize /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << dsize << units[unit];
    return oss.str();
}

} // anonymous namespace

FileList::FileList(const std::vector<ChangeFile>& files)
    : files_(files) {
    updateEntries();
}

void FileList::updateEntries() {
    entries_.clear();
    for (const auto& file : files_) {
        std::ostringstream oss;
        oss << (file.selected ? "[x]" : "[ ]") << " "
            << getStatusDisplay(file.status) << " "
            << std::setw(10) << formatSize(file.size) << "  "
            << file.path.string();
        entries_.push_back(oss.str());
    }
}

ftxui::Component FileList::operator()() {
    using namespace ftxui;

    // Create menu with custom style - entries_ is a member variable
    auto menu = Menu(&entries_, &focusedIndex_);

    // Handle keyboard input
    menu |= CatchEvent([this](Event event) {
        if (event == Event::Character(' ')) {
            toggleSelection();
            return true;
        }
        return false;
    });

    return menu;
}

void FileList::updateFiles(const std::vector<ChangeFile>& files) {
    files_ = files;
    focusedIndex_ = 0;
    updateEntries();
}

void FileList::toggleSelection() {
    if (files_.empty()) return;

    size_t index = static_cast<size_t>(focusedIndex_);
    if (index < files_.size()) {
        files_[index].selected = !files_[index].selected;
        updateEntries();  // Update display entries
        notifyChange();
    }
}

void FileList::selectAll() {
    for (auto& file : files_) {
        file.selected = true;
    }
    updateEntries();  // Update display entries
    notifyChange();
}

void FileList::unselectAll() {
    for (auto& file : files_) {
        file.selected = false;
    }
    updateEntries();  // Update display entries
    notifyChange();
}

std::vector<size_t> FileList::getSelectedIndices() const {
    std::vector<size_t> indices;
    for (size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].selected) {
            indices.push_back(i);
        }
    }
    return indices;
}

std::vector<ChangeFile> FileList::getSelectedFiles() const {
    std::vector<ChangeFile> selected;
    for (const auto& file : files_) {
        if (file.selected) {
            selected.push_back(file);
        }
    }
    return selected;
}

size_t FileList::getSelectedCount() const {
    return std::count_if(files_.begin(), files_.end(),
                        [](const ChangeFile& f) { return f.selected; });
}

uint64_t FileList::getSelectedSize() const {
    uint64_t total = 0;
    for (const auto& file : files_) {
        if (file.selected) {
            total += file.size;
        }
    }
    return total;
}

void FileList::setFocusedIndex(int index) {
    if (index >= 0 && index < static_cast<int>(files_.size())) {
        focusedIndex_ = index;
    }
}

void FileList::notifyChange() {
    if (onChangeCallback_) {
        onChangeCallback_(files_);
    }
}

std::string FileList::formatSize(uint64_t size) {
    return formatBytes(size);
}

std::string FileList::getStatusDisplay(FileStatus status) {
    switch (status) {
        case FileStatus::Modified: return "M";
        case FileStatus::Staged:  return "M ";
        case FileStatus::Added:   return "A";
        case FileStatus::Untracked: return "??";
        case FileStatus::Deleted: return "D";
        default:                  return "?";
    }
}

} // namespace diffsync
