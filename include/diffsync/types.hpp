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

    bool hasErrors() const { return !errors.empty(); }
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
