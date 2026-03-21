#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace diffsync {

// File change status from git
enum class FileStatus {
    Modified,    // M - Modified (unstaged)
    Staged,      // Modified after staging
    Added,       // A - New file (staged)
    Untracked,   // ?? - Untracked
    Deleted      // D - Deleted (for reference, not synced)
};

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

// Convert FileStatus to string representation
std::string toString(FileStatus status);
FileStatus parseFileStatus(const std::string& statusStr);

// Single changed file
struct ChangeFile {
    std::filesystem::path path;  // Path relative to repo root
    FileStatus status;
    uint64_t size;               // File size in bytes
    bool selected = false;       // TUI selection state

    // Get display string for status (M, A, ??, etc.)
    std::string statusString() const;
};

// Remote connection configuration
struct RemoteConfig {
    std::string name;           // Profile name
    std::string host;           // Host address
    std::string user;           // Username
    std::string remotePath;     // Remote root directory
    std::string identityFile;   // SSH private key path (optional)
    std::string password;       // SSH password (NOT RECOMMENDED - use key instead)
    uint16_t port = 22;         // SSH port

    // Validate config fields
    bool isValid() const;
    std::string toString() const;
};

// Sync operation result
struct SyncResult {
    int totalFiles = 0;
    int successCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    std::vector<std::string> errors;

    // Detailed file-level results
    std::vector<FileTransferLog> fileResults;

    bool hasErrors() const { return !errors.empty(); }
    bool hasFailures() const { return failedCount > 0; }  // Check if any files failed to transfer
};

// Error codes
enum class ErrorCode {
    NoError,
    NotAGitRepo,
    NoChanges,
    SshConnectionFailed,
    RsyncNotAvailable,
    RemotePathNotFound,
    ConfigCorrupted,
    PathValidationError
};

// Error with context
struct Error {
    ErrorCode code;
    std::string message;

    Error() : code(ErrorCode::NoError) {}
    Error(ErrorCode c, const std::string& msg) : code(c), message(msg) {}
    explicit operator bool() const { return code != ErrorCode::NoError; }
};

} // namespace diffsync
