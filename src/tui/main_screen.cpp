#include "diffsync/tui/main_screen.hpp"
#include "diffsync/core/rsync.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

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

MainScreen::MainScreen(GitScanner& scanner, ConfigManager& config)
    : configManager_(config), repoRoot_(scanner.repoRoot()),
      configModal_(std::make_unique<ConfigModal>(config)),
      logModal_(std::make_unique<LogModal>(transferLog_, [this](const std::vector<ChangeFile>& files) {
          retryFailedFiles(files);
      })) {

    // Get current config and store a copy
    auto currentConfig = config.getCurrentConfig();
    if (currentConfig) {
        remoteConfig_ = *currentConfig;
    }

    // Scan for files
    std::vector<ChangeFile> files = scanner.scan();

    // Create file list
    fileList_ = std::make_unique<FileList>(files);
}

MainScreen::~MainScreen() {
    // Ensure sync thread completes before destruction
    std::unique_lock<std::mutex> lock(syncMutex_);
    if (syncThread_.joinable()) {
        lock.unlock();
        syncThread_.join();
    }
}

SyncResult MainScreen::run() {
    using namespace ftxui;

    // Create screen with alternative screen mode (clears on exit)
    ScreenInteractive screen = ScreenInteractive::Fullscreen();

    // Store screen reference for triggering redraws
    screen_ = &screen;

    // Build UI
    auto component = buildUI();

    component |= CatchEvent([&](Event event) {
        // Handle global shortcuts
        if (event == Event::Escape) {
            if (syncModalShown_) {
                // Sync modal is open
                if (syncComplete_) {
                    // Sync complete - close modal
                    syncModalShown_ = false;
                    syncComplete_ = false;
                    return true;
                }
                // Sync in progress - don't allow close
                return true;
            } else if (helpModalShown_) {
                // Help modal is open - close it
                helpModalShown_ = false;
                return true;
            } else if (logModalShown_) {
                // Log modal is open - close it
                logModalShown_ = false;
                return true;
            } else if (configModalShown_) {
                // Config modal is open - don't consume, let it handle
                return false;
            } else {
                // No modal is open - exit the program
                screen.ExitLoopClosure()();
                return true;
            }
        }
        // Handle ? key for help toggle/close
        if (event == Event::Character('?')) {
            if (helpModalShown_) {
                helpModalShown_ = false;
            } else {
                helpModalShown_ = true;
            }
            return true;
        }
        return false;
    });

    // Store exit callback
    screenExit_ = screen.ExitLoopClosure();

    // Run loop with automatic refresh during sync
    std::atomic<bool> stopTimer{false};
    std::thread timerThread([&]() {
        while (!stopTimer) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // 50ms for smooth updates
            if (syncing_ && screen_) {
                // Continue posting events even when complete, to ensure final progress is rendered
                screen_->PostEvent(ftxui::Event::Character(' '));
            }
        }
    });

    screen.Loop(component);

    stopTimer = true;
    if (timerThread.joinable()) {
        timerThread.join();
    }

    return syncResult_;
}

ftxui::Component MainScreen::buildUI() {
    using namespace ftxui;

    // Get file list component
    auto fileList = (*fileList_)();

    // Wrap with shortcuts
    auto withShortcuts = buildShortcuts(fileList);

    // Get config modal component
    auto configModal = (*configModal_)(&configModalShown_);

    // Create main renderer - capture fileList by value to keep it alive
    auto renderer = Renderer(withShortcuts, [fileList, this] {
        // Refresh remote config from modal changes
        auto currentConfig = configManager_.getCurrentConfig();
        if (currentConfig) {
            remoteConfig_ = *currentConfig;
        }

        // Get selected count
        size_t selectedCount = fileList_->getSelectedCount();
        uint64_t selectedSize = fileList_->getSelectedSize();
        size_t totalCount = fileList_->getFiles().size();

        // Build content
        Elements content;

        // Header
        content.push_back(getHeaderElement());
        content.push_back(separator());

        // Shortcut hints - simplified format
        content.push_back(hbox({
            text("Toggle: <Space>"),
            text(" | "),
            text("Select: <F2>"),
            text(" | "),
            text("Sync: <Enter>"),
            text(" | "),
            text("Help: <?>"),
        }));

        content.push_back(separator());

        // File list - always show (sync progress shown in modal overlay)
        content.push_back(fileList->Render() | flex);

        // Status bar
        std::ostringstream status;
        status << "Selected: " << selectedCount << "/" << totalCount;
        if (selectedSize > 0) {
            status << "  Total: " << formatSize(selectedSize);
        }

        content.push_back(separator());
        content.push_back(text(status.str()) | bold);

        return vbox(content) | border | size(WIDTH, GREATER_THAN, 80);
    });

    // Wrap with modal for config dialog
    auto withConfigModal = Modal(renderer, configModal, &configModalShown_);

    // Wrap with help modal (nested)
    auto helpModal = buildHelpModal();
    auto withHelpModal = Modal(withConfigModal, helpModal, &helpModalShown_);

    // Wrap with log modal
    auto logModal = buildLogModal();
    auto withLogModal = Modal(withHelpModal, logModal, &logModalShown_);

    // Wrap with sync modal (outermost)
    auto syncModal = buildSyncModal();
    auto modalWrapper = Modal(withLogModal, syncModal, &syncModalShown_);

    return modalWrapper;
}

ftxui::Component MainScreen::buildHelpModal() {
    using namespace ftxui;

    // Use Renderer with content built INSIDE the lambda to avoid dangling reference
    auto renderer = Renderer([&] {
        Elements content;
        content.push_back(text("Keyboard Shortcuts") | bold | center);
        content.push_back(separator());
        content.push_back(text(""));

        // Navigation
        content.push_back(text("Navigation:") | bold);
        content.push_back(text("  <Up>/<Down>    - Move cursor"));
        content.push_back(text("  <Space>        - Toggle file selection"));
        content.push_back(text(""));

        // Actions
        content.push_back(text("Actions:") | bold);
        content.push_back(text("  <F2>           - Select all / Unselect all"));
        content.push_back(text("  <C>            - Open configuration"));
        content.push_back(text("  <Enter>        - Sync selected files"));
        content.push_back(text("  <L>            - View transfer log"));
        content.push_back(text("  <?>            - Show this help"));
        content.push_back(text("  <Esc>          - Quit / Close dialog"));
        content.push_back(text(""));

        // Config modal shortcuts
        content.push_back(text("Configuration Mode:") | bold);
        content.push_back(text("  <A>            - Add new profile"));
        content.push_back(text("  <E>            - Edit selected profile"));
        content.push_back(text("  <D>            - Delete selected profile"));
        content.push_back(text("  <S>            - Select profile as current"));
        content.push_back(text(""));

        // Log modal shortcuts
        content.push_back(text("Log Window:") | bold);
        content.push_back(text("  <R>            - Retry selected failed file"));
        content.push_back(text("  <A>            - Retry all failed files"));
        content.push_back(text(""));

        content.push_back(separator());
        content.push_back(text("Press <Esc> or <?> to close") | dim | center);

        return vbox(content) | border | size(WIDTH, LESS_THAN, 60) | center;
    });

    return renderer;
}

ftxui::Component MainScreen::buildSyncModal() {
    using namespace ftxui;

    // Sync progress modal content
    auto renderer = Renderer([&] {
        Elements content;

        if (syncComplete_) {
            // Show results
            content.push_back(text("Sync Complete") | bold | center);
            content.push_back(separator());
            content.push_back(text(""));

            // Summary
            std::ostringstream oss;
            oss << "Total: " << syncResult_.totalFiles << " files";
            content.push_back(text(oss.str()));
            oss.str("");
            oss << "Success: " << syncResult_.successCount;
            content.push_back(text(oss.str()));

            if (syncResult_.failedCount > 0) {
                oss.str("");
                oss << "Failed: " << syncResult_.failedCount;
                content.push_back(text(oss.str()) | color(Color::Red));
            }
            if (syncResult_.skippedCount > 0) {
                oss.str("");
                oss << "Skipped: " << syncResult_.skippedCount;
                content.push_back(text(oss.str()));
            }

            content.push_back(text(""));
            content.push_back(text(syncStatus_));

            if (syncResult_.hasErrors()) {
                content.push_back(text(""));
                content.push_back(text("Errors:") | color(Color::Red));
                for (const auto& error : syncResult_.errors) {
                    content.push_back(text("• " + error) | color(Color::Red));
                }
            }

            content.push_back(text(""));
            if (transferLog_.hasFailures()) {
                content.push_back(text("Press [Esc] to close | [L] to view log") | dim | center);
            } else {
                content.push_back(text("Press [Esc] to close") | dim | center);
            }

        } else {
            // Show progress
            content.push_back(text("Syncing Files...") | bold | center);
            content.push_back(separator());
            content.push_back(text(""));

            // Progress bar using Gauge
            content.push_back(hbox({
                text("Progress:"),
                text(" "),
                gauge(syncProgress_ / 100.0f) | size(WIDTH, GREATER_THAN, 20),
            }));

            content.push_back(text(""));

            // Progress percentage and size info
            std::ostringstream oss;
            oss << syncProgress_ << "%";
            content.push_back(text(oss.str()) | bold | center);

            oss.str("");
            oss << "Transferred: " << formatBytes(syncTotalSize_ * syncProgress_ / 100);
            content.push_back(text(oss.str()) | dim | center);

            content.push_back(text(""));
            content.push_back(text(syncStatus_) | center);
        }

        return vbox(content) | border | size(WIDTH, LESS_THAN, 50) | center;
    });

    // Event handler - close on ESC when sync complete, show log on L
    auto withEvents = CatchEvent(renderer, [this](Event event) {
        if (event == Event::Escape) {
            if (syncComplete_) {
                syncModalShown_ = false;
                syncComplete_ = false;  // Reset for next sync
                return true;
            }
        }
        if ((event == Event::Character('l') || event == Event::Character('L')) && syncComplete_) {
            // Show log modal
            if (!transferLog_.getEntries().empty()) {
                logModal_->show();
                syncModalShown_ = false;
            }
            return true;
        }
        return false;
    });

    return withEvents;
}

ftxui::Component MainScreen::buildShortcuts(ftxui::Component child) {
    using namespace ftxui;

    return CatchEvent(child, [&](Event event) {
        if (syncing_) return false;  // Ignore shortcuts during sync

        if (event == Event::F2) {
            // Toggle: if all selected, unselect all; otherwise select all
            size_t selectedCount = fileList_->getSelectedCount();
            size_t totalCount = fileList_->getFiles().size();

            if (selectedCount == totalCount && totalCount > 0) {
                fileList_->unselectAll();
            } else {
                fileList_->selectAll();
            }
            return true;
        }
        if (event == Event::Character('c') || event == Event::Character('C')) {
            // Show config modal
            configModal_->show();
            return true;
        }
        if (event == Event::Character('l') || event == Event::Character('L')) {
            // Show log modal if there are log entries
            if (!transferLog_.getEntries().empty()) {
                logModal_->show();
            }
            return true;
        }
        if (event == Event::Return) {
            startSync();
            return true;
        }

        return false;
    });
}

void MainScreen::startSync() {
    // Get selected files
    std::vector<ChangeFile> selectedFiles = fileList_->getSelectedFiles();

    if (selectedFiles.empty()) {
        syncStatus_ = "No files selected";
        return;
    }

    // Check if we have a valid config
    if (remoteConfig_.name.empty()) {
        syncStatus_ = "Error: No profile selected";
        return;
    }

    // CRITICAL SECTION: All thread state must be managed atomically
    std::unique_lock<std::mutex> lock(syncMutex_);

    // Step 1: Clean up previous thread if exists
    // Note: If previous sync just completed, join() returns immediately
    if (syncThread_.joinable()) {
        lock.unlock();  // Release lock during join (join may block)
        syncThread_.join();
        lock.lock();    // Re-acquire lock
    }

    // Step 2: Check if a sync is already in progress
    // (Double-check after re-acquiring lock)
    if (syncThreadRunning_) {
        syncStatus_ = "Sync already in progress";
        return;
    }

    // Step 3: Set sync running state BEFORE starting thread
    // This prevents race where multiple threads could be started
    syncing_ = true;
    syncComplete_ = false;
    syncStatus_ = "Starting sync...";
    syncProgress_ = 0;
    syncThreadRunning_ = true;

    // Store files for retry and clear log
    lastSyncFiles_ = selectedFiles;
    transferLog_.clear();

    // Calculate total size for progress
    syncTotalSize_ = 0;
    for (const auto& file : selectedFiles) {
        syncTotalSize_ += file.size;
    }

    // Show sync modal
    syncModalShown_ = true;

    // Step 4: Start new thread (state is already set to "running")
    syncThread_ = std::thread([this, selectedFiles]() {
        try {
            RsyncExecutor rsync;
            RsyncExecutor::Options options;
            options.compress = true;
            options.showProgress = true;
            options.verbose = true;
            options.fileCallback = [this](const std::string& filename, TransferStatus status,
                                          const std::string& errorMessage) {
                transferLog_.logFile(filename, status, errorMessage);
            };

            auto progressCallback = [this](const RsyncExecutor::Progress& progress) {
                std::lock_guard<std::mutex> lock(syncMutex_);
                syncProgress_ = progress.percent;
                std::ostringstream oss;
                oss << "Syncing: " << progress.filesTransferred << " files, "
                    << formatBytes(progress.bytesTransferred);
                syncStatus_ = oss.str();
            };

            syncResult_ = rsync.execute(
                selectedFiles,
                repoRoot_,
                remoteConfig_,
                options,
                progressCallback
            );

            {
                std::lock_guard<std::mutex> lock(syncMutex_);
                syncProgress_ = 100;  // Force progress to 100% BEFORE marking complete
                syncComplete_ = true;
                syncing_ = false;
                syncThreadRunning_ = false;
                if (syncResult_.hasErrors()) {
                    syncStatus_ = "Sync completed with errors";
                } else {
                    syncStatus_ = "Sync completed successfully";
                }
            }
            // Trigger immediate UI refresh to show 100% progress
            if (screen_) {
                screen_->PostEvent(ftxui::Event::Character(' '));
            }
            syncCV_.notify_one();
        } catch (const std::exception& e) {
            {
                std::lock_guard<std::mutex> lock(syncMutex_);
                syncProgress_ = 100;  // Force progress to 100% BEFORE marking complete
                syncComplete_ = true;
                syncing_ = false;
                syncThreadRunning_ = false;
                syncStatus_ = std::string("Sync failed: ") + e.what();
                syncResult_.errors.push_back(e.what());
            }
            // Trigger immediate UI refresh to show 100% progress
            if (screen_) {
                screen_->PostEvent(ftxui::Event::Character(' '));
            }
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(syncMutex_);
                syncProgress_ = 100;  // Force progress to 100% BEFORE marking complete
                syncComplete_ = true;
                syncing_ = false;
                syncThreadRunning_ = false;
                syncStatus_ = "Sync failed: unknown error";
                syncResult_.errors.push_back("unknown error");
            }
            // Trigger immediate UI refresh to show 100% progress
            if (screen_) {
                screen_->PostEvent(ftxui::Event::Character(' '));
            }
        }
    });
}

void MainScreen::exitTui() {
    if (screenExit_) {
        screenExit_();
    }
}

std::string MainScreen::getStatusBarText() const {
    size_t selectedCount = fileList_->getSelectedCount();
    size_t totalCount = fileList_->getFiles().size();

    std::ostringstream oss;
    oss << "Selected: " << selectedCount << "/" << totalCount;
    return oss.str();
}

ftxui::Element MainScreen::getSyncElement() const {
    // Not used anymore - sync progress shown in modal
    using namespace ftxui;
    return text("Syncing...") | center;
}

ftxui::Element MainScreen::getHeaderElement() const {
    using namespace ftxui;

    std::ostringstream oss;
    oss << "diffsync";

    if (!remoteConfig_.name.empty()) {
        oss << " - Config: " << remoteConfig_.name
            << " (" << remoteConfig_.user << "@"
            << remoteConfig_.host << ":"
            << remoteConfig_.remotePath << ")";
    } else {
        oss << " - No config selected";
    }

    return text(oss.str()) | bold | center;
}

ftxui::Element MainScreen::getFooterElement() const {
    using namespace ftxui;
    return text(getStatusBarText()) | center;
}

std::string MainScreen::formatSize(uint64_t size) {
    return formatBytes(size);
}

ftxui::Component MainScreen::buildLogModal() {
    return (*logModal_)(&logModalShown_);
}

void MainScreen::retryFailedFiles(const std::vector<ChangeFile>& files) {
    if (files.empty()) {
        return;
    }

    // Check if we have a valid config
    if (remoteConfig_.name.empty()) {
        syncStatus_ = "Error: No profile selected";
        return;
    }

    // CRITICAL SECTION: All thread state must be managed atomically
    std::unique_lock<std::mutex> lock(syncMutex_);

    // Clean up previous thread if exists
    if (syncThread_.joinable()) {
        lock.unlock();
        syncThread_.join();
        lock.lock();
    }

    // Check if a sync is already in progress
    if (syncThreadRunning_) {
        syncStatus_ = "Sync already in progress";
        return;
    }

    // Set sync running state
    syncing_ = true;
    syncComplete_ = false;
    syncStatus_ = "Retrying failed files...";
    syncProgress_ = 0;
    syncThreadRunning_ = true;

    // Store files for retry and clear log
    lastSyncFiles_ = files;
    transferLog_.clear();

    // Calculate total size for progress
    syncTotalSize_ = 0;
    for (const auto& file : files) {
        syncTotalSize_ += file.size;
    }

    // Show sync modal
    syncModalShown_ = true;

    // Start retry thread
    syncThread_ = std::thread([this, files]() {
        try {
            RsyncExecutor rsync;
            RsyncExecutor::Options options;
            options.compress = true;
            options.showProgress = true;
            options.verbose = true;
            options.fileCallback = [this](const std::string& filename, TransferStatus status,
                                          const std::string& errorMessage) {
                transferLog_.logFile(filename, status, errorMessage);
            };

            auto progressCallback = [this](const RsyncExecutor::Progress& progress) {
                std::lock_guard<std::mutex> lock(syncMutex_);
                syncProgress_ = progress.percent;
                std::ostringstream oss;
                oss << "Retrying: " << progress.filesTransferred << " files, "
                    << formatBytes(progress.bytesTransferred);
                syncStatus_ = oss.str();
            };

            syncResult_ = rsync.execute(
                files,
                repoRoot_,
                remoteConfig_,
                options,
                progressCallback
            );

            {
                std::lock_guard<std::mutex> lock(syncMutex_);
                syncProgress_ = 100;
                syncComplete_ = true;
                syncing_ = false;
                syncThreadRunning_ = false;
                if (syncResult_.hasErrors()) {
                    syncStatus_ = "Retry completed with errors";
                } else {
                    syncStatus_ = "Retry completed successfully";
                }
            }
            if (screen_) {
                screen_->PostEvent(ftxui::Event::Character(' '));
            }
            syncCV_.notify_one();
        } catch (const std::exception& e) {
            {
                std::lock_guard<std::mutex> lock(syncMutex_);
                syncProgress_ = 100;
                syncComplete_ = true;
                syncing_ = false;
                syncThreadRunning_ = false;
                syncStatus_ = std::string("Retry failed: ") + e.what();
                syncResult_.errors.push_back(e.what());
            }
            if (screen_) {
                screen_->PostEvent(ftxui::Event::Character(' '));
            }
        }
    });
}

} // namespace diffsync
