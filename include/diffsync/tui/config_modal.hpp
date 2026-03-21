#pragma once

#include "diffsync/core/config.hpp"
#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

namespace diffsync {

/**
 * ConfigModal - Modal dialog for managing remote configurations
 *
 * Features:
 * - List view: Display, select, delete configurations
 * - Edit view: Add/edit configurations with form input
 * - Keyboard shortcuts for all operations
 */
class ConfigModal {
public:
    enum class Mode { List, Edit };

    /**
     * Create config modal
     * @param config Config manager (non-const to allow modifications)
     */
    explicit ConfigModal(ConfigManager& config);

    /**
     * Show the modal in list mode
     */
    void show();

    /**
     * Hide the modal
     */
    void hide();

    /**
     * Check if modal is currently shown
     */
    bool isShown() const { return shown_; }

    /**
     * Get the FTXUI component
     * @param externalShown Pointer to external shown state for Modal (optional)
     */
    ftxui::Component operator()(bool* externalShown = nullptr);

    /**
     * Refresh data from config manager
     * Call this after external config changes
     */
    void refresh();

private:
    ConfigManager& config_;
    bool shown_ = false;
    Mode mode_ = Mode::List;
    int modeIndex_ = 0;  // For Container::Tab - must stay alive

    // List view state - MUST be members to avoid dangling references
    std::vector<RemoteConfig> configs_;
    int listFocusedIndex_ = 0;
    std::vector<std::string> listEntries_;

    // Edit view state - MUST be members
    std::string editName_;
    std::string editHost_;
    std::string editUser_;
    std::string editPath_;
    std::string editPort_;
    std::string editKey_;
    std::string editPassword_;
    std::string errorMessage_;
    bool isEditing_ = false;  // false=adding, true=editing existing

    // Component storage - MUST be members to keep alive
    ftxui::Component inputName_;
    ftxui::Component inputHost_;
    ftxui::Component inputUser_;
    ftxui::Component inputPath_;
    ftxui::Component inputPort_;
    ftxui::Component inputKey_;
    ftxui::Component inputPassword_;
    ftxui::Component editContainer_;

    /**
     * Set external shown state pointer
     */
    void setExternalShown(bool* ptr) { externalShown_ = ptr; }

    // For external state synchronization
    bool* externalShown_ = nullptr;

    /**
     * Switch to add mode (blank form)
     */
    void startAdd();

    /**
     * Switch to edit mode (pre-filled form)
     * @param index Index in configs_ to edit
     */
    void startEdit(int index);

    /**
     * Save the current form data
     */
    void saveEdit();

    /**
     * Cancel edit mode, return to list
     */
    void cancelEdit();

    /**
     * Delete configuration at index
     * @param index Index in configs_ to delete
     */
    void deleteConfig(int index);

    /**
     * Select configuration at index as current
     * @param index Index in configs_ to select
     */
    void selectConfig(int index);

    /**
     * Validate form inputs
     * @return Error message if invalid, empty if valid
     */
    std::string validateInputs() const;

    /**
     * Update list entries display
     */
    void updateListEntries();
};

} // namespace diffsync
