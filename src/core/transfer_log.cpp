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
