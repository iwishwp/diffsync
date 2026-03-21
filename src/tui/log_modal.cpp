#include "diffsync/tui/log_modal.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/screen/screen.hpp>

namespace diffsync {

LogModal::LogModal(TransferLog& transferLog, RetryCallback retryCallback)
    : transferLog_(transferLog), retryCallback_(retryCallback) {
}

void LogModal::show() {
    if (externalShown_) {
        *externalShown_ = true;
    }
    updateListEntries();
}

void LogModal::hide() {
    if (externalShown_) {
        *externalShown_ = false;
    }
}

void LogModal::setRetryFiles(const std::vector<ChangeFile>& files) {
    retryFiles_ = files;
}

ftxui::Component LogModal::operator()(bool* externalShown) {
    externalShown_ = externalShown;

    using namespace ftxui;

    // Create menu component - listEntries_ must be a member variable
    auto menu = Menu(&listEntries_, &focusedIndex_);

    // Event handler for keyboard shortcuts
    auto withEvents = CatchEvent(menu, [this](Event event) {
        if (event == Event::Escape) {
            hide();
            return true;
        }
        if (event == Event::Character('r') || event == Event::Character('R')) {
            // Retry selected failed file
            auto failedFiles = transferLog_.getFailedFiles();
            if (focusedIndex_ >= 0 && focusedIndex_ < static_cast<int>(failedFiles.size())) {
                std::vector<ChangeFile> filesToRetry;
                // Get the file info from retryFiles_ based on the log entry
                for (const auto& file : retryFiles_) {
                    if (file.path.string() == failedFiles[focusedIndex_].filename) {
                        filesToRetry.push_back(file);
                        break;
                    }
                }
                if (!filesToRetry.empty() && retryCallback_) {
                    retryCallback_(filesToRetry);
                    hide();
                }
            }
            return true;
        }
        if (event == Event::Character('a') || event == Event::Character('A')) {
            // Retry all failed files
            auto failedFiles = transferLog_.getFailedFiles();
            if (!failedFiles.empty() && !retryFiles_.empty() && retryCallback_) {
                // Filter retryFiles_ to only include failed ones
                std::vector<ChangeFile> filesToRetry;
                for (const auto& file : retryFiles_) {
                    for (const auto& failed : failedFiles) {
                        if (file.path.string() == failed.filename) {
                            filesToRetry.push_back(file);
                            break;
                        }
                    }
                }
                if (!filesToRetry.empty()) {
                    retryCallback_(filesToRetry);
                    hide();
                }
            }
            return true;
        }
        return false;
    });

    // Renderer for the modal content
    auto renderer = Renderer(withEvents, [this, menu] {
        Elements content;

        content.push_back(text("Transfer Log") | bold | center);
        content.push_back(separator());
        content.push_back(text(""));

        // Get log entries
        const auto& entries = transferLog_.getEntries();

        if (entries.empty()) {
            content.push_back(text("No transfer log entries") | dim | center);
        } else {
            // Show summary
            int success = transferLog_.getCount(TransferStatus::Success);
            int failed = transferLog_.getCount(TransferStatus::Failed);
            int skipped = transferLog_.getCount(TransferStatus::Skipped);

            Elements summary;
            summary.push_back(text("Summary:") | bold);
            summary.push_back(text("  Total:   " + std::to_string(entries.size())));
            summary.push_back(text("  Success: " + std::to_string(success)));
            if (failed > 0) {
                summary.push_back(text("  Failed:  " + std::to_string(failed)) | color(Color::Red));
            }
            if (skipped > 0) {
                summary.push_back(text("  Skipped: " + std::to_string(skipped)));
            }
            content.push_back(hbox(summary));
            content.push_back(text(""));

            // Show file list
            content.push_back(text("Files:") | bold);
            content.push_back(menu->Render() | flex | vscroll_indicator | frame);
        }

        // Show retry hints if there are failures
        if (transferLog_.hasFailures()) {
            content.push_back(text(""));
            Elements hints;
            hints.push_back(text("Retry:"));
            hints.push_back(text("  <R> - Retry selected failed file"));
            hints.push_back(text("  <A> - Retry all failed files"));
            content.push_back(hbox(hints) | dim);
        }

        content.push_back(text(""));
        content.push_back(text("Press <Esc> to close") | dim | center);

        return vbox(content) | border | size(WIDTH, LESS_THAN, 70) | center;
    });

    return renderer;
}

void LogModal::updateListEntries() {
    listEntries_.clear();

    const auto& entries = transferLog_.getEntries();
    for (const auto& entry : entries) {
        std::string statusSymbol = formatStatus(entry.status);
        std::string display = statusSymbol + " " + entry.filename;
        if (entry.status == TransferStatus::Failed && !entry.errorMessage.empty()) {
            display += " - " + entry.errorMessage;
        }
        listEntries_.push_back(display);
    }
}

std::string LogModal::formatStatus(TransferStatus status) {
    switch (status) {
        case TransferStatus::Success:
            return "✓";  // Checkmark
        case TransferStatus::Failed:
            return "✗";  // X mark
        case TransferStatus::Skipped:
            return "○";  // Circle
        default:
            return "?";
    }
}

} // namespace diffsync
