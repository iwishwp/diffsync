#include "diffsync/core/git_scanner.hpp"
#include <cstdlib>
#include <sstream>
#include <array>
#include <memory>
#include <set>
#include <sstream>

namespace diffsync {

namespace {
    constexpr const char* SHELL_SPECIAL_CHARS = " \t\n\r\"'`$\\&|;()<>";
}

std::string GitScanner::escapeShellArg(const std::string& arg) {
    // Single-quote escaping for POSIX shells
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

std::string GitScanner::execCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd.c_str(), "r"), pclose);

    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

GitScanner::GitScanner(const std::filesystem::path& repoPath)
    : repoPath_(repoPath) {
    auto err = findRepoRoot();
    isRepo_ = !err;
}

Error GitScanner::findRepoRoot() {
    // Check if git is available
    if (system("git --version > /dev/null 2>&1") != 0) {
        return Error(ErrorCode::NotAGitRepo, "git command not found");
    }

    // Validate path doesn't contain suspicious patterns
    std::string pathStr = repoPath_.string();
    if (pathStr.find("..") != std::string::npos) {
        return Error(ErrorCode::PathValidationError, "path contains '..'");
    }

    // Use git rev-parse --show-toplevel to find repo root
    // Properly escape the path argument
    std::string escapedPath = escapeShellArg(pathStr);
    std::string cmd = "cd " + escapedPath + " 2>/dev/null && git rev-parse --show-toplevel 2>/dev/null";
    std::string output = execCommand(cmd);

    if (output.empty()) {
        return Error(ErrorCode::NotAGitRepo, "not a git repository");
    }

    // Remove trailing newline
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }

    repoRoot_ = output;
    return Error();
}

bool GitScanner::isGitRepo() const {
    return isRepo_;
}

std::vector<ChangeFile> GitScanner::scan(const Options& options) {
    std::vector<ChangeFile> changes;

    if (!isRepo_) {
        return changes;  // Empty vector for non-repo
    }

    // Properly escape the repo root path
    std::string escapedRoot = escapeShellArg(repoRoot_.string());
    std::string cdCmd = "cd " + escapedRoot;

    // 1. Get unstaged and staged changes
    std::string diffCmd = cdCmd + " && git diff HEAD --name-status";
    if (options.detectRenames) {
        diffCmd += " -M";
    }
    std::string diffOutput = execCommand(diffCmd);
    auto diffChanges = parseDiffOutput(diffOutput);
    changes.insert(changes.end(), diffChanges.begin(), diffChanges.end());

    // 2. Get staged changes (only modifications)
    std::string stagedCmd = cdCmd + " && git diff --cached --name-status";
    if (options.detectRenames) {
        stagedCmd += " -M";
    }
    std::string stagedOutput = execCommand(stagedCmd);
    auto stagedChanges = parseDiffOutput(stagedOutput);
    changes.insert(changes.end(), stagedChanges.begin(), stagedChanges.end());

    // 3. Get untracked files
    std::string untrackedCmd = cdCmd + " && git ls-files --others --exclude-standard";
    std::string untrackedOutput = execCommand(untrackedCmd);
    auto untrackedChanges = parseUntrackedFiles(untrackedOutput);
    changes.insert(changes.end(), untrackedChanges.begin(), untrackedChanges.end());

    // Remove duplicates and filter deleted if not requested
    std::vector<ChangeFile> result;
    std::set<std::string> seen;

    for (auto& file : changes) {
        std::string pathStr = file.path.string();

        // Skip deleted files if not requested
        if (!options.includeDeleted && file.status == FileStatus::Deleted) {
            continue;
        }

        // Skip duplicates (keep first occurrence)
        if (seen.insert(pathStr).second) {
            file.size = getFileSize(repoRoot_ / file.path);
            result.push_back(std::move(file));
        }
    }

    return result;
}

std::vector<ChangeFile> GitScanner::parseDiffOutput(const std::string& output) {
    std::vector<ChangeFile> changes;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Format: STATUS<tab>PATH
        // STATUS can be: M, MM, AM, etc. (first char is primary status)
        size_t tabPos = line.find('\t');
        if (tabPos == std::string::npos) continue;

        std::string statusStr = line.substr(0, tabPos);
        std::string pathStr = line.substr(tabPos + 1);

        // Use first character as status
        FileStatus status = parseFileStatus(statusStr.substr(0, 1));

        ChangeFile file;
        file.path = pathStr;
        file.status = status;
        file.size = 0;  // Will be filled later
        file.selected = false;

        changes.push_back(std::move(file));
    }

    return changes;
}

std::vector<ChangeFile> GitScanner::parseUntrackedFiles(const std::string& output) {
    std::vector<ChangeFile> changes;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        ChangeFile file;
        file.path = line;
        file.status = FileStatus::Untracked;
        file.size = 0;  // Will be filled later
        file.selected = false;

        changes.push_back(std::move(file));
    }

    return changes;
}

uint64_t GitScanner::getFileSize(const std::filesystem::path& path) const {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}

} // namespace diffsync
