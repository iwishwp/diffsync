#pragma once

#include "diffsync/types.hpp"
#include "diffsync/core/config.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <functional>

namespace diffsync {

/**
 * RsyncExecutor - Executes rsync to transfer files to remote machines
 *
 * Handles:
 * - Building rsync commands with proper SSH options
 * - Parsing progress information from rsync output
 * - Secure shell argument escaping
 * - Version checking for rsync >= 3.1.0
 */
class RsyncExecutor {
public:
    /**
     * Options for rsync execution
     */
    struct Options {
        bool dryRun = false;
        bool compress = true;        // -z flag
        bool verbose = false;
        std::vector<std::string> excludes;
        std::vector<std::string> includes;
        uint16_t sshPort = 22;
        std::string identityFile;
        int timeout = 30;  // seconds
        bool showProgress = true;    // Show progress updates
        std::function<void(const std::string& filename, TransferStatus status, const std::string& error)> fileCallback;
    };

    /**
     * Progress information during transfer
     */
    struct Progress {
        uint64_t bytesTransferred = 0;
        int percent = 0;
        double speed = 0;  // MB/s
        std::string eta;
        int filesTransferred = 0;
        int filesRemaining = 0;
    };

    /**
     * Result of command execution
     */
    struct ExecResult {
        int exitCode = 0;
        std::string stdout;
        std::string stderr;
    };

    /**
     * Execute rsync to sync files
     * @param files Files to sync
     * @param localPath Local repository root
     * @param remoteConfig Remote configuration
     * @param options Rsync options
     * @param progressCallback Callback for progress updates (optional, called in real-time)
     * @return Sync result
     */
    SyncResult execute(
        const std::vector<ChangeFile>& files,
        const std::filesystem::path& localPath,
        const RemoteConfig& remoteConfig,
        const Options& options,
        std::function<void(const Progress&)> progressCallback
    );

    /**
     * Execute rsync to sync files (with default options)
     */
    SyncResult execute(
        const std::vector<ChangeFile>& files,
        const std::filesystem::path& localPath,
        const RemoteConfig& remoteConfig
    ) {
        return execute(files, localPath, remoteConfig, Options{}, {});
    }

    /**
     * Check if rsync is available and meets version requirements
     * @return Error if rsync not available or version too old
     */
    static Error checkRsyncAvailable();

    /**
     * Format bytes to human readable string
     */
    static std::string formatBytes(uint64_t bytes);

    /**
     * Format speed to MB/s string
     */
    static std::string formatSpeed(double mbPerSec);

private:
    /**
     * Build rsync command string
     */
    std::string buildRsyncCommand(
        const std::vector<ChangeFile>& files,
        const std::filesystem::path& localPath,
        const RemoteConfig& remoteConfig,
        const Options& options
    );

    /**
     * Parse progress line from rsync --info=progress2 output
     * Format: "12,345  25%   10.23MB/s    0:00:03  (xfr#4, to-chk=8/20)"
     */
    Progress parseProgress(const std::string& line);

    /**
     * Execute command and capture output
     */
    ExecResult executeCommand(const std::string& cmd);

    /**
     * Escape shell argument for safe command execution
     * Reuses GitScanner's escaping logic
     */
    static std::string escapeShellArg(const std::string& arg);

    /**
     * Parse rsync version string to get major.minor version
     */
    static std::pair<int, int> parseRsyncVersion(const std::string& versionOutput);

    /**
     * Parse rsync verbose output to extract file status
     * Returns pair<filename, status> or {"", TransferStatus::Skipped} if not a file line
     */
    std::pair<std::string, TransferStatus> parseVerboseLine(const std::string& line);
};

} // namespace diffsync
