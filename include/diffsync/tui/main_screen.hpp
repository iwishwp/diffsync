#pragma once

#include "diffsync/types.hpp"
#include "diffsync/core/config.hpp"
#include "diffsync/core/git_scanner.hpp"
#include "diffsync/core/transfer_log.hpp"
#include "diffsync/tui/file_list.hpp"
#include "diffsync/tui/config_modal.hpp"
#include "diffsync/tui/log_modal.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

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
    MainScreen(GitScanner& scanner, ConfigManager& config);

    /**
     * Launch TUI loop (blocking)
     * @return Sync result if sync was performed
     */
    SyncResult run();

    /**
     * Get sync result after TUI exits
     */
    SyncResult getSyncResult() const { return syncResult_; }

    /**
     * Destructor - ensures sync thread completes before destruction
     */
    ~MainScreen();

private:
    // Config manager reference (for modal operations)
    ConfigManager& configManager_;

    // Store values instead of references to avoid dangling references
    std::string repoRoot_;
    RemoteConfig remoteConfig_;
    std::unique_ptr<FileList> fileList_;
    std::unique_ptr<ConfigModal> configModal_;
    std::unique_ptr<LogModal> logModal_;
    bool configModalShown_ = false;  // For Modal component - must stay alive
    bool helpModalShown_ = false;    // For help modal - must stay alive
    bool syncModalShown_ = false;    // For sync progress modal - must stay alive
    bool logModalShown_ = false;     // For log modal - must stay alive
    SyncResult syncResult_;
    bool syncing_ = false;
    bool syncComplete_ = false;      // Track if sync has completed
    std::string syncStatus_;
    int syncProgress_ = 0;
    uint64_t syncTotalSize_ = 0;     // Total size for progress calculation
    TransferLog transferLog_;        // Transfer log for current session
    std::vector<ChangeFile> lastSyncFiles_;  // Files from last sync for retry

    // Thread management
    std::thread syncThread_;
    mutable std::mutex syncMutex_;
    std::condition_variable syncCV_;
    bool syncThreadRunning_ = false;

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
     * Get help modal component
     */
    ftxui::Component buildHelpModal();

    /**
     * Get sync progress modal component
     */
    ftxui::Component buildSyncModal();

    /**
     * Get log modal component
     */
    ftxui::Component buildLogModal();

    /**
     * Retry failed files
     */
    void retryFailedFiles(const std::vector<ChangeFile>& files);

    /**
     * Screen exit callback
     */
    std::function<void()> screenExit_;

    /**
     * Screen reference for triggering redraws
     * Using pointer because ScreenInteractive is not copyable
     */
    ftxui::ScreenInteractive* screen_ = nullptr;
};

} // namespace diffsync
