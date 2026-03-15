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

MainScreen::MainScreen(GitScanner& scanner, const ConfigManager& config)
    : repoRoot_(scanner.repoRoot()) {

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

    // Create screen
    ScreenInteractive screen = ScreenInteractive::TerminalOutput();

    // Build UI
    auto component = buildUI();
    component |= CatchEvent([&](Event event) {
        // Handle global shortcuts
        if (event == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    // Store exit callback
    screenExit_ = screen.ExitLoopClosure();

    // Run loop
    screen.Loop(component);

    return syncResult_;
}

ftxui::Component MainScreen::buildUI() {
    using namespace ftxui;

    // Get file list component
    auto fileList = (*fileList_)();

    // Wrap with shortcuts
    auto withShortcuts = buildShortcuts(fileList);

    // Create main renderer
    auto renderer = Renderer(withShortcuts, [&] {
        // Get selected count
        size_t selectedCount = fileList_->getSelectedCount();
        uint64_t selectedSize = fileList_->getSelectedSize();
        size_t totalCount = fileList_->getFiles().size();

        // Build content
        Elements content;

        // Header
        content.push_back(getHeaderElement());
        content.push_back(separator());

        // Shortcut hints
        content.push_back(hbox({
            text("[F2] Select All") | xflex,
            text("[F3] Unselect All") | xflex,
            text("[Enter] Sync") | xflex,
            text("[Esc] Quit") | xflex,
        }));

        content.push_back(separator());

        // File list or sync status
        if (syncing_) {
            content.push_back(getSyncElement() | flex);
        } else {
            content.push_back(fileList->Render() | flex);
        }

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

    return renderer;
}

ftxui::Component MainScreen::buildShortcuts(ftxui::Component child) {
    using namespace ftxui;

    return CatchEvent(child, [&](Event event) {
        if (syncing_) return false;  // Ignore shortcuts during sync

        if (event == Event::F2) {
            fileList_->selectAll();
            return true;
        }
        if (event == Event::F3) {
            fileList_->unselectAll();
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

    // Check if sync thread is already running
    std::lock_guard<std::mutex> lock(syncMutex_);
    if (syncThreadRunning_) {
        syncStatus_ = "Sync already in progress";
        return;
    }

    // Start sync
    syncing_ = true;
    syncStatus_ = "Starting sync...";
    syncProgress_ = 0;
    syncThreadRunning_ = true;

    // Run sync in background
    syncThread_ = std::thread([this, selectedFiles]() {
        RsyncExecutor rsync;
        RsyncExecutor::Options options;
        options.compress = true;
        options.showProgress = true;

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
            syncing_ = false;
            syncThreadRunning_ = false;
            if (syncResult_.hasErrors()) {
                syncStatus_ = "Sync failed";
            } else {
                syncStatus_ = "Sync complete";
            }
        }
        syncCV_.notify_one();

        // Exit after sync
        if (screenExit_) {
            screenExit_();
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
    using namespace ftxui;

    Elements content;
    content.push_back(text("Syncing files...") | bold);
    content.push_back(text(""));

    // Progress bar
    int barWidth = 40;
    int filled = (syncProgress_ * barWidth) / 100;
    std::string bar;
    for (int i = 0; i < barWidth; ++i) {
        bar += (i < filled) ? "=" : " ";
    }

    content.push_back(hbox({
        text("["),
        text(bar) | color(Color::Blue),
        text("]"),
        text(std::to_string(syncProgress_) + "%")
    }));

    content.push_back(text(""));
    content.push_back(text(syncStatus_));

    return vbox(content) | center | size(HEIGHT, LESS_THAN, 10);
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

} // namespace diffsync
