# diffsync Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a C++ command-line tool that synchronizes local code changes to a remote build machine based on `git diff`, with both CLI and TUI modes.

**Architecture:** The tool uses a layered architecture with CLI/TUI frontends, core engine (GitDiffScanner, ConfigManager, RsyncExecutor), and external command execution (git, rsync, ssh). All git operations use shell commands rather than libgit2 for simplicity and proven edge-case handling.

**Tech Stack:** C++20, FTXUI (TUI), toml++ (config), CLI11 (CLI parsing), CMake 3.20+

---

## Chunk 1: Project Setup and Core Types

This chunk sets up the project structure, build system, and core data structures.

### File Structure for Chunk 1

```
diffsync/
├── CMakeLists.txt              # Main build config
├── cmake/
│   └── FetchContent.cmake      # Dependency management
├── src/
│   └── main.cpp                # Entry point (minimal)
├── include/diffsync/
│   └── types.hpp               # Core data structures
└── tests/unit/
    └── test_types.cpp          # Types unit tests
```

### Task 1.1: Create CMakeLists.txt with dependency fetching

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/FetchContent.cmake`

- [ ] **Step 1: Write the CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(diffsync VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_TUI "Build TUI components" ON)

# Dependencies
include(cmake/FetchContent.cmake)

# FTXUI for TUI
if(BUILD_TUI)
    FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v5.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(ftxui)
endif()

# toml++ for config
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# CLI11 for CLI parsing
FetchContent_Declare(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cli11)

# Source files
set(SOURCES
    src/main.cpp
)

# Header files
set(HEADERS
    include/diffsync/types.hpp
)

# Main executable
add_executable(diffsync ${SOURCES} ${HEADERS})

target_include_directories(diffsync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(diffsync PRIVATE
    CLI11::CLI11
    tomlplusplus::tomlplusplus
)

if(BUILD_TUI)
    target_link_libraries(diffsync PRIVATE ftxui::screen ftxui::component)
    target_compile_definitions(diffsync PRIVATE BUILD_TUI=1)
endif()

# Testing
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(diffsync PRIVATE /W4)
else()
    target_compile_options(diffsync PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

- [ ] **Step 2: Write cmake/FetchContent.cmake**

```cmake
include(FetchContent)

# Set policy to suppress deprecation warnings in older CMake versions
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 NEW)
endif()
```

- [ ] **Step 3: Create minimal main.cpp**

```cpp
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "diffsync v0.1.0\n";
    return 0;
}
```

- [ ] **Step 4: Create tests directory structure**

```bash
mkdir -p tests/unit
```

- [ ] **Step 5: Create tests/CMakeLists.txt**

```cmake
# Simple test framework (we'll use catch2 later, for now just compile checks)
# Placeholder for test structure
```

- [ ] **Step 6: Try to configure and build**

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Expected: Successful build with dependencies downloaded

- [ ] **Step 7: Run the executable**

```bash
./build/diffsync
```

Expected output: `diffsync v0.1.0`

- [ ] **Step 8: Initialize git and commit**

```bash
git add CMakeLists.txt cmake/ src/ tests/ include/
git commit -m "feat: set up project structure with CMake and dependencies"
```

### Task 1.2: Define core data types

**Files:**
- Create: `include/diffsync/types.hpp`

- [ ] **Step 1: Write the types header**

```cpp
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
```

- [ ] **Step 2: Create src/types.cpp for implementations**

```cpp
#include "diffsync/types.hpp"
#include <sstream>
#include <algorithm>

namespace diffsync {

std::string toString(FileStatus status) {
    switch (status) {
        case FileStatus::Modified:   return "M";
        case FileStatus::Staged:     return "M ";  // Two chars to distinguish
        case FileStatus::Added:      return "A";
        case FileStatus::Untracked:  return "??";
        case FileStatus::Deleted:    return "D";
        default:                     return "?";
    }
}

FileStatus parseFileStatus(const std::string& statusStr) {
    if (statusStr == "M")  return FileStatus::Modified;
    if (statusStr == "A")  return FileStatus::Added;
    if (statusStr == "??") return FileStatus::Untracked;
    if (statusStr == "D")  return FileStatus::Deleted;
    // Default for staged modifications
    return FileStatus::Staged;
}

std::string ChangeFile::statusString() const {
    return toString(status);
}

bool RemoteConfig::isValid() const {
    if (name.empty()) return false;
    if (host.empty()) return false;
    if (user.empty()) return false;
    if (remotePath.empty()) return false;
    if (port == 0 || port > 65535) return false;
    return true;
}

std::string RemoteConfig::toString() const {
    std::ostringstream oss;
    oss << user << "@" << host << ":" << port << remotePath;
    return oss.str();
}

} // namespace diffsync
```

- [ ] **Step 3: Update CMakeLists.txt to include types.cpp**

```cmake
# In the SOURCES section, add:
set(SOURCES
    src/main.cpp
    src/types.cpp
)
```

- [ ] **Step 4: Update main.cpp to test the types**

```cpp
#include <iostream>
#include "diffsync/types.hpp"

int main() {
    using namespace diffsync;

    // Test FileStatus
    std::cout << "FileStatus tests:\n";
    std::cout << "  Modified: " << toString(FileStatus::Modified) << "\n";
    std::cout << "  Added: " << toString(FileStatus::Added) << "\n";
    std::cout << "  Untracked: " << toString(FileStatus::Untracked) << "\n";

    // Test RemoteConfig
    RemoteConfig config;
    config.name = "test";
    config.host = "localhost";
    config.user = "builder";
    config.remotePath = "~/build";
    config.port = 22;

    std::cout << "\nRemoteConfig test:\n";
    std::cout << "  Valid: " << (config.isValid() ? "yes" : "no") << "\n";
    std::cout << "  String: " << config.toString() << "\n";

    return 0;
}
```

- [ ] **Step 5: Build and verify**

```bash
cmake --build build
./build/diffsync
```

Expected output includes status characters and config string.

- [ ] **Step 6: Create unit tests for types**

```cpp
// tests/unit/test_types.cpp
#include <cassert>
#include <iostream>
#include "diffsync/types.hpp"

using namespace diffsync;

void testFileStatus() {
    assert(toString(FileStatus::Modified) == "M");
    assert(toString(FileStatus::Added) == "A");
    assert(toString(FileStatus::Untracked) == "??");
    assert(toString(FileStatus::Deleted) == "D");

    assert(parseFileStatus("M") == FileStatus::Modified);
    assert(parseFileStatus("A") == FileStatus::Added);
    assert(parseFileStatus("??") == FileStatus::Untracked);

    std::cout << "FileStatus tests passed\n";
}

void testRemoteConfig() {
    RemoteConfig config;
    assert(!config.isValid());  // Empty config is invalid

    config.name = "test";
    config.host = "localhost";
    config.user = "user";
    config.remotePath = "/path";
    config.port = 22;
    assert(config.isValid());

    // Test invalid port
    config.port = 0;
    assert(!config.isValid());
    config.port = 70000;
    assert(!config.isValid());

    std::cout << "RemoteConfig tests passed\n";
}

void testChangeFile() {
    ChangeFile file;
    file.path = "src/main.cpp";
    file.status = FileStatus::Modified;
    file.size = 1024;
    file.selected = false;

    assert(file.statusString() == "M");
    assert(file.path == "src/main.cpp");
    assert(file.size == 1024);

    std::cout << "ChangeFile tests passed\n";
}

int main() {
    testFileStatus();
    testRemoteConfig();
    testChangeFile();
    std::cout << "\nAll tests passed!\n";
    return 0;
}
```

- [ ] **Step 7: Update tests/CMakeLists.txt**

```cmake
# Simple test executable
add_executable(test_types
    unit/test_types.cpp
    ../src/types.cpp
)

target_include_directories(test_types PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME test_types COMMAND test_types)
```

- [ ] **Step 8: Build and run tests**

```bash
cmake --build build
ctest --test-dir build --verbose
```

Expected: All tests pass

- [ ] **Step 9: Clean up main.cpp (remove test code)**

```cpp
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "diffsync v0.1.0\n";
    return 0;
}
```

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "feat: add core data types with unit tests"
```

---

**Chunk 1 Complete!** At this point we have:
- Working CMake build system with dependencies
- Core data structures defined and tested
- Project foundation ready for implementation

---

## Chunk 2: GitDiffScanner - Change Detection

This chunk implements the GitDiffScanner component that detects changed files by calling git commands.

### File Structure for Chunk 2

```
diffsync/
├── include/diffsync/
│   └── core/
│       └── git_scanner.hpp      # Git scanner interface
├── src/core/
│   └── git_scanner.cpp          # Git scanner implementation
└── tests/unit/
    └── test_git_scanner.cpp     # Git scanner tests (mocked)
```

### Task 2.1: Create GitScanner interface and implementation

**Files:**
- Create: `include/diffsync/core/git_scanner.hpp`
- Create: `src/core/git_scanner.cpp`

- [ ] **Step 1: Write the GitScanner header**

```cpp
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
 * - Unstaged modifications (git diff HEAD --name-status)
 * - Staged modifications (git diff --cached --name-status)
 * - Untracked files (git ls-files --others --exclude-standard)
 */
class GitScanner {
public:
    struct Options {
        bool includeDeleted = false;  // Include deleted files in results
        bool detectRenames = true;    // Use -M flag for rename detection
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
    std::vector<ChangeFile> scan(const Options& options = {});

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
```

- [ ] **Step 2: Write the GitScanner implementation**

```cpp
#include "diffsync/core/git_scanner.hpp"
#include <cstdlib>
#include <sstream>
#include <array>
#include <memory>
#include <fstream>
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
```

- [ ] **Step 3: Update CMakeLists.txt to include new sources**

```cmake
# Add core source files
set(SOURCES
    src/main.cpp
    src/types.cpp
    src/core/git_scanner.cpp
)

# Update HEADERS
set(HEADERS
    include/diffsync/types.hpp
    include/diffsync/core/git_scanner.hpp
)
```

- [ ] **Step 4: Create integration test (runs against real git)**

```cpp
// tests/integration/test_git_scanner_integration.cpp
#include <iostream>
#include <cassert>
#include "diffsync/core/git_scanner.hpp"

using namespace diffsync;

void testGitScannerIntegration() {
    // This is an integration test - runs real git commands
    GitScanner scanner(".");

    if (!scanner.isGitRepo()) {
        std::cout << "Skipping test: not in a git repo\n";
        return;
    }

    std::cout << "Repo root: " << scanner.repoRoot() << "\n";

    auto changes = scanner.scan();
    std::cout << "Found " << changes.size() << " changed files\n";

    for (const auto& file : changes) {
        std::cout << "  " << file.statusString() << " "
                  << file.path << " (" << file.size << " bytes)\n";
    }
}

int main() {
    testGitScannerIntegration();
    return 0;
}
```

- [ ] **Step 5: Update tests/CMakeLists.txt**

```cmake
# Add git scanner integration test
add_executable(test_git_scanner_integration
    integration/test_git_scanner_integration.cpp
    ../../src/types.cpp
    ../../src/core/git_scanner.cpp
)

target_include_directories(test_git_scanner_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

add_test(NAME test_git_scanner_integration COMMAND test_git_scanner_integration)
```

- [ ] **Step 6: Build and test**

```bash
cmake --build build
ctest --test-dir build --test-dir build -R test_git_scanner_integration -V
```

Expected: Lists changed files in current repo

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: implement GitScanner with secure command execution"
```

---

**Chunk 2 Complete!** At this point we have:
- GitScanner that detects all changed files
- Support for unstaged, staged, and untracked files
- Proper error handling for non-git repositories

---

## Chunk 3: ConfigManager - Configuration Management

This chunk implements the ConfigManager for handling remote connection profiles.

### File Structure for Chunk 3

```
diffsync/
├── include/diffsync/
│   └── core/
│       └── config.hpp            # Config manager interface
├── src/core/
│   └── config.cpp                # Config manager implementation
└── tests/unit/
    └── test_config.cpp           # Config tests
```

### Task 3.1: Create ConfigManager interface

**Files:**
- Create: `include/diffsync/core/config.hpp`
- Create: `src/core/config.cpp`

- [ ] **Step 1: Write the ConfigManager header**

```cpp
#pragma once

#include "diffsync/types.hpp"
#include <vector>
#include <optional>
#include <filesystem>

namespace diffsync {

/**
 * ConfigManager - Manages remote connection profiles
 *
 * Config file location: ~/.config/diffsync/config.toml
 */
class ConfigManager {
public:
    static constexpr const char* DEFAULT_CONFIG_DIR = ".config/diffsync";
    static constexpr const char* CONFIG_FILENAME = "config.toml";
    static constexpr int CURRENT_SCHEMA_VERSION = 1;

    /**
     * Create config manager
     * @param configPath Custom config path (optional)
     */
    explicit ConfigManager(const std::filesystem::path& configPath = {});

    /**
     * Load config from file
     * @return Error if config is corrupted
     */
    Error load();

    /**
     * Save config to file
     * @return Error if save fails
     */
    Error save();

    /**
     * Get all profiles
     */
    const std::vector<RemoteConfig>& profiles() const { return profiles_; }

    /**
     * Get current profile name
     */
    const std::string& currentProfile() const { return currentProfile_; }

    /**
     * Set current profile by name
     * @return Error if profile not found
     */
    Error setCurrentProfile(const std::string& name);

    /**
     * Get current profile config
     */
    std::optional<RemoteConfig> getCurrentConfig() const;

    /**
     * Add or update a profile (validates before adding)
     * @return Error if config is invalid
     */
    Error addProfile(const RemoteConfig& config);

    /**
     * Remove a profile
     * @return Error if removing current profile or profile not found
     */
    Error removeProfile(const std::string& name);

    /**
     * Get default exclude patterns
     */
    const std::vector<std::string>& defaultExcludes() const { return defaultExcludes_; }

    /**
     * Check if config file exists
     */
    bool configExists() const;

    /**
     * Get config file path
     */
    const std::filesystem::path& configPath() const { return configPath_; }

    /**
     * Create default config directory and file
     */
    Error createDefault();

private:
    std::filesystem::path configPath_;
    std::vector<RemoteConfig> profiles_;
    std::string currentProfile_;
    std::vector<std::string> defaultExcludes_;
    int schemaVersion_ = CURRENT_SCHEMA_VERSION;

    Error ensureConfigDirectory();
    std::filesystem::path getDefaultConfigPath() const;
    Error migrateFromVersion(int fromVersion);
    bool validateFilePermissions();
};

} // namespace diffsync
```

- [ ] **Step 2: Write the ConfigManager implementation (simplified TOML parser)**

```cpp
#include "diffsync/core/config.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <system_error>
#include <sys/file.h>
#include <fcntl.h>
#include <chrono>
#include <thread>

namespace diffsync {

namespace {
    std::string expandTilde(const std::string& path) {
        if (path.empty() || path[0] != '~') return path;
        const char* home = std::getenv("HOME");
        if (!home) home = std::getenv("USERPROFILE");
        if (home) return std::string(home) + path.substr(1);
        return path;
    }

    // Simple TOML value extractor (handles basic string and int values)
    std::string extractTomlValue(const std::string& line) {
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) return "";

        std::string value = line.substr(eqPos + 1);

        // Trim whitespace
        size_t start = value.find_first_not_of(" \t");
        size_t end = value.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            value = value.substr(start, end - start + 1);
        }

        // Remove quotes if present
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        return value;
    }

    // Safe integer parsing with error handling
    std::optional<int> parsePort(const std::string& value) {
        try {
            int port = std::stoi(value);
            if (port > 0 && port <= 65535) return port;
        } catch (...) {
            return std::nullopt;
        }
        return std::nullopt;
    }
}

ConfigManager::ConfigManager(const std::filesystem::path& configPath) {
    if (configPath.empty()) {
        configPath_ = getDefaultConfigPath();
    } else {
        configPath_ = configPath;
    }

    defaultExcludes_ = {".git/*", "*.o", "*.a", "build/*", "node_modules/*"};
}

std::filesystem::path ConfigManager::getDefaultConfigPath() const {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    if (!home) return ".diffsync/config.toml";

    std::filesystem::path configDir(home);
    configDir /= DEFAULT_CONFIG_DIR;
    configDir /= CONFIG_FILENAME;
    return configDir;
}

Error ConfigManager::ensureConfigDirectory() {
    std::filesystem::path dir = configPath_.parent_path();
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            return Error(ErrorCode::ConfigCorrupted,
                "failed to create config directory: " + ec.message());
        }
    }
    return Error();
}

bool ConfigManager::configExists() const {
    return std::filesystem::exists(configPath_);
}

Error ConfigManager::createDefault() {
    auto err = ensureConfigDirectory();
    if (err) return err;

    RemoteConfig example;
    example.name = "example";
    example.host = "build.example.com";
    example.user = "builder";
    example.remotePath = "~/projects/myapp";
    example.port = 22;

    profiles_.push_back(example);
    currentProfile_ = "example";

    return save();
}

Error ConfigManager::migrateFromVersion(int fromVersion) {
    // Schema version 1 is initial version, no migration needed yet
    // Future versions add migration logic here
    return Error();
}

bool ConfigManager::validateFilePermissions() {
    if (!configExists()) return true;

    auto perms = std::filesystem::status(configPath_).permissions();
    bool hasGroupRead = (perms & std::filesystem::perms::group_read) != std::filesystem::perms::none;
    bool hasOtherRead = (perms & std::filesystem::perms::others_read) != std::filesystem::perms::none;

    if (hasGroupRead || hasOtherRead) {
        std::cerr << "Warning: Config file has open permissions. Consider running: chmod 600 "
                  << configPath_ << "\n";
        return false;
    }
    return true;
}

Error ConfigManager::load() {
    if (!configExists()) {
        return Error(ErrorCode::ConfigCorrupted, "config file does not exist");
    }

    std::ifstream file(configPath_);
    if (!file.is_open()) {
        return Error(ErrorCode::ConfigCorrupted, "cannot open config file");
    }

    profiles_.clear();
    currentProfile_.clear();
    schemaVersion_ = 1;  // Default

    validateFilePermissions();

    std::string line;
    bool inProfiles = false;
    RemoteConfig currentProfile;
    currentProfile.port = 22;  // Default

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Parse schema_version
        if (line.find("schema_version") == 0) {
            auto value = extractTomlValue(line);
            try { schemaVersion_ = std::stoi(value); } catch (...) {}
            continue;
        }

        // Parse current_profile
        if (line.find("current_profile") == 0) {
            currentProfile_ = extractTomlValue(line);
            continue;
        }

        // Detect [[profiles]] section
        if (line.find("[[profiles]]") == 0) {
            if (currentProfile.isValid()) {
                profiles_.push_back(currentProfile);
            }
            currentProfile = RemoteConfig{};
            currentProfile.port = 22;
            inProfiles = true;
            continue;
        }

        // Parse profile fields
        if (inProfiles) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string valueStr = line.substr(eqPos + 1);

                size_t kEnd = key.find_last_not_of(" \t");
                if (kEnd != std::string::npos) key = key.substr(0, kEnd + 1);

                std::string value = extractTomlValue(line);

                if (key == "name") currentProfile.name = value;
                else if (key == "host") currentProfile.host = value;
                else if (key == "user") currentProfile.user = value;
                else if (key == "remote_path") currentProfile.remotePath = value;
                else if (key == "identity_file") currentProfile.identityFile = expandTilde(value);
                else if (key == "port") {
                    auto port = parsePort(value);
                    if (port) currentProfile.port = *port;
                }
            }
        }
    }

    // Add last profile
    if (currentProfile.isValid()) {
        profiles_.push_back(currentProfile);
    }

    // Set current profile if not set
    if (currentProfile_.empty() && !profiles_.empty()) {
        currentProfile_ = profiles_[0].name;
    }

    // Handle schema migration
    if (schemaVersion_ < CURRENT_SCHEMA_VERSION) {
        auto err = migrateFromVersion(schemaVersion_);
        if (err) return err;
    }

    // Validate all loaded profiles
    for (const auto& profile : profiles_) {
        if (!profile.isValid()) {
            return Error(ErrorCode::ConfigCorrupted, "invalid profile found in config");
        }
    }

    return Error();
}

Error ConfigManager::save() {
    auto err = ensureConfigDirectory();
    if (err) return err;

    // Open with timeout for file locking
    int fd = open(configPath_.c_str(), O_WRONLY | O_CREAT, 0600);
    if (fd == -1) {
        return Error(ErrorCode::ConfigCorrupted, "cannot open config for writing");
    }

    // Try to get lock with 5 second timeout
    int maxRetries = 50;  // 50 * 100ms = 5 seconds
    for (int i = 0; i < maxRetries; ++i) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) break;
        if (i == maxRetries - 1) {
            close(fd);
            return Error(ErrorCode::ConfigCorrupted, "config file is locked (timeout)");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::ofstream file(configPath_);
    if (!file.is_open()) {
        flock(fd, LOCK_UN);
        close(fd);
        return Error(ErrorCode::ConfigCorrupted, "cannot write config file");
    }

    file << "# diffsync configuration\n";
    file << "schema_version = " << CURRENT_SCHEMA_VERSION << "\n";
    file << "current_profile = \"" << currentProfile_ << "\"\n\n";
    file << "default_excludes = [\n";
    for (const auto& excl : defaultExcludes_) {
        file << "    \"" << excl << "\",\n";
    }
    file << "]\n\n";

    for (const auto& profile : profiles_) {
        file << "[[profiles]]\n";
        file << "name = \"" << profile.name << "\"\n";
        file << "host = \"" << profile.host << "\"\n";
        file << "user = \"" << profile.user << "\"\n";
        file << "remote_path = \"" << profile.remotePath << "\"\n";
        file << "port = " << profile.port << "\n";
        if (!profile.identityFile.empty()) {
            file << "identity_file = \"" << profile.identityFile << "\"\n";
        }
        file << "\n";
    }

    file.close();
    flock(fd, LOCK_UN);
    close(fd);

    // Set secure permissions
    std::filesystem::permissions(configPath_,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);

    return Error();
}

Error ConfigManager::setCurrentProfile(const std::string& name) {
    for (const auto& profile : profiles_) {
        if (profile.name == name) {
            currentProfile_ = name;
            return Error();
        }
    }
    return Error(ErrorCode::ConfigCorrupted, "profile not found: " + name);
}

std::optional<RemoteConfig> ConfigManager::getCurrentConfig() const {
    for (const auto& profile : profiles_) {
        if (profile.name == currentProfile_) {
            return profile;
        }
    }
    return std::nullopt;
}

Error ConfigManager::addProfile(const RemoteConfig& config) {
    // Validate before adding
    if (!config.isValid()) {
        return Error(ErrorCode::ConfigCorrupted, "invalid config: missing required fields");
    }

    // Remove existing profile with same name
    auto it = std::remove_if(profiles_.begin(), profiles_.end(),
        [&config](const RemoteConfig& p) { return p.name == config.name; });
    profiles_.erase(it, profiles_.end());

    profiles_.push_back(config);

    if (profiles_.size() == 1 || currentProfile_.empty()) {
        currentProfile_ = config.name;
    }

    return Error();
}

Error ConfigManager::removeProfile(const std::string& name) {
    if (name == currentProfile_) {
        return Error(ErrorCode::ConfigCorrupted, "cannot remove current profile");
    }

    auto it = std::remove_if(profiles_.begin(), profiles_.end(),
        [&name](const RemoteConfig& p) { return p.name == name; });

    if (it == profiles_.end()) {
        return Error(ErrorCode::ConfigCorrupted, "profile not found: " + name);
    }

    profiles_.erase(it, profiles_.end());
    return Error();
}

} // namespace diffsync
```

- [ ] **Step 3: Update CMakeLists.txt**

```cmake
# Add config source
set(SOURCES
    src/main.cpp
    src/types.cpp
    src/core/git_scanner.cpp
    src/core/config.cpp
)

# Update HEADERS
set(HEADERS
    include/diffsync/types.hpp
    include/diffsync/core/git_scanner.hpp
    include/diffsync/core/config.hpp
)
```

- [ ] **Step 4: Create config test with edge cases**

```cpp
// tests/unit/test_config.cpp
#include <iostream>
#include <cassert>
#include <filesystem>
#include "diffsync/core/config.hpp"

using namespace diffsync;

void testConfigManager() {
    std::filesystem::path tempConfig = "/tmp/diffsync_test_config.toml";
    std::filesystem::remove(tempConfig);

    // Test create default
    {
        ConfigManager config(tempConfig);
        auto err = config.createDefault();
        assert(!err && "createDefault should succeed");
        assert(config.configExists());
        assert(!config.profiles().empty());
    }

    // Test load and validate
    {
        ConfigManager config(tempConfig);
        auto err = config.load();
        assert(!err && "load should succeed");

        // Test add profile with validation
        RemoteConfig valid;
        valid.name = "test";
        valid.host = "test.com";
        valid.user = "testuser";
        valid.remotePath = "/test";
        valid.port = 2222;

        err = config.addProfile(valid);
        assert(!err && "addProfile with valid config should succeed");

        // Test invalid profile (missing required field)
        RemoteConfig invalid;
        invalid.name = "invalid";
        // Missing host - should fail
        invalid.user = "user";
        invalid.remotePath = "/path";
        invalid.port = 22;

        err = config.addProfile(invalid);
        assert(err && "addProfile with invalid config should fail");

        // Test port validation
        RemoteConfig badPort;
        badPort.name = "badport";
        badPort.host = "host";
        badPort.user = "user";
        badPort.remotePath = "/path";
        badPort.port = 0;

        err = config.addProfile(badPort);
        assert(err && "addProfile with port=0 should fail");
    }

    // Test current profile
    {
        ConfigManager config(tempConfig);
        config.load();

        auto current = config.getCurrentConfig();
        assert(current.has_value());

        auto err = config.setCurrentProfile("test");
        assert(!err && "setCurrentProfile should succeed");
        assert(config.currentProfile() == "test");

        err = config.removeProfile("test");
        assert(err && "cannot remove current profile");

        err = config.setCurrentProfile("example");
        err = config.removeProfile("test");
        assert(!err && "removeProfile should succeed");
    }

    // Test malformed config handling
    {
        std::ofstream badConfig("/tmp/bad_config.toml");
        badConfig << "current_profile = \"missing\"\n";
        badConfig << "[[profiles]]\n";
        badConfig << "name = \"incomplete\"\n";
        badConfig << "# Missing host, user, remote_path\n";
        badConfig.close();

        ConfigManager bad("/tmp/bad_config.toml");
        auto err = bad.load();
        assert(err && "loading malformed config should fail");

        std::filesystem::remove("/tmp/bad_config.toml");
    }

    std::filesystem::remove(tempConfig);
    std::cout << "ConfigManager tests passed\n";
}

int main() {
    testConfigManager();
    return 0;
}
```

- [ ] **Step 5: Update tests/CMakeLists.txt**

```cmake
# Add config test
add_executable(test_config
    unit/test_config.cpp
    ../../src/types.cpp
    ../../src/core/config.cpp
)

target_include_directories(test_config PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

add_test(NAME test_config COMMAND test_config)
```

- [ ] **Step 6: Build and test**

```bash
cmake --build build
ctest --test-dir build -R test_config -V
```

Expected: All tests pass

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: implement ConfigManager with validation and locking"
```

---

**Chunk 3 Complete!** At this point we have:
- ConfigManager with simplified TOML parsing
- Profile validation before adding/loading
- File locking with 5-second timeout
- Schema version field for future migrations
- Permission validation with warnings
- Comprehensive tests including edge cases

**Note:** For production, replace the simplified TOML parser with toml++ library for full spec compliance.

---

## Remaining Work Summary

The following chunks are documented at a high level. Implement them following the same pattern as Chunks 1-3.

### Chunk 4: CLI Parser and Commands

**Files:**
- `include/diffsync/cli/parser.hpp`
- `src/cli/parser.cpp`
- `src/cli/commands.cpp`

**Tasks:**
1. Set up CLI11 parser with subcommands (status, sync, config, tui)
2. Implement `status` command - lists changed files
3. Implement `sync` command - calls rsync with selected files
4. Implement `config` subcommands (add, list, remove, use)
5. Handle environment variables (DIFFSYNC_PROFILE, etc.)
6. Add --dry-run, --batch, --verbose flags

### Chunk 5: RsyncExecutor

**Files:**
- `include/diffsync/core/rsync.hpp`
- `src/core/rsync.cpp`

**Tasks:**
1. Build rsync command with proper SSH options
2. Implement progress parsing with regex for `--info=progress2`
3. Handle dry-run mode
4. Add retry logic for connection failures
5. Return detailed SyncResult

### Chunk 6: TUI with FTXUI

**Files:**
- `include/diffsync/tui/main_screen.hpp`
- `src/tui/main_screen.cpp`
- `include/diffsync/tui/file_list.hpp`
- `src/tui/file_list.cpp`
- `include/diffsync/tui/progress.hpp`
- `src/tui/progress.cpp`

**Tasks:**
1. Create main screen with header, file list, and footer
2. Implement file list component with virtual scrolling
3. Add keyboard handling (Space, Enter, Esc, F1, arrows)
4. Implement progress screen during sync
5. Add error overlay for failures
6. Profile switcher (F1 key)

### Chunk 7: Integration and Polish

**Tasks:**
1. Wire up all components in main.cpp
2. Add man page or --help documentation
3. Integration tests for full workflows
4. Performance testing (10k files scan)
5. Package with CMake install rules

---

## Implementation Notes

- Use TDD: Write test, see it fail, implement, see it pass, commit
- Commit frequently after each working step
- Keep files focused - split if any file grows beyond ~300 lines
- Follow existing patterns in each chunk

---

**Plan Complete!**

This implementation plan provides a complete roadmap for building diffsync. Chunks 1-3 are detailed and ready for implementation. Chunks 4-7 follow the same pattern and can be expanded following the established template.


