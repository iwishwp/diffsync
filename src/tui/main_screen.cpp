#include "diffsync/tui/main_screen.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>
#include <sstream>
#include <iomanip>
#include <thread>
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

MainScreen::MainScreen(GitScanner& scanner, const ConfigManager& config)
    : scanner_(scanner), config_(config) {

    // Scan for files
    std::vector<ChangeFile> files = scanner_.scan();

    // Create file list
    fileList_ = std::make_unique<FileList>(files);
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

    // Get current config
    auto remoteConfig = config_.getCurrentConfig();
    if (!remoteConfig) {
        syncStatus_ = "Error: No profile selected";
        return;
    }

    // Start sync
    syncing_ = true;
    syncStatus_ = "Starting sync...";
    syncProgress_ = 0;

    // Get screen to trigger refresh
    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    // Run sync in background
    std::thread([this, selectedFiles, remoteConfig]() {
        RsyncExecutor rsync;
        RsyncExecutor::Options options;
        options.compress = true;
        options.showProgress = true;

        auto progressCallback = [this](const RsyncExecutor::Progress& progress) {
            syncProgress_ = progress.percent;
            std::ostringstream oss;
            oss << "Syncing: " << progress.filesTransferred << " files, "
                << formatBytes(progress.bytesTransferred);
            syncStatus_ = oss.str();
        };

        syncResult_ = rsync.execute(
            selectedFiles,
            scanner_.repoRoot(),
            *remoteConfig,
            options,
            progressCallback
        );

        syncing_ = false;
        if (syncResult_.hasErrors()) {
            syncStatus_ = "Sync failed";
        } else {
            syncStatus_ = "Sync complete";
        }

        // Exit after sync
        if (screenExit_) {
            screenExit_();
        }
    }).detach();
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

    // Get current config
    auto currentConfig = config_.getCurrentConfig();

    std::ostringstream oss;
    oss << "diffsync";

    if (currentConfig) {
        oss << " - Config: " << currentConfig->name
            << " (" << currentConfig->user << "@"
            << currentConfig->host << ":"
            << currentConfig->remotePath << ")";
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
