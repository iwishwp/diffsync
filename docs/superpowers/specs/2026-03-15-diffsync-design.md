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

Scans git repository for changes by calling `git` commands:
- Unstaged modifications (`git diff HEAD --name-status`)
- Staged modifications (`git diff --cached --name-status`)
- Untracked files (`git ls-files --others --exclude-standard`)

**Decision: Shell commands over libgit2**
- Simpler implementation, leverages git's proven logic
- Easier to handle edge cases (submodules, worktrees)
- Performance acceptable for typical codebases (<10k files)
- Error handling via exit codes and stderr parsing

**Git command execution:**
```bash
# Combined change detection
git diff HEAD --name-status
git diff --cached --name-status
git ls-files --others --exclude-standard
```

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
    Untracked,   // ?? - Untracked
    Deleted      // D - Deleted (for reference, not synced)
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

// Error codes
enum class ErrorCode {
    NotAGitRepo,
    NoChanges,
    SshConnectionFailed,
    RsyncNotAvailable,
    RemotePathNotFound,
    ConfigCorrupted,
    PathValidationError
};
```

**File Status Notes:**
- Deleted files are detected but NOT synced (remote manages its own deletions)
- Renamed files detected via `git diff -M` appear as deleted+added pairs
- Permission-only changes are NOT detected (use `git diff` for content changes)
- No conflict resolution - remote is treated as authoritative

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

### TUI Edge Cases

| Scenario | Behavior |
|----------|----------|
| 100+ files | Virtual scrolling, show visible window only |
| Long filenames | Truncate with ellipsis in middle, full path in tooltip |
| Connection failure during sync | Error overlay, show error message, allow retry or cancel |
| Partial sync failure | Show summary with failed files highlighted in red |
| Concurrent sync | Disable sync button while syncing, show "Syncing..." |
| Empty file list | Show "No changes detected" message with help text |
| No config on first run | Auto-create config directory, prompt for initial profile |

### TUI Error States

```
┌──────────────────────────────────────────────────────────────────────┐
│  ❌ Sync Failed                                                       │
├──────────────────────────────────────────────────────────────────────┤
│  2 of 5 files failed to sync:                                        │
│                                                                      │
│  ✗ src/large_file.cpp   - Connection timed out                      │
│  ✗ tests/mock.cpp       - Permission denied                          │
│                                                                      │
│  [R] Retry All  [S] Skip Failed  [Esc] Return                        │
└──────────────────────────────────────────────────────────────────────┘
```

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

### Progress Parsing Details

**rsync output format (from `--info=progress2`):**
```
    12,345  25%   10.23MB/s    0:00:03  (xfr#4, to-chk=8/20)
```

**Parser regex pattern:**
```cpp
R"(\s*(\d+)\s+(\d+)%\s+([\d.]+\w+/s)\s+([\d:]+)\s+\(xfr#(\d+),\s+to-chk=(\d+)/(\d+)\))"
```

**Extracted fields:**
1. Bytes transferred (`\d+`)
2. Percentage (`\d+%`)
3. Speed (`[\d.]+\w+/s`)
4. ETA (`[\d:]+`)
5. Files transferred (`xfr#`)
6. Files remaining (`to-chk`)

**Fallback handling:**
- If `--info=progress2` not available, fall back to parsing individual file progress
- Parse locale-agnostic numbers (handle `,` vs `.` for thousands)
- Validate rsync version >= 3.1.0 on startup

**Progress update frequency:**
- Poll rsync stdout every 100ms
- Update TUI at most 30 FPS to avoid rendering overhead

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

### Detailed Error Handling

| Error Code | Action | Retry Policy |
|------------|--------|--------------|
| `NotAGitRepo` | Exit with error message | No retry |
| `NoChanges` | Exit with info message | No retry |
| `SshConnectionFailed` | Show error, prompt user | Allow 3 retries with 5s timeout each |
| `RsyncNotAvailable` | Prompt to install rsync | No retry |
| `RemotePathNotFound` | Ask to create: "Create remote path? [y/N]" | Single prompt |
| `ConfigCorrupted` | Backup and recreate default | Automatic, notify user |
| `PathValidationError` | Reject malicious paths immediately | No retry |

**UI Flow for connection failure (TUI):**
```
┌──────────────────────────────────────────────────────────────────────┐
│  ⚠ Connection Failed                                                  │
├──────────────────────────────────────────────────────────────────────┤
│  Could not connect to user@192.168.1.100:22                          │
│  Please check:                                                       │
│  - Host is reachable                                                 │
│  - SSH key is configured correctly                                   │
│  - User has permissions on remote host                               │
│                                                                      │
│  [R] Retry (2/3)  [C] Check Config  [Esc] Cancel                    │
└──────────────────────────────────────────────────────────────────────┘
```

## Configuration Management

### Config File Operations

| Operation | Behavior |
|-----------|----------|
| First run (no config) | Create `~/.config/diffsync/` with default config template |
| `config add` | Validate host/port, prompt for missing fields, append to profiles |
| `config remove` | Remove profile, error if removing active profile |
| `config use` | Set `current_profile`, validate profile exists |
| Write conflicts | Lock file with `flock()`, wait up to 5s for lock |
| Config validation | Check required fields (host, user, remote_path), validate port range 1-65535 |

### Config Schema Migration

When config schema changes:
1. Check `schema_version` field (defaults to 1 if missing)
2. Apply migrations sequentially to reach current version
3. Backup old config before migration
4. On migration failure, restore from backup

## CLI / TUI Boundary

| Command | Mode | Behavior |
|---------|------|----------|
| `diffsync` (no args) | TUI | Interactive mode |
| `diffsync tui` | TUI | Explicit TUI mode |
| `diffsync status` | CLI | Print list, exit |
| `diffsync sync` | CLI | Sync all, exit |
| `diffsync sync <files>` | CLI | Sync specific files, exit |
| `diffsync sync --interactive` | TUI | Start TUI with files pre-selected |

**Environment variables:**
```bash
DIFFSYNC_PROFILE=dev        # Override default profile
DIFFSYNC_CONFIG=/path/to/config.toml  # Custom config path
DIFFSYNC_SSH_KEY=/path/to/key # Override identity file
```

**Non-interactive mode (`--batch`):**
- Never prompt for input
- Exit with error code on any failure
- Suitable for CI/CD scripts

## Security Considerations

| Threat | Mitigation |
|--------|------------|
| SSH key passphrase | Use ssh-agent, never prompt in TUI |
| Credential storage | Config file uses `~/.ssh/` paths, not embedded secrets |
| Path traversal | Validate paths are within repo root, reject `../` |
| Command injection | Use `popen()` with properly escaped arguments, never concatenate |
| rsync remote command | Disable rsync remote command execution |
| Config file permissions | Set `0600` on config file, warn if permissions too open |

**Path validation:**
```cpp
bool isValidPath(const std::string& path) {
    // Reject absolute paths
    if (path.starts_with("/")) return false;
    // Reject parent directory traversal
    if (path.contains("..")) return false;
    // Reject shell metacharacters
    if (path.contains_any_of(";|$&`")) return false;
    return true;
}
```

## Testing Strategy

### Unit Tests
- Mock external dependencies (git, rsync, ssh)
- Test core logic in isolation
- Coverage target: 80%

### Integration Tests
- Use temporary git repos
- Mock SSH server (openssh dummy mode)
- Test with actual rsync (local-to-local)

### Test Structure
```
tests/
├── unit/
│   ├── test_git_scanner.cpp
│   ├── test_config.cpp
│   └── test_rsync_parser.cpp
├── integration/
│   ├── test_sync_workflow.cpp
│   └── test_tui_interaction.cpp
└── fixtures/
    ├── mini_repo/      # Pre-configured test repo
    └── configs/        # Sample config files
```

### Performance Criteria
- Scan 10k files in < 2 seconds
- TUI render at 30 FPS minimum
- Memory usage < 100 MB during sync

## Technology Stack

| Dependency | Version | Purpose | Link |
|------------|---------|---------|------|
| **FTXUI** | >= 5.0.0 | TUI framework | https://github.com/ArthurSonzogni/FTXUI |
| **toml++** | >= 3.4.0 | Config file parsing | https://github.com/marzer/tomlplusplus |
| **CLI11** | >= 2.4.0 | CLI argument parsing | https://github.com/CLIUtils/CLI11 |
| **spdlog** (optional) | >= 1.12 | Logging | https://github.com/gabime/spdlog |
| **git** (external) | >= 2.30.0 | Git operations | External command |
| **rsync** (external) | >= 3.1.0 | File transfer | External command |
| **ssh** (external) | OpenSSH >= 8.0 | Remote connection | External command |

### Build Requirements

| Requirement | Specification |
|-------------|---------------|
| C++ Standard | C++20 |
| Compiler | GCC 11+, Clang 13+, MSVC 2022+ |
| CMake | >= 3.20 |
| Platform Support | Linux, macOS (primary); Windows (best effort) |
| Linking | Prefer static linking for dependencies |

### Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux | ✅ Primary | Fully supported |
| macOS | ✅ Primary | Fully supported |
| Windows | ⚠️ Best effort | Requires WSL or Git Bash for git/rsync |

**Windows considerations:**
- Recommend WSL2 for native experience
- Git Bash may work with path translation
- No native Windows support in Phase 1

## Non-Goals

Features explicitly out of scope for Phase 1:

1. **Bidirectional sync** - Remote changes are not pulled back
2. **Conflict resolution** - Remote is authoritative, no merge logic
3. **Automatic retry** - User-initiated retry only
4. **Background sync daemon** - Manual sync only
5. **GUI application** - Terminal UI only
6. **Multi-threaded transfer** - Single rsync process
7. **Compression options** - Use rsync defaults (-z)
8. **Bandwidth throttling** - Not needed for typical use

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
