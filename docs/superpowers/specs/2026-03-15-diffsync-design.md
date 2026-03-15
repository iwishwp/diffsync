# diffsync Design Specification

**Date:** 2026-03-15
**Status:** Draft
**Author:** Claude & User

## Overview

`diffsync` is a C++ command-line tool that synchronizes local code changes to a remote build machine based on `git diff`. It supports both CLI and TUI modes for flexible workflows.

### Problem Statement

When developing code locally that needs to be synchronized to a high-performance remote build machine, modified files are scattered across different paths. Manually uploading via SFTP/SCP requires frequent directory switching, which is tedious and error-prone.

### Solution

Detect all changed files (unstaged, staged, and untracked) via `git diff`, display them for user selection, and sync selected files to remote via `rsync` over SSH.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         diffsync                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐ │
│  │   CLI 模式   │    │   TUI 模式   │    │      核心引擎        │ │
│  │  (Parser)   │    │  (FTXUI)    │    │  (Core Engine)       │ │
│  └──────┬──────┘    └──────┬──────┘    └──────────┬──────────┘ │
│         │                  │                      │              │
│         └──────────────────┴──────────────────────┘              │
│                            │                                      │
│         ┌──────────────────┴──────────────────┐                  │
│         ▼                                      ▼                  │
│  ┌───────────────┐                   ┌──────────────┐           │
│  │ GitDiffScanner│                   │ ConfigManager │           │
│  │ (变更检测)     │                   │ (配置管理)     │           │
│  └───────┬───────┘                   └──────┬───────┘           │
│          │                                   │                    │
│          └───────────────┬───────────────────┘                   │
│                          ▼                                        │
│                 ┌────────────────┐                                │
│                 │  RsyncExecutor │                                │
│                 │  (传输执行)     │                                │
│                 └────────────────┘                                │
└─────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. GitDiffScanner

Scans git repository for changes:
- Unstaged modifications (`git diff HEAD`)
- Staged modifications (`git diff --cached`)
- Untracked files (`git ls-files --others --exclude-standard`)

### 2. ConfigManager

Manages remote connection profiles stored in `~/.config/diffsync/config.toml`.

### 3. RsyncExecutor

Executes rsync commands and parses progress output from `--info=progress2`.

### 4. CLI Parser

Handles command-line argument parsing and dispatches commands.

### 5. TUI (FTXUI)

Interactive terminal UI for file selection and progress monitoring.

## Data Structures

```cpp
enum class FileStatus {
    Modified,    // M - Modified (unstaged)
    Staged,      // Modified after staging
    Added,       // A - New file (staged)
    Untracked    // ?? - Untracked
};

struct ChangeFile {
    std::string path;           // Path relative to repo root
    FileStatus status;
    size_t size;                // File size in bytes
    bool selected;              // TUI selection state
};

struct RemoteConfig {
    std::string name;           // Profile name
    std::string host;           // Host address
    std::string user;           // Username
    std::string remotePath;     // Remote root directory
    std::string identityFile;   // SSH private key path (optional)
    uint16_t port = 22;         // SSH port
};

struct SyncResult {
    int totalFiles;
    int successCount;
    int failedCount;
    int skippedCount;
    std::vector<std::string> errors;
};
```

## Command-Line Interface

```bash
# View changed files
diffsync status [--short]

# Sync files
diffsync sync [options] [--] [<files>...]

# TUI interactive mode (default)
diffsync [tui]

# Configuration management
diffsync config
diffsync config add <name>
diffsync config list
diffsync config remove <name>
diffsync config use <name>

# Common options
--profile <name>      # Use specific profile
--dry-run            # Preview mode, no actual transfer
--remote <host:path>  # Temporarily specify remote
--exclude <pattern>   # Exclude patterns
--include <pattern>   # Include patterns
-v, --verbose         # Verbose output
```

### Usage Examples

```bash
# TUI mode - interactively select files and sync
diffsync

# CLI - sync all changes
diffsync sync

# Sync specific files
diffsync sync src/main.cpp include/hello.h

# Sync with specific profile
diffsync sync --profile production

# Preview sync without transferring
diffsync sync --dry-run
```

## TUI Design

```
┌──────────────────────────────────────────────────────────────────────┐
│  diffsync - Config: dev-build (user@192.168.1.100:~/build)          │
├──────────────────────────────────────────────────────────────────────┤
│  [F1] Configs  [F2] Select All  [F3] Unselect All  [Enter] Sync      │
├──────────────────────────────────────────────────────────────────────┤
│  ☐ M  src/core/engine.cpp           12.4 KB  Modified              │
│  ☒ A  include/api/hello.h           2.1 KB   Added (staged)        │
│  ☐ M  tests/integration_test.cpp    8.7 KB   Modified              │
│  ☐ ?? new_module/reader.cpp         5.3 KB   Untracked             │
│  ☐ ?? docs/architecture.md          3.8 KB   Untracked             │
│  ☐ M  CMakeLists.txt                1.2 KB   Modified              │
│                                                                      │
│  Selected: 1/6  Total: ~33.5 KB                                      │
├──────────────────────────────────────────────────────────────────────┤
│  [↓] Sync to Remote                    [Esc] Quit                   │
└──────────────────────────────────────────────────────────────────────┘
```

**Keyboard Shortcuts:**
- `Space` - Toggle selection
- `Enter` - Start sync
- `Esc` - Quit
- `F1` - Switch profile
- `↑↓` - Navigate

## Configuration File

Located at `~/.config/diffsync/config.toml`:

```toml
# Current active profile
current_profile = "dev-build"

# Default exclude patterns
default_excludes = [
    ".git/*",
    "*.o",
    "*.a",
    "build/*",
    "node_modules/*"
]

[[profiles]]
name = "dev-build"
host = "192.168.1.100"
user = "builder"
remote_path = "~/projects/myapp"
port = 22
identity_file = "~/.ssh/id_rsa"

[[profiles]]
name = "production"
host = "build.company.com"
user = "deploy"
remote_path = "/opt/build/src"
port = 2222
identity_file = "~/.ssh/deploy_key"
```

## rsync Integration

### Command Generation

```cpp
rsync -avz --info=progress2 \
    --include="src/main.cpp" \
    --include="include/hello.h" \
    --exclude="*" \
    -e "ssh -p 22 -i ~/.ssh/id_rsa" \
    /local/repo/ \
    user@host:~/remote/repo/
```

### Progress Parsing

rsync `--info=progress2` output format:
```
    12,345  25%   10.23MB/s    0:00:03  (xfr#4, to-chk=8/20)
```

Extract: bytes transferred, percentage, speed, ETA

### Dry-Run Mode

Add `--dry-run` to preview operations without actual transfer.

## Error Handling

| Scenario | Handling |
|----------|----------|
| Not a git repository | Error message and exit |
| No changed files | Info message and exit |
| SSH connection failed | Prompt to check config, allow retry |
| rsync not installed | Prompt to install rsync |
| Remote path doesn't exist | Ask to create via `ssh mkdir -p` |
| Partial file failures | Show failed list, continue with others |
| Config file corrupted | Use defaults, notify user |

## Technology Stack

| Dependency | Purpose |
|------------|---------|
| **FTXUI** | TUI framework |
| **libgit2** | Git operations (or call git commands) |
| **toml++** | Config file parsing |
| **CLI11** / **cxxopts** | CLI argument parsing |
| **std::filesystem** | Path operations |
| **std::thread** | rsync process monitoring |

## Project Structure

```
diffsync/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # Entry point
│   ├── core/
│   │   ├── git_scanner.cpp   # Git change scanning
│   │   ├── config.cpp        # Configuration management
│   │   └── rsync.cpp         # rsync invocation
│   ├── tui/
│   │   ├── main_screen.cpp   # TUI main screen
│   │   ├── file_list.cpp     # File list component
│   │   └── progress.cpp      # Progress display
│   └── cli/
│       └── commands.cpp      # CLI command handling
├── include/
│   └── diffsync/
│       ├── core/
│       └── tui/
└── tests/
```

## Success Criteria

1. Correctly detect all git changes (unstaged, staged, untracked)
2. TUI allows selective file sync with clear visual feedback
3. CLI supports quick sync workflows
4. Multiple remote profiles are manageable
5. Progress is accurately displayed during transfer
6. Errors are handled gracefully with helpful messages
