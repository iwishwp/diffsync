#include "diffsync/core/rsync.hpp"
#include <iostream>
#include <sstream>
#include <array>
#include <memory>
#include <regex>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace diffsync {

std::string RsyncExecutor::escapeShellArg(const std::string& arg) {
    // Single-quote escaping for POSIX shells (same as GitScanner)
    std::string result = "'";
    for (char c : arg) {
        if (c == '\'') {
            result += "'\\''";  // End quote, escaped quote, start quote
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

std::pair<int, int> RsyncExecutor::parseRsyncVersion(const std::string& versionOutput) {
    // Parse version string like "rsync  version 3.2.7  protocol version 31"
    // or "rsync version 2.6.9 compatible" (openrsync)
    std::regex versionRegex(R"(rsync\s+version\s+(\d+)\.(\d+))");
    std::smatch match;

    if (std::regex_search(versionOutput, match, versionRegex) && match.size() >= 3) {
        try {
            int major = std::stoi(match[1].str());
            int minor = std::stoi(match[2].str());
            return {major, minor};
        } catch (...) {
            return {0, 0};
        }
    }

    return {0, 0};
}

Error RsyncExecutor::checkRsyncAvailable() {
    // Check if rsync command exists
    std::string cmd = "rsync --version 2>&1";
    std::array<char, 256> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        return Error(ErrorCode::RsyncNotAvailable, "rsync command not found");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    if (result.empty()) {
        return Error(ErrorCode::RsyncNotAvailable, "rsync command not found");
    }

    // Check version (need >= 3.1.0 for --info=progress2)
    auto [major, minor] = parseRsyncVersion(result);

    if (major < 3 || (major == 3 && minor < 1)) {
        std::ostringstream oss;
        oss << "rsync version >= 3.1.0 required (found " << major << "." << minor << ")";
        return Error(ErrorCode::RsyncNotAvailable, oss.str());
    }

    return Error();
}

std::string RsyncExecutor::formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double dsize = static_cast<double>(bytes);

    while (dsize >= 1024.0 && unit < 4) {
        dsize /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << dsize << units[unit];
    return oss.str();
}

std::string RsyncExecutor::formatSpeed(double mbPerSec) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << mbPerSec << "MB/s";
    return oss.str();
}

std::string RsyncExecutor::buildRsyncCommand(
    const std::vector<ChangeFile>& files,
    const std::filesystem::path& localPath,
    const RemoteConfig& remoteConfig,
    const Options& options
) {
    std::ostringstream cmd;

    // Base rsync command
    cmd << "rsync -av";

    // Compression
    if (options.compress) {
        cmd << "z";
    }

    // Progress info (requires rsync >= 3.1.0)
    if (options.showProgress) {
        cmd << " --info=progress2";
    }

    // Dry run
    if (options.dryRun) {
        cmd << " --dry-run";
    }

    // Verbose
    if (options.verbose) {
        cmd << " --verbose";
    }

    // Include patterns
    for (const auto& file : files) {
        // Escape the path for shell safety
        std::string escapedPath = escapeShellArg(file.path.string());
        cmd << " --include=" << escapedPath;
    }

    // Exclude everything else (only sync included files)
    cmd << " --exclude='*'";

    // SSH options
    std::string sshCmd = "ssh";
    if (options.sshPort != 22) {
        sshCmd += " -p " + std::to_string(options.sshPort);
    }
    if (!options.identityFile.empty()) {
        std::string escapedKey = escapeShellArg(options.identityFile);
        sshCmd += " -i " + escapedKey;
    }

    std::string escapedSshCmd = escapeShellArg(sshCmd);
    cmd << " -e " << escapedSshCmd;

    // Source path (must end with / for rsync)
    std::string escapedLocalPath = escapeShellArg(localPath.string());
    if (escapedLocalPath.back() != '/') {
        escapedLocalPath += "/";
    }
    cmd << " " << escapedLocalPath;

    // Destination
    std::string dest = remoteConfig.user + "@" + remoteConfig.host + ":" + remoteConfig.remotePath + "/";
    std::string escapedDest = escapeShellArg(dest);
    cmd << " " << escapedDest;

    return cmd.str();
}

RsyncExecutor::Progress RsyncExecutor::parseProgress(const std::string& line) {
    Progress progress;

    // Parse rsync progress2 format:
    // "12,345  25%   10.23MB/s    0:00:03  (xfr#4, to-chk=8/20)"
    // or simpler: "1,234,567  50%   5.23MB/s    0:00:05"

    std::string pattern = R"(([0-9,]+)\s+(\d+)%\s+([\d.]+)([A-Z]*/s)\s+([\d:]+)(?:\s+\(xfr#(\d+),\s*to-chk=(\d+)/(\d+)\))?)";
    std::regex progressRegex(pattern);

    std::smatch match;
    if (std::regex_search(line, match, progressRegex) && match.size() >= 6) {
        try {
            // Parse bytes transferred (remove commas)
            std::string bytesStr = match[1].str();
            bytesStr.erase(std::remove(bytesStr.begin(), bytesStr.end(), ','), bytesStr.end());
            progress.bytesTransferred = std::stoull(bytesStr);

            // Parse percentage
            progress.percent = std::stoi(match[2].str());

            // Parse speed (convert to MB/s)
            std::string speedStr = match[3].str();
            std::string unit = match[4].str();
            double speed = std::stod(speedStr);

            if (unit.find("GB") != std::string::npos) {
                progress.speed = speed * 1024.0;  // GB/s to MB/s
            } else if (unit.find("KB") != std::string::npos) {
                progress.speed = speed / 1024.0;  // KB/s to MB/s
            } else {
                progress.speed = speed;  // Already MB/s
            }

            // ETA
            progress.eta = match[5].str();

            // If we have detailed file transfer info
            if (match.size() >= 9) {
                progress.filesTransferred = std::stoi(match[6].str());
                int toCheck = std::stoi(match[7].str());
                (void)std::stoi(match[8].str());  // Total files (not used currently)
                progress.filesRemaining = toCheck;
            }
        } catch (...) {
            // If parsing fails, return default progress
        }
    }

    return progress;
}

RsyncExecutor::ExecResult RsyncExecutor::executeCommand(const std::string& cmd) {
    ExecResult result;

    std::array<char, 4096> buffer;
    std::string stdoutStr;
    std::string stderrStr;

    // Use popen with read mode
    auto start = std::chrono::steady_clock::now();

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        result.exitCode = -1;
        result.stderr = "Failed to execute command";
        return result;
    }

    // Read output
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        stdoutStr += buffer.data();

        // Check for timeout
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        if (elapsed > 30) {  // 30 second timeout
            break;
        }
    }

    // Get exit code
    int exitCode = pclose(pipe.release());
    result.exitCode = WEXITSTATUS(exitCode);
    result.stdout = stdoutStr;
    result.stderr = stderrStr;

    return result;
}

SyncResult RsyncExecutor::execute(
    const std::vector<ChangeFile>& files,
    const std::filesystem::path& localPath,
    const RemoteConfig& remoteConfig,
    const Options& options,
    std::function<void(const Progress&)> progressCallback
) {
    SyncResult result;

    // Validate inputs
    if (files.empty()) {
        result.errors.push_back("No files to sync");
        return result;
    }

    if (!remoteConfig.isValid()) {
        result.errors.push_back("Invalid remote configuration");
        return result;
    }

    // Check rsync availability
    auto rsyncError = checkRsyncAvailable();
    if (rsyncError) {
        result.errors.push_back("rsync check failed: " + rsyncError.message);
        return result;
    }

    // Build command
    std::string cmd = buildRsyncCommand(files, localPath, remoteConfig, options);

    if (options.verbose || options.dryRun) {
        std::cout << "Command: " << cmd << "\n";
    }

    // Execute
    ExecResult execResult = executeCommand(cmd);

    if (options.verbose) {
        std::cout << "Exit code: " << execResult.exitCode << "\n";
        if (!execResult.stdout.empty()) {
            std::cout << "Output:\n" << execResult.stdout << "\n";
        }
        if (!execResult.stderr.empty()) {
            std::cout << "Errors:\n" << execResult.stderr << "\n";
        }
    }

    // Parse progress if callback provided
    if (progressCallback && options.showProgress) {
        std::istringstream stream(execResult.stdout);
        std::string line;

        while (std::getline(stream, line)) {
            if (!line.empty()) {
                Progress progress = parseProgress(line);
                if (progress.percent > 0) {
                    progressCallback(progress);
                }
            }
        }
    }

    // Process results
    result.totalFiles = static_cast<int>(files.size());

    if (execResult.exitCode == 0) {
        result.successCount = result.totalFiles;
    } else {
        result.failedCount = result.totalFiles;
        result.errors.push_back("rsync failed with exit code " + std::to_string(execResult.exitCode));
        if (!execResult.stderr.empty()) {
            result.errors.push_back(execResult.stderr);
        }
    }

    if (options.dryRun) {
        std::cout << "[Dry-run complete - no files actually transferred]\n";
    }

    return result;
}

} // namespace diffsync
