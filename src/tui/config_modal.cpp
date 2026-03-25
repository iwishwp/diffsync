#include "diffsync/tui/config_modal.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <algorithm>

namespace diffsync {

ConfigModal::ConfigModal(ConfigManager& config)
    : config_(config), modeIndex_(0) {

    // Create input components (must be done in constructor)
    inputName_ = ftxui::Input(&editName_, "profile name");
    inputHost_ = ftxui::Input(&editHost_, "hostname or IP");
    inputUser_ = ftxui::Input(&editUser_, "username");
    inputPath_ = ftxui::Input(&editPath_, "/remote/path");
    inputPort_ = ftxui::Input(&editPort_, "22 (optional)");
    inputKey_  = ftxui::Input(&editKey_,  "/path/to/key (optional)");
    // Password input with hidden characters
    ftxui::InputOption passwordOption;
    passwordOption.password = true;
    inputPassword_ = ftxui::Input(&editPassword_, "password (NOT RECOMMENDED)", passwordOption);

    // Build edit container
    editContainer_ = ftxui::Container::Vertical({
        inputName_,
        inputHost_,
        inputUser_,
        inputPath_,
        inputPort_,
        inputKey_,
        inputPassword_,
    });

    refresh();
}

void ConfigModal::show() {
    shown_ = true;
    mode_ = Mode::List;
    modeIndex_ = 0;
    if (externalShown_) *externalShown_ = true;
    refresh();
}

void ConfigModal::hide() {
    shown_ = false;
    mode_ = Mode::List;
    if (externalShown_) *externalShown_ = false;
}

void ConfigModal::refresh() {
    configs_ = config_.profiles();
    updateListEntries();
}

ftxui::Component ConfigModal::operator()(bool* externalShown) {
    using namespace ftxui;

    // Store external state pointer for synchronization
    externalShown_ = externalShown;

    // ============================================
    // LIST MODE: Create an interactive menu
    // ============================================
    auto listMenu = Menu(&listEntries_, &listFocusedIndex_);

    // Wrap list menu with proper rendering
    auto listRenderer = Renderer(listMenu, [this, listMenu] {
        Elements content;

        // Header
        content.push_back(text("Configuration Profiles") | bold | center);
        content.push_back(separator());
        content.push_back(text(""));

        if (configs_.empty()) {
            content.push_back(text("No profiles found.") | dim | center);
            content.push_back(text("Press [A] to add a new profile.") | dim | center);
        } else {
            // Use the menu's render
            content.push_back(listMenu->Render() | frame);
        }

        content.push_back(text(""));
        content.push_back(separator());
        content.push_back(text(""));

        // Shortcuts
        content.push_back(hbox({
            text("[A]dd") | bold,
            text(" | "),
            text("[E]dit") | bold,
            text(" | "),
            text("[D]elete") | bold,
            text(" | "),
            text("[S]elect") | bold,
            text(" | "),
            text("[Esc] Close") | bold,
        }) | center);

        return vbox(content) | border | size(WIDTH, LESS_THAN, 60) | center;
    });

    // Add event handlers to the list renderer (not listMenu, so events work even when list is empty)
    listRenderer |= CatchEvent([this](Event event) {
        // Add new profile
        if (event == Event::Character('a') || event == Event::Character('A')) {
            startAdd();
            return true;
        }

        // Edit current profile
        if ((event == Event::Character('e') || event == Event::Character('E')) &&
            !configs_.empty()) {
            startEdit(listFocusedIndex_);
            return true;
        }

        // Delete current profile
        if ((event == Event::Character('d') || event == Event::Character('D')) &&
            !configs_.empty()) {
            deleteConfig(listFocusedIndex_);
            return true;
        }

        // Select current profile
        if ((event == Event::Character('s') || event == Event::Character('S')) &&
            !configs_.empty()) {
            selectConfig(listFocusedIndex_);
            return true;
        }

        // Close modal
        if (event == Event::Escape) {
            hide();
            return true;
        }

        return false;
    });

    // ============================================
    // EDIT MODE: Create form inputs
    // ============================================
    auto editFormWrapper = CatchEvent(editContainer_, [this](Event event) {
        // Save on Enter
        if (event == Event::Return) {
            saveEdit();
            return true;
        }

        // Cancel on Escape
        if (event == Event::Escape) {
            cancelEdit();
            return true;
        }

        return false;
    });

    auto editRenderer = Renderer(editFormWrapper, [this] {
        std::string title = isEditing_ ? "Edit Profile" : "Add Profile";

        Elements content;
        content.push_back(text(title) | bold | center);
        content.push_back(separator());
        content.push_back(text(""));
        content.push_back(hbox({
            text("Name:    ") | size(WIDTH, EQUAL, 10),
            inputName_->Render(),
        }));
        content.push_back(hbox({
            text("Host:    ") | size(WIDTH, EQUAL, 10),
            inputHost_->Render(),
        }));
        content.push_back(hbox({
            text("User:    ") | size(WIDTH, EQUAL, 10),
            inputUser_->Render(),
        }));
        content.push_back(hbox({
            text("Path:    ") | size(WIDTH, EQUAL, 10),
            inputPath_->Render(),
        }));
        content.push_back(hbox({
            text("Port:    ") | size(WIDTH, EQUAL, 10),
            inputPort_->Render(),
        }));
        content.push_back(hbox({
            text("Key:     ") | size(WIDTH, EQUAL, 10),
            inputKey_->Render(),
        }));
        content.push_back(hbox({
            text("Password:") | size(WIDTH, EQUAL, 10),
            inputPassword_->Render(),
        }) | color(Color::Yellow));  // Yellow to indicate caution
        content.push_back(text(""));

        // Error message
        if (!errorMessage_.empty()) {
            content.push_back(text(errorMessage_) | color(Color::Red));
            content.push_back(text(""));
        }

        content.push_back(separator());
        content.push_back(text(""));

        content.push_back(hbox({
            text("[Enter] Save") | bold,
            text(" | "),
            text("[Esc] Cancel") | bold,
        }) | center);

        return vbox(content) | border | size(WIDTH, LESS_THAN, 60) | center;
    });

    // ============================================
    // Tab container to switch between modes
    // ============================================
    auto mainContainer = Container::Tab({
        listRenderer,
        editRenderer,
    }, &modeIndex_);

    return mainContainer;
}

void ConfigModal::startAdd() {
    isEditing_ = false;
    editName_.clear();
    editHost_.clear();
    editUser_.clear();
    editPath_.clear();
    editPort_.clear();
    editKey_.clear();
    editPassword_.clear();
    errorMessage_.clear();
    mode_ = Mode::Edit;
    modeIndex_ = 1;
    inputName_->TakeFocus();
}

void ConfigModal::startEdit(int index) {
    if (index < 0 || index >= static_cast<int>(configs_.size())) {
        return;
    }

    isEditing_ = true;
    const auto& config = configs_[index];
    editName_ = config.name;
    editHost_ = config.host;
    editUser_ = config.user;
    editPath_ = config.remotePath;
    editPort_ = (config.port != 22) ? std::to_string(config.port) : "";
    editKey_ = config.identityFile;
    editPassword_ = config.password;
    errorMessage_.clear();
    mode_ = Mode::Edit;
    modeIndex_ = 1;
    inputName_->TakeFocus();
}

void ConfigModal::saveEdit() {
    errorMessage_ = validateInputs();
    if (!errorMessage_.empty()) {
        return;
    }

    RemoteConfig config;
    config.name = editName_;
    config.host = editHost_;
    config.user = editUser_;
    config.remotePath = editPath_;

    // Parse port (default to 22 if empty)
    if (!editPort_.empty()) {
        try {
            config.port = static_cast<uint16_t>(std::stoi(editPort_));
        } catch (...) {
            errorMessage_ = "Invalid port number";
            return;
        }
    } else {
        config.port = 22;
    }

    // Parse SSH key (optional)
    if (!editKey_.empty()) {
        config.identityFile = editKey_;
    }

    // Parse password (optional)
    if (!editPassword_.empty()) {
        config.password = editPassword_;
    }

    auto error = config_.addProfile(config);
    if (error) {
        errorMessage_ = error.message;
        return;
    }

    // If adding new profile, set it as current
    if (!isEditing_) {
        config_.setCurrentProfile(config.name);
    }

    // Save to disk
    auto saveError = config_.save();
    if (saveError) {
        errorMessage_ = saveError.message;
        return;
    }

    refresh();
    mode_ = Mode::List;
    modeIndex_ = 0;
}

void ConfigModal::cancelEdit() {
    mode_ = Mode::List;
    modeIndex_ = 0;
    errorMessage_.clear();
}

void ConfigModal::deleteConfig(int index) {
    if (index < 0 || index >= static_cast<int>(configs_.size())) {
        return;
    }

    const auto& config = configs_[index];
    auto error = config_.removeProfile(config.name);
    if (error) {
        errorMessage_ = error.message;
        return;
    }

    // Save to disk
    auto saveError = config_.save();
    if (saveError) {
        errorMessage_ = saveError.message;
        return;
    }

    refresh();

    // Adjust focus if needed
    if (listFocusedIndex_ >= static_cast<int>(configs_.size())) {
        listFocusedIndex_ = std::max(0, static_cast<int>(configs_.size()) - 1);
    }
}

void ConfigModal::selectConfig(int index) {
    if (index < 0 || index >= static_cast<int>(configs_.size())) {
        return;
    }

    const auto& config = configs_[index];
    auto error = config_.setCurrentProfile(config.name);
    if (error) {
        errorMessage_ = error.message;
        return;
    }

    // Save to disk
    auto saveError = config_.save();
    if (saveError) {
        errorMessage_ = saveError.message;
        return;
    }

    refresh();
}

std::string ConfigModal::validateInputs() const {
    if (editName_.empty()) {
        return "Name is required";
    }
    if (editHost_.empty()) {
        return "Host is required";
    }
    if (editUser_.empty()) {
        return "User is required";
    }
    if (editPath_.empty()) {
        return "Path is required";
    }

    // Check for duplicate name when adding
    if (!isEditing_) {
        for (const auto& config : configs_) {
            if (config.name == editName_) {
                return "Profile name already exists";
            }
        }
    }

    return "";
}

void ConfigModal::updateListEntries() {
    listEntries_.clear();

    for (const auto& config : configs_) {
        std::string entry;
        bool isCurrent = (config.name == config_.currentProfile());

        entry += isCurrent ? "* " : "  ";
        entry += config.name;
        entry += "\n    ";
        entry += config.user + "@" + config.host + ":" + config.remotePath;

        listEntries_.push_back(entry);
    }
}

} // namespace diffsync
