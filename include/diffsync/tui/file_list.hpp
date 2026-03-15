#pragma once

#include "diffsync/types.hpp"
#include <ftxui/component/component.hpp>
#include <vector>
#include <functional>
#include <memory>

namespace diffsync {

/**
 * FileList - TUI component for displaying and selecting files
 *
 * Features:
 * - Display files with status, size, and path
 * - Navigate with arrow keys
 * - Toggle selection with Space
 * - Select all / Unselect all
 */
class FileList {
public:
    /**
     * Callback type for when file list changes
     */
    using OnChangeCallback = std::function<void(const std::vector<ChangeFile>&)>;

    /**
     * Create file list component
     * @param files Initial list of files
     */
    explicit FileList(const std::vector<ChangeFile>& files = {});

    /**
     * Get FTXUI component
     */
    ftxui::Component operator()();

    /**
     * Update files in the list
     */
    void updateFiles(const std::vector<ChangeFile>& files);

    /**
     * Toggle selection for focused file
     */
    void toggleSelection();

    /**
     * Select all files
     */
    void selectAll();

    /**
     * Unselect all files
     */
    void unselectAll();

    /**
     * Get indices of selected files
     */
    std::vector<size_t> getSelectedIndices() const;

    /**
     * Get selected files
     */
    std::vector<ChangeFile> getSelectedFiles() const;

    /**
     * Get all files
     */
    const std::vector<ChangeFile>& getFiles() const { return files_; }

    /**
     * Get count of selected files
     */
    size_t getSelectedCount() const;

    /**
     * Get total size of selected files
     */
    uint64_t getSelectedSize() const;

    /**
     * Set callback for when selection changes
     */
    void setOnChangeCallback(OnChangeCallback callback) {
        onChangeCallback_ = std::move(callback);
    }

    /**
     * Get currently focused index
     */
    int getFocusedIndex() const { return focusedIndex_; }

    /**
     * Set focused index
     */
    void setFocusedIndex(int index);

private:
    std::vector<ChangeFile> files_;
    int focusedIndex_ = 0;
    OnChangeCallback onChangeCallback_;

    /**
     * Notify callback of changes
     */
    void notifyChange();

    /**
     * Format file size for display
     */
    static std::string formatSize(uint64_t size);

    /**
     * Get display string for file status
     */
    static std::string getStatusDisplay(FileStatus status);
};

} // namespace diffsync
