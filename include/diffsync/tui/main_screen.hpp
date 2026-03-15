#pragma once

#include "diffsync/types.hpp"
#include "diffsync/core/config.hpp"
#include "diffsync/core/git_scanner.hpp"
#include "diffsync/core/rsync.hpp"
#include "diffsync/tui/file_list.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <functional>

namespace diffsync {

/**
 * MainScreen - Main TUI interface for diffsync
 *
 * Features:
 * - Display configuration info
 * - File list with selection
 * - Sync progress display
 * - Keyboard shortcuts
 */
class MainScreen {
public:
    /**
     * Create main screen
     * @param scanner Git scanner for repo
     * @param config Config manager
     */
    MainScreen(GitScanner& scanner, const ConfigManager& config);

    /**
     * Launch TUI loop (blocking)
     * @return Sync result if sync was performed
     */
    SyncResult run();

    /**
     * Get sync result after TUI exits
     */
    SyncResult getSyncResult() const { return syncResult_; }

private:
    GitScanner& scanner_;
    const ConfigManager& config_;
    std::unique_ptr<FileList> fileList_;
    SyncResult syncResult_;
    bool syncing_ = false;
    std::string syncStatus_;
    int syncProgress_ = 0;

    /**
     * Build main UI component
     */
    ftxui::Component buildUI();

    /**
     * Handle keyboard shortcuts
     */
    ftxui::Component buildShortcuts(ftxui::Component child);

    /**
     * Start sync operation
     */
    void startSync();

    /**
     * Handle exit
     */
    void exitTui();

    /**
     * Update status bar
     */
    std::string getStatusBarText() const;

    /**
     * Update sync display
     */
    ftxui::Element getSyncElement() const;

    /**
     * Get header element
     */
    ftxui::Element getHeaderElement() const;

    /**
     * Get footer element
     */
    ftxui::Element getFooterElement() const;

    /**
     * Format file size
     */
    static std::string formatSize(uint64_t size);

    /**
     * Screen exit callback
     */
    std::function<void()> screenExit_;
};

} // namespace diffsync
