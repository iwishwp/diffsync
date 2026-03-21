# Transfer Log Feature Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a transfer log feature that displays file-by-file transfer results and allows retrying failed files.

**Architecture:**
- Add `TransferStatus` enum and `FileTransferLog` struct to types.hpp for transfer-specific status tracking
- Create `TransferLog` class to manage log entries (in-memory, cleared on exit)
- Create `LogModal` component for displaying log and retry functionality
- Integrate `LogModal` into existing modal stack in `MainScreen`
- Modify `RsyncExecutor` to track and return file-level results
- Add `L` key handler to open log window

**Tech Stack:** C++20, FTXUI v5.0.0, CMake

---

## Chunk 1: Core Data Types

### Task 1: Add Transfer Log Types to types.hpp

**Files:**
- Modify: `include/diffsync/types.hpp` (after line 17, before ChangeFile)

Add new enum and structs for transfer logging:

```cpp
// Transfer status (separate from git FileStatus)
enum class TransferStatus {
    Success,   // File transferred successfully
    Failed,    // File transfer failed
    Skipped    // File was skipped (already up-to-date)
};

// Single file transfer result
struct FileTransferLog {
    std::string filename;           // File path relative to repo root
    TransferStatus status;
    std::string errorMessage;       // Error message if status == Failed

    FileTransferLog() : status(TransferStatus::Success) {}
    FileTransferLog(const std::string& name, TransferStatus s, const std::string& err = "")
        : filename(name), status(s), errorMessage(err) {}
};
```

- [ ] **Step 1: Add the code above to types.hpp**

Edit `include/diffsync/types.hpp`, insert after `FileStatus` enum (around line 17).

- [ ] **Step 2: Update SyncResult to include file results**

Modify `SyncResult` struct (around line 49):

```cpp
struct SyncResult {
    int totalFiles = 0;
    int successCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    std::vector<std::string> errors;

    // NEW: Detailed file-level results
    std::vector<FileTransferLog> fileResults;

    bool hasErrors() const { return !errors.empty(); }
    bool hasFailures() const { return failedCount > 0; }  // NEW convenience method
};
```

- [ ] **Step 3: Compile to verify**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Clean build (no errors)

- [ ] **Step 4: Commit**

```bash
git add include/diffsync/types.hpp
git commit -m "feat: add TransferStatus and FileTransferLog types"
```

---

## Chunk 2: TransferLog Manager Class

### Task 2: Create TransferLog Class

**Files:**
- Create: `include/diffsync/core/transfer_log.hpp`
- Create: `src/core/transfer_log.cpp`

- [ ] **Step 1: Create header file**

Create `include/diffsync/core/transfer_log.hpp`:

```cpp
#pragma once

#include "diffsync/types.hpp"
#include <vector>
#include <string>
#include <mutex>

namespace diffsync {

/**
 * TransferLog - Manages file transfer log entries
 *
 * Thread-safe log storage for transfer session.
 * Log is in-memory only, cleared on program exit.
 */
class TransferLog {
public:
    TransferLog() = default;
    ~TransferLog() = default;

    // Disable copy, allow move
    TransferLog(const TransferLog&) = delete;
    TransferLog& operator=(const TransferLog&) = delete;
    TransferLog(TransferLog&&) = default;
    TransferLog& operator=(TransferLog&&) = default;

    /**
     * Log a file transfer result
     * Thread-safe
     */
    void logFile(const std::string& filename, TransferStatus status,
                 const std::string& errorMessage = "");

    /**
     * Check if there are any failed transfers
     * Thread-safe
     */
    bool hasFailures() const;

    /**
     * Get list of failed files for retry
     * Thread-safe
     */
    std::vector<FileTransferLog> getFailedFiles() const;

    /**
     * Update a specific file's log entry (for retry)
     * Thread-safe
     */
    void updateEntry(const std::string& filename, TransferStatus status,
                     const std::string& errorMessage = "");

    /**
     * Clear all log entries
     * Thread-safe
     */
    void clear();

    /**
     * Get all log entries (read-only)
     * Thread-safe
     */
    const std::vector<FileTransferLog>& getEntries() const;

    /**
     * Get count of entries by status
     */
    int getCount(TransferStatus status) const;

private:
    std::vector<FileTransferLog> entries_;
    mutable std::mutex mutex_;
};

} // namespace diffsync
```

- [ ] **Step 2: Create implementation file**

Create `src/core/transfer_log.cpp`:

```cpp
#include "diffsync/core/transfer_log.hpp"
#include <algorithm>

namespace diffsync {

void TransferLog::logFile(const std::string& filename, TransferStatus status,
                          const std::string& errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.emplace_back(filename, status, errorMessage);
}

bool TransferLog::hasFailures() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(entries_.begin(), entries_.end(),
        [](const FileTransferLog& log) {
            return log.status == TransferStatus::Failed;
        });
}

std::vector<FileTransferLog> TransferLog::getFailedFiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FileTransferLog> failed;
    for (const auto& entry : entries_) {
        if (entry.status == TransferStatus::Failed) {
            failed.push_back(entry);
        }
    }
    return failed;
}

void TransferLog::updateEntry(const std::string& filename, TransferStatus status,
                              const std::string& errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : entries_) {
        if (entry.filename == filename) {
            entry.status = status;
            entry.errorMessage = errorMessage;
            return;
        }
    }
    // If not found, add new entry
    entries_.emplace_back(filename, status, errorMessage);
}

void TransferLog::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

const std::vector<FileTransferLog>& TransferLog::getEntries() const {
    // Note: Caller should ensure thread safety if iterating after this call
    return entries_;
}

int TransferLog::getCount(TransferStatus status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::count_if(entries_.begin(), entries_.end(),
        [status](const FileTransferLog& log) {
            return log.status == status;
        });
}

} // namespace diffsync
```

- [ ] **Step 3: Update CMakeLists.txt**

Add to `CMakeLists.txt` in the sources list (around line 60):
```cmake
src/core/transfer_log.cpp
```

- [ ] **Step 4: Compile to verify**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Clean build

- [ ] **Step 5: Commit**

```bash
git add include/diffsync/core/transfer_log.hpp src/core/transfer_log.cpp CMakeLists.txt
git commit -m "feat: add TransferLog manager class"
```

---

## Chunk 3: Enhance RsyncExecutor for File-Level Results

### Task 3: Modify RsyncExecutor to Track File Results

**Files:**
- Modify: `src/core/rsync.cpp`

- [ ] **Step 1: Add file callback to Options**

Modify `Options` struct in `include/diffsync/core/rsync.hpp` (around line 26):

Add to the struct:
```cpp
std::function<void(const std::string& filename, TransferStatus status, const std::string& error)> fileCallback;
```

- [ ] **Step 2: Parse rsync verbose output for file results**

In `src/core/rsync.cpp`, modify the `execute()` method to parse verbose output.

Add this helper function to `RsyncExecutor` class (in rsync.hpp):

```cpp
private:
    /**
     * Parse rsync verbose output to extract file status
     * Returns pair<filename, status> or {"", TransferStatus::Skipped} if not a file line
     */
    std::pair<std::string, TransferStatus> parseVerboseLine(const std::string& line);
```

Implement in rsync.cpp:

```cpp
std::pair<std::string, TransferStatus> RsyncExecutor::parseVerboseLine(const std::string& line) {
    // rsync verbose format examples:
    // "file.txt"              - new file
    // ">f+++++++++ file.txt"   - file created
    // "*.f............ file.txt" - file matches (skip)
    // "cd+++++++++ dirname/"   - directory created
    // "sender file not found: file.txt" - error

    // Skip directory lines
    if (line.find("/$") != std::string::npos || line.find("+++++++++ ") == std::string::npos) {
        return {"", TransferStatus::Skipped};
    }

    // Check for error patterns
    if (line.find("sender file not found:") != std::string::npos ||
        line.find("failed:") != std::string::npos ||
        line.find("permission denied") != std::string::npos) {
        // Extract filename from error
        size_t pos = line.find_last_of(' ');
        if (pos != std::string::npos) {
            return {line.substr(pos + 1), TransferStatus::Failed};
        }
        return {"", TransferStatus::Failed};
    }

    // Look for file transfer indicators
    // Format: "*c+++++++++ filename" or ">f+++++++++ filename"
    size_t filePos = line.find_last_of(' ');
    if (filePos != std::string::npos && filePos < line.length() - 1) {
        std::string filename = line.substr(filePos + 1);
        // Check if it's a file transfer (starts with >f or *f)
        if (line.find(">f") != std::string::npos || line.find("*f") != std::string::npos) {
            // Check if it was actually sent (not skipped with *)
            if (line.find("*f") == 0) {
                return {filename, TransferStatus::Skipped};  // Already exists
            }
            return {filename, TransferStatus::Success};
        }
    }

    return {"", TransferStatus::Skipped};
}
```

- [ ] **Step 3: Integrate file logging into execute()**

Modify `RsyncExecutor::execute()` in `src/core/rsync.cpp` (around line 360).

Add before the line-reading loop:

```cpp
// Track which files we're transferring for result logging
std::vector<std::string> filesToTransfer;
for (const auto& file : files) {
    filesToTransfer.push_back(file.path.string());
}
```

Then modify the output reading loop to parse verbose output and call fileCallback:

```cpp
// Read output line by line in real-time
while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    std::string line(buffer.data());
    stdoutStr += line;

    // Parse file status from verbose output
    auto [filename, status] = parseVerboseLine(line);
    if (!filename.empty() && options.fileCallback) {
        std::string error;
        if (status == TransferStatus::Failed) {
            error = line;  // Full line contains error details
        }
        options.fileCallback(filename, status, error);
    }

    // Parse progress in real-time if callback provided
    if (progressCallback && options.showProgress) {
        Progress progress = parseProgress(line);
        if (progress.percent > 0 || progress.filesTransferred > 0) {
            progressCallback(progress);
        }
    }
}
```

- [ ] **Step 4: Build and test**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Clean build

- [ ] **Step 5: Commit**

```bash
git add include/diffsync/core/rsync.hpp src/core/rsync.cpp
git commit -m "feat: add file-level result tracking to RsyncExecutor"
```

---

## Chunk 4: LogModal Component

### Task 4: Create LogModal Header

**Files:**
- Create: `include/diffsync/tui/log_modal.hpp`

- [ ] **Step 1: Create log_modal.hpp**

```cpp
#pragma once

#include "diffsync/core/transfer_log.hpp"
#include <ftxui/component/component.hpp>
#include <string>
#include <functional>

namespace diffsync {

/**
 * LogModal - Displays transfer log and allows retry
 *
 * Features:
 * - View all transfer files with status
 * - Color-coded status indicators
 * - Select and retry failed files
 * - Retry all failed files at once
 */
class LogModal {
public:
    /**
     * Callback type for retry operation
     * @param files Files to retry
     */
    using RetryCallback = std::function<void(const std::vector<ChangeFile>&)>;

    /**
     * Create log modal
     * @param transferLog Reference to transfer log
     * @param retryCallback Called when user requests retry
     */
    LogModal(TransferLog& transferLog, RetryCallback retryCallback);

    /**
     * Get FTXUI component
     * @param externalShown Pointer to external shown flag (for Modal component)
     */
    ftxui::Component operator()(bool* externalShown);

    /**
     * Show the modal
     */
    void show();

    /**
     * Hide the modal
     */
    void hide();

    /**
     * Set files for retry (used when retrying)
     */
    void setRetryFiles(const std::vector<ChangeFile>& files);

private:
    TransferLog& transferLog_;
    RetryCallback retryCallback_;
    bool* externalShown_ = nullptr;

    // UI state
    int focusedIndex_ = 0;
    std::vector<std::string> listEntries_;

    // Files available for retry
    std::vector<ChangeFile> retryFiles_;

    /**
     * Build log list component
     */
    ftxui::Component buildLogList();

    /**
     * Update list entries from transfer log
     */
    void updateListEntries();

    /**
     * Format status to display string
     */
    static std::string formatStatus(TransferStatus status);

    /**
     * Get color for status
     */
    static ftxui::Color getStatusColor(TransferStatus status);
};

} // namespace diffsync
```

- [ ] **Step 2: Commit header**

```bash
git add include/diffsync/tui/log_modal.hpp
git commit -m "feat: add LogModal header"
```

### Task 5: Implement LogModal

**Files:**
- Create: `src/tui/log_modal.cpp`

- [ ] **Step 1: Create implementation file**

Create `src/tui/log_modal.cpp`:

```cpp
#include "diffsync/tui/log_modal.hpp"
#include "diffsync/types.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <algorithm>

namespace diffsync {

LogModal::LogModal(TransferLog& transferLog, RetryCallback retryCallback)
    : transferLog_(transferLog), retryCallback_(retryCallback) {
}

void LogModal::show() {
    if (externalShown_) *externalShown_ = true;
    updateListEntries();
    focusedIndex_ = 0;
}

void LogModal::hide() {
    if (externalShown_) *externalShown_ = false;
}

void LogModal::setRetryFiles(const std::vector<ChangeFile>& files) {
    retryFiles_ = files;
}

ftxui::Component LogModal::operator()(bool* externalShown) {
    using namespace ftxui;

    externalShown_ = externalShown;

    // Build log list with menu
    auto logMenu = Menu(&listEntries_, &focusedIndex_);

    // Add event handlers
    logMenu |= CatchEvent([this](Event event) {
        // Close on Escape
        if (event == Event::Escape) {
            hide();
            return true;
        }

        // Retry selected on R
        if (event == Event::Character('r') || event == Event::Character('R')) {
            const auto& entries = transferLog_.getEntries();
            if (focusedIndex_ >= 0 && focusedIndex_ < static_cast<int>(entries.size())) {
                const auto& entry = entries[focusedIndex_];
                if (entry.status == TransferStatus::Failed && !retryFiles_.empty()) {
                    // Find and retry this file
                    std::vector<ChangeFile> filesToRetry;
                    for (const auto& f : retryFiles_) {
                        if (f.path.string() == entry.filename) {
                            filesToRetry.push_back(f);
                            break;
                        }
                    }
                    if (!filesToRetry.empty() && retryCallback_) {
                        hide();
                        retryCallback_(filesToRetry);
                    }
                }
            }
            return true;
        }

        // Retry all on A
        if (event == Event::Character('a') || event == Event::Character('A')) {
            auto failedFiles = transferLog_.getFailedFiles();
            if (!failedFiles.empty() && !retryFiles_.empty() && retryCallback_) {
                // Filter retry files to only failed ones
                std::vector<ChangeFile> filesToRetry;
                for (const auto& failed : failedFiles) {
                    for (const auto& f : retryFiles_) {
                        if (f.path.string() == failed.filename) {
                            filesToRetry.push_back(f);
                            break;
                        }
                    }
                }
                if (!filesToRetry.empty()) {
                    hide();
                    retryCallback_(filesToRetry);
                }
            }
            return true;
        }

        return false;
    });

    // Renderer
    auto renderer = Renderer(logMenu, [this, logMenu] {
        const auto& entries = transferLog_.getEntries();

        Elements content;

        // Header
        content.push_back(text("Transfer Log") | bold | center);
        content.push_back(separator());
        content.push_back(text(""));

        if (entries.empty()) {
            content.push_back(text("No transfer records") | dim | center);
        } else {
            // Show summary
            int success = transferLog_.getCount(TransferStatus::Success);
            int failed = transferLog_.getCount(TransferStatus::Failed);
            int skipped = transferLog_.getCount(TransferStatus::Skipped);

            std::ostringstream oss;
            oss << "[Files: " << entries.size()
                << "  Success: " << success
                << "  Failed: " << failed
                << "  Skipped: " << skipped << "]";
            content.push_back(text(oss.str()) | dim);
            content.push_back(text(""));
            content.push_back(logMenu->Render() | frame);
        }

        content.push_back(text(""));
        content.push_back(separator());

        // Footer with shortcuts
        content.push_back(hbox({
            text("[↑/↓] Select") | bold,
            text(" | "),
            text("[R] Retry Selected") | bold,
            text(" | "),
            text("[A] Retry All Failed") | bold,
            text(" | "),
            text("[Esc] Close") | bold,
        }) | center);

        return vbox(content) | border | size(ftxui::WIDTH, ftxui::LESS_THAN, 80) | center;
    });

    return renderer;
}

void LogModal::updateListEntries() {
    listEntries_.clear();
    const auto& entries = transferLog_.getEntries();

    for (const auto& entry : entries) {
        std::string line;

        // Status symbol
        switch (entry.status) {
            case TransferStatus::Success:
                line = "✓ ";
                break;
            case TransferStatus::Failed:
                line = "✗ ";
                break;
            case TransferStatus::Skipped:
                line = "○ ";
                break;
        }

        // Filename
        line += entry.filename;

        // Status text
        line += "    ";
        line += formatStatus(entry.status);

        listEntries_.push_back(line);

        // Add error message if failed
        if (entry.status == TransferStatus::Failed && !entry.errorMessage.empty()) {
            listEntries_.push_back("    → " + entry.errorMessage);
        }
    }
}

std::string LogModal::formatStatus(TransferStatus status) {
    switch (status) {
        case TransferStatus::Success: return "Success";
        case TransferStatus::Failed: return "Failed";
        case TransferStatus::Skipped: return "Skipped";
        default: return "Unknown";
    }
}

ftxui::Color LogModal::getStatusColor(TransferStatus status) {
    switch (status) {
        case TransferStatus::Success: return ftxui::Color::Green;
        case TransferStatus::Failed: return ftxui::Color::Red;
        case TransferStatus::Skipped: return ftxui::Color::Yellow;
        default: return ftxui::Color::GrayLight;
    }
}

} // namespace diffsync
```

- [ ] **Step 2: Update CMakeLists.txt**

Add to `CMakeLists.txt` in the sources list:
```cmake
src/tui/log_modal.cpp
```

- [ ] **Step 3: Build to verify**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Clean build

- [ ] **Step 4: Commit**

```bash
git add src/tui/log_modal.cpp CMakeLists.txt
git commit -m "feat: implement LogModal component"
```

---

## Chunk 5: Integrate into MainScreen

### Task 6: Add LogModal to MainScreen

**Files:**
- Modify: `include/diffsync/tui/main_screen.hpp`
- Modify: `src/tui/main_screen.cpp`

- [ ] **Step 1: Add includes and members to header**

Modify `include/diffsync/tui/main_screen.hpp`:

Add includes (after line 7):
```cpp
#include "diffsync/tui/log_modal.hpp"
#include "diffsync/core/transfer_log.hpp"
```

Add members in private section (after line 61):
```cpp
std::unique_ptr<LogModal> logModal_;
bool logModalShown_ = false;        // For log modal
TransferLog transferLog_;          // Transfer log manager
std::vector<ChangeFile> lastSyncFiles_;  // Files from last sync (for retry)
```

Add method declaration (after line 131):
```cpp
/**
 * Get log modal component
 */
ftxui::Component buildLogModal();

/**
 * Retry failed files
 */
void retryFailedFiles(const std::vector<ChangeFile>& files);
```

- [ ] **Step 2: Initialize LogModal in constructor**

Modify `MainScreen` constructor in `src/tui/main_screen.cpp` (around line 35):

```cpp
MainScreen::MainScreen(GitScanner& scanner, ConfigManager& config)
    : configManager_(config), repoRoot_(scanner.repoRoot()),
      configModal_(std::make_unique<ConfigModal>(config)),
      logModal_(std::make_unique<LogModal>(transferLog_,
          [this](const auto& files) { this->retryFailedFiles(files); })) {
    // ... rest of constructor
}
```

- [ ] **Step 3: Implement buildLogModal()**

Add to `src/tui/main_screen.cpp`:

```cpp
ftxui::Component MainScreen::buildLogModal() {
    return (*logModal_)(&logModalShown_);
}
```

- [ ] **Step 4: Implement retryFailedFiles()**

Add to `src/tui/main_screen.cpp`:

```cpp
void MainScreen::retryFailedFiles(const std::vector<ChangeFile>& files) {
    if (files.empty()) {
        syncStatus_ = "No files to retry";
        return;
    }

    // Check config
    if (remoteConfig_.name.empty()) {
        syncStatus_ = "Error: No profile selected";
        return;
    }

    // Store retry files for log modal
    lastSyncFiles_ = files;

    // Use same sync logic but with retry files
    std::unique_lock<std::mutex> lock(syncMutex_);

    if (syncThread_.joinable()) {
        lock.unlock();
        syncThread_.join();
        lock.lock();
    }

    if (syncThreadRunning_) {
        syncStatus_ = "Sync already in progress";
        return;
    }

    syncing_ = true;
    syncComplete_ = false;
    syncStatus_ = "Retrying files...";
    syncProgress_ = 0;
    syncThreadRunning_ = true;

    // Calculate total size
    syncTotalSize_ = 0;
    for (const auto& file : files) {
        syncTotalSize_ += file.size;
    }

    syncModalShown_ = true;

    syncThread_ = std::thread([this, files]() {
        try {
            RsyncExecutor rsync;
            RsyncExecutor::Options options;
            options.compress = true;
            options.showProgress = true;
            options.verbose = true;

            // Add file callback to update log entries
            options.fileCallback = [this](const std::string& filename,
                                          TransferStatus status,
                                          const std::string& error) {
                transferLog_.updateEntry(filename, status, error);
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

            // Reopen log modal to show updated results
            if (screen_) {
                screen_->PostEvent(ftxui::Event::Character(' '));
                logModalShown_ = true;
            }
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
                logModalShown_ = true;
            }
        }
    });
}
```

- [ ] **Step 5: Integrate LogModal into modal stack**

Modify `buildUI()` in `src/tui/main_screen.cpp` (around line 205):

Change:
```cpp
auto modalWrapper = Modal(withHelpModal, syncModal, &syncModalShown_);
```

To:
```cpp
// Wrap with log modal (outermost)
auto withLogModal = Modal(withHelpModal, buildLogModal(), &logModalShown_);
auto modalWrapper = Modal(withLogModal, syncModal, &syncModalShown_);
```

- [ ] **Step 6: Add L key handler**

Modify `buildShortcuts()` in `src/tui/main_screen.cpp` (around line 347):

Add after `if (event == Event::Return)` block:
```cpp
if (event == Event::Character('l') || event == Event::Character('L')) {
    // Open log modal if there are entries
    if (!transferLog_.getEntries().empty()) {
        logModal_->setRetryFiles(lastSyncFiles_);
        logModalShown_ = true;
    }
    return true;
}
```

- [ ] **Step 7: Add file logging to sync**

Modify `startSync()` in `src/tui/main_screen.cpp` (around line 376):

Add after storing selected files:
```cpp
// Store files for potential retry
lastSyncFiles_ = selectedFiles;

// Clear previous log and start new session
transferLog_.clear();
```

Add file callback to options (around line 432):
```cpp
// Add file callback to log transfer results
options.fileCallback = [this](const std::string& filename,
                              TransferStatus status,
                              const std::string& error) {
    transferLog_.logFile(filename, status, error);
};
```

- [ ] **Step 8: Add "Press L" prompt when failures exist**

Modify `buildSyncModal()` renderer in `src/tui/main_screen.cpp` (around line 260):

Add after `Skipped` display:
```cpp
// Add prompt for viewing log if there are failures
if (syncResult_.hasFailures() && transferLog_.hasFailures()) {
    content.push_back(text(""));
    content.push_back(text("Press [L] to view transfer log") | ftxui::color(ftxui::Color::Yellow) | bold);
}
```

- [ ] **Step 9: Build to test**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Clean build

- [ ] **Step 10: Commit**

```bash
git add include/diffsync/tui/main_screen.hpp src/tui/main_screen.cpp
git commit -m "feat: integrate LogModal into MainScreen"
```

---

## Chunk 6: Update Help Text

### Task 7: Add Log Shortcuts to Help Modal

**Files:**
- Modify: `src/tui/main_screen.cpp`

- [ ] **Step 1: Update help modal text**

Modify `buildHelpModal()` in `src/tui/main_screen.cpp` (around line 230):

Add after "Actions:" section:
```cpp
content.push_back(text("  <L>            - View transfer log"));
```

Add at end before closing quotes:
```cpp
content.push_back(text(""));
content.push_back(text("Log Window:") | bold);
content.push_back(text("  <R>            - Retry selected file"));
content.push_back(text("  <A>            - Retry all failed files"));
```

- [ ] **Step 2: Build to verify**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Clean build

- [ ] **Step 3: Commit**

```bash
git add src/tui/main_screen.cpp
git commit -m "docs: add log shortcuts to help modal"
```

---

## Testing Checklist

After implementation, manually test:

- [ ] 1. Start TUI with some changed files
- [ ] 2. Select files and press Enter to sync
- [ ] 3. During sync, verify progress updates
- [ ] 4. After sync completes, press `L` to open log
- [ ] 5. Verify log shows all files with correct status colors
- [ ] 6. If there are failures, verify "Press [L]" prompt appears
- [ ] 7. Try retrying individual failed file with `R`
- [ ] 8. Try retrying all failed files with `A`
- [ ] 9. Verify log updates after retry
- [ ] 10. Start new sync and verify old log is cleared
- [ ] 11. Exit and restart program - verify log is empty
- [ ] 12. Press `?` to verify help shows log shortcuts

---

## Plan Complete

All chunks defined. Total files:
- New: 3 files (transfer_log.hpp/.cpp, log_modal.hpp/.cpp)
- Modified: 4 files (types.hpp, rsync.hpp, rsync.cpp, main_screen.hpp, main_screen.cpp)
