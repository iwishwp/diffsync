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

    // Disable copy and move (mutex is not movable)
    TransferLog(const TransferLog&) = delete;
    TransferLog& operator=(const TransferLog&) = delete;
    TransferLog(TransferLog&&) = delete;
    TransferLog& operator=(TransferLog&&) = delete;

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
