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

    // Use sshpass if password is provided (non-interactive)
    // WARNING: Password will be visible in process list!
    if (!remoteConfig.password.empty()) {
        std::string escapedPassword = escapeShellArg(remoteConfig.password);
        cmd << "sshpass -p " << escapedPassword << " ";
    }

    // Base rsync command
    cmd << "rsync -av";

    // Compression
    if (options.compress) {
        cmd << "z";
    }

    // Progress info (requires rsync >= 3.1.0)
    // Use both --progress and --info=progress2 for detailed progress
    if (options.showProgress) {
        cmd << " --progress --info=progress2";
    }

    // Dry run
    if (options.dryRun) {
        cmd << " --dry-run";
    }

    // Verbose
    if (options.verbose) {
        cmd << " --verbose";
    }

    // SSH options
    std::string sshCmd = "ssh";
    if (remoteConfig.port != 22) {
        sshCmd += " -p " + std::to_string(remoteConfig.port);
    }
    if (!remoteConfig.identityFile.empty()) {
        std::string escapedKey = escapeShellArg(remoteConfig.identityFile);
        sshCmd += " -i " + escapedKey;
    }

    std::string escapedSshCmd = escapeShellArg(sshCmd);
    cmd << " -e " << escapedSshCmd;

    // Add source files directly (not using include/exclude patterns)
    // Each file is a separate source argument
    for (const auto& file : files) {
        std::string fullPath = (localPath / file.path).string();
        std::string escapedPath = escapeShellArg(fullPath);
        cmd << " " << escapedPath;
    }

    // Destination
    std::string dest = remoteConfig.user + "@" + remoteConfig.host + ":" + remoteConfig.remotePath + "/";
    std::string escapedDest = escapeShellArg(dest);
    cmd << " " << escapedDest;

    return cmd.str();
}

RsyncExecutor::Progress RsyncExecutor::parseProgress(const std::string& line) {
    Progress progress;

    // Remove leading carriage returns and whitespace
    // rsync progress lines often start with \r for overwriting
    std::string cleanedLine = line;
    size_t start = 0;
    while (start < cleanedLine.size() && (cleanedLine[start] == '\r' || cleanedLine[start] == ' ')) {
        start++;
    }
    if (start > 0) {
        cleanedLine = cleanedLine.substr(start);
    }

    // Try to parse (xfr#N, to-chk=M/T) pattern first (part of progress2 output)
    std::regex xfrRegex(R"(\(xfr#(\d+),\s*to-chk=(\d+)/(\d+)\))");
    std::smatch xfrMatch;
    if (std::regex_search(cleanedLine, xfrMatch, xfrRegex) && xfrMatch.size() >= 4) {
        try {
            progress.filesTransferred = std::stoi(xfrMatch[1].str());
            int toCheck = std::stoi(xfrMatch[2].str());
            int total = std::stoi(xfrMatch[3].str());
            progress.filesRemaining = toCheck;

            // Calculate percentage from file count
            if (total > 0) {
                progress.percent = (progress.filesTransferred * 100) / total;
            }
        } catch (...) {
            // If parsing fails, continue to try full progress2 format
        }
    }

    // Parse full rsync progress2 format:
    // "12,345  25%   10.23MB/s    0:00:03  (xfr#4, to-chk=8/20)"
    // or simpler: "1,234,567  50%   5.23MB/s    0:00:05"

    std::string pattern = R"(([0-9,]+)\s+(\d+)%\s+([\d.]+)([A-Z]*/s)\s+([\d:]+)(?:\s+\(xfr#(\d+),\s*to-chk=(\d+)/(\d+)\))?)";
    std::regex progressRegex(pattern);

    std::smatch match;
    if (std::regex_search(cleanedLine, match, progressRegex) && match.size() >= 6) {
        try {
            // Parse bytes transferred (remove commas)
            std::string bytesStr = match[1].str();
            bytesStr.erase(std::remove(bytesStr.begin(), bytesStr.end(), ','), bytesStr.end());
            progress.bytesTransferred = std::stoull(bytesStr);

            // Parse percentage (overrides file count calculation)
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
            // If parsing fails, return default progress (may have xfr info from above)
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

    // Read output line by line
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        stdoutStr += buffer.data();

        // Check for timeout
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        if (elapsed > 300) {  // 5 minute timeout (was 30s, too short for large files)
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

    // Check sshpass availability if password is provided
    if (!remoteConfig.password.empty()) {
        std::string sshpassCmd = "sshpass -V 2>&1";
        std::array<char, 256> buffer;
        std::string sshpassOutput;

        std::unique_ptr<FILE, decltype(&pclose)> pipe(
            popen(sshpassCmd.c_str(), "r"), pclose);

        if (!pipe || fgets(buffer.data(), buffer.size(), pipe.get()) == nullptr) {
            result.errors.push_back("Password is configured but sshpass is not installed. "
                                    "Install sshpass or use SSH key authentication instead.");
            return result;
        }
    }

    // Build command
    std::string cmd = buildRsyncCommand(files, localPath, remoteConfig, options);

    // Redirect stderr to stdout so we can capture error messages
    cmd += " 2>&1";

    // Execute and read output in real-time
    std::array<char, 4096> buffer;
    std::string stdoutStr;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        result.errors.push_back("Failed to execute rsync command");
        result.failedCount = static_cast<int>(files.size());
        return result;
    }

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

    // Get exit code
    int exitCode = pclose(pipe.release());
    int actualExitCode = WEXITSTATUS(exitCode);

    // Process results
    result.totalFiles = static_cast<int>(files.size());

    if (actualExitCode == 0) {
        result.successCount = result.totalFiles;
    } else {
        result.failedCount = result.totalFiles;

        // Add exit code error
        std::string exitCodeMsg = "rsync failed with exit code " + std::to_string(actualExitCode);
        if (actualExitCode == 255) {
            exitCodeMsg += " (SSH connection failed or rsync error)";
        }
        result.errors.push_back(exitCodeMsg);

        // Add output/stderr as error details (stderr is now redirected to stdout)
        if (!stdoutStr.empty()) {
            result.errors.push_back(stdoutStr);
        }
    }

    return result;
}

} // namespace diffsync
