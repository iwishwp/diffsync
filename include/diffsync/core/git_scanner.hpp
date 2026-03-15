#pragma once

#include "diffsync/types.hpp"
#include <filesystem>
#include <vector>
#include <optional>

namespace diffsync {

/**
 * GitDiffScanner - Scans git repository for changed files
 *
 * Uses shell git commands to detect:
 * - Unstaged modifications (git diff --name-status)
 * - Staged modifications (git diff --cached --name-status)
 * - Untracked files (git ls-files --others --exclude-standard)
 */
class GitScanner {
public:
    struct Options {
        bool includeDeleted;  // Include deleted files in results
        bool detectRenames;   // Use -M flag for rename detection

        Options() : includeDeleted(false), detectRenames(true) {}
    };

    /**
     * Create a scanner for a repository path
     * @param repoPath Path to git repository (default: current directory)
     */
    explicit GitScanner(const std::filesystem::path& repoPath = ".");

    /**
     * Scan for all changed files
     * @param options Scan options
     * @return Vector of changed files, or Error if not a git repo
     */
    std::vector<ChangeFile> scan(const Options& options = Options{});

    /**
     * Check if the current directory is a git repository
     */
    bool isGitRepo() const;

    /**
     * Get the repository root path
     */
    const std::filesystem::path& repoRoot() const { return repoRoot_; }

private:
    std::filesystem::path repoPath_;
    std::filesystem::path repoRoot_;
    bool isRepo_ = false;

    // Helper methods
    Error findRepoRoot();
    std::vector<ChangeFile> parseDiffOutput(const std::string& output);
    std::vector<ChangeFile> parseUntrackedFiles(const std::string& output);
    uint64_t getFileSize(const std::filesystem::path& path) const;

    // Secure command execution with path escaping
    static std::string execCommand(const std::string& cmd);
    static std::string escapeShellArg(const std::string& arg);
};

} // namespace diffsync
