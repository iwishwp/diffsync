# diffsync

[English](#english) | [简体中文](#简体中文)

---

## 简体中文

一个基于 `git diff` 的 C++ 命令行工具，用于将本地代码更改同步到远程构建机器。支持 CLI 和 TUI 两种模式。

### 功能特性

- **智能变更检测**：自动检测未暂存、已暂存和未跟踪的文件
- **TUI 交互模式**：友好的终端界面，支持文件选择、进度显示和传输日志
- **配置管理**：支持多个远程配置文件，方便切换不同环境
- **传输日志**：详细记录每个文件的传输状态（成功/失败/跳过）
- **失败重试**：支持单个或批量重试失败的文件

### 安装

```bash
# 克隆仓库
git clone https://github.com/iwishwp/diffsync.git
cd diffsync

# 构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 可选：安装到系统
sudo make install
```

**依赖要求：**
- C++20 编译器 (GCC 11+, Clang 13+)
- CMake >= 3.16
- git >= 2.30
- rsync >= 3.1
- ssh

### 快速开始

```bash
# 启动 TUI 模式（推荐）
diffsync

# 或直接同步所有更改
diffsync sync

# 查看更改状态
diffsync status
```

### TUI 模式使用

```
┌──────────────────────────────────────────────────────────────────────┐
│  diffsync - Config: dev (user@192.168.1.100:~/build)                │
├──────────────────────────────────────────────────────────────────────┤
│  [?] Help  [F2] Select All  [C] Config  [Enter] Sync                │
├──────────────────────────────────────────────────────────────────────┤
│  [✓] src/main.cpp           12.4 KB  Modified                      │
│  [ ] include/api.h          2.1 KB   Added                          │
│  [ ] tests/test.cpp         8.7 KB   Modified                       │
│                                                                      │
│  Selected: 1/3  Total: ~23.2 KB                                      │
├──────────────────────────────────────────────────────────────────────┤
│  [Enter] Sync  [L] Log  [Esc] Quit                                  │
└──────────────────────────────────────────────────────────────────────┘
```

**快捷键：**
- `Space` / `Enter` - 切换选择 / 开始同步
- `F2` - 全选/取消全选
- `C` - 打开配置管理
- `L` - 查看传输日志
- `↑↓` - 导航
- `Esc` - 退出

### 配置文件

配置文件位于 `~/.config/diffsync/config.toml`：

```toml
# 当前活动配置
current_profile = "dev"

[[profiles]]
name = "dev"
host = "192.168.1.100"
user = "builder"
remote_path = "~/build"
port = 22
identity_file = "~/.ssh/id_rsa"

[[profiles]]
name = "production"
host = "build.example.com"
user = "deploy"
remote_path = "/opt/build/src"
port = 2222
identity_file = "~/.ssh/deploy_key"
```

### CLI 命令

```bash
# 查看更改的文件
diffsync status

# 同步所有更改
diffsync sync

# 同步指定文件
diffsync sync src/main.cpp include/hello.h

# 使用指定配置同步
diffsync sync --profile production

# 预览同步（不实际传输）
diffsync sync --dry-run

# 配置管理
diffsync config list           # 列出所有配置
diffsync config use <name>     # 切换默认配置
```

### 传输日志

同步完成后，按 `L` 查看详细传输日志：

```
┌─────────────────────────────────────────────┐
│           Transfer Log                      │
├─────────────────────────────────────────────┤
│  ✓ src/main.cpp              Success       │
│  ✓ include/api.h              Success       │
│  ✗ tests/test.cpp             Failed        │
│     → Permission denied                     │
│                                             │
│  [Files: 3]  [Success: 2]  [Failed: 1]     │
│                                             │
│  [R] Retry Selected  [A] Retry All Failed   │
│  [Esc] Close                                │
└─────────────────────────────────────────────┘
```

### 路线图

- [ ] 支持双向同步
- [ ] 支持冲突解决
- [ ] 支持多线程传输
- [ ] 支持带宽限制

### 许可证

MIT License

---

## English

A C++ command-line tool that synchronizes local code changes to a remote build machine based on `git diff`. Supports both CLI and TUI modes.

### Features

- **Smart Change Detection**: Automatically detects unstaged, staged, and untracked files
- **TUI Interactive Mode**: Friendly terminal interface with file selection, progress display, and transfer log
- **Configuration Management**: Support multiple remote profiles for easy environment switching
- **Transfer Logging**: Detailed logging of each file's transfer status (success/failed/skipped)
- **Failure Retry**: Support single or batch retry of failed files

### Installation

```bash
# Clone repository
git clone https://github.com/iwishwp/diffsync.git
cd diffsync

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Optional: install to system
sudo make install
```

**Requirements:**
- C++20 compiler (GCC 11+, Clang 13+)
- CMake >= 3.16
- git >= 2.30
- rsync >= 3.1
- ssh

### Quick Start

```bash
# Start TUI mode (recommended)
diffsync

# Or sync all changes directly
diffsync sync

# View change status
diffsync status
```

### TUI Mode

```
┌──────────────────────────────────────────────────────────────────────┐
│  diffsync - Config: dev (user@192.168.1.100:~/build)                │
├──────────────────────────────────────────────────────────────────────┤
│  [?] Help  [F2] Select All  [C] Config  [Enter] Sync                │
├──────────────────────────────────────────────────────────────────────┤
│  [✓] src/main.cpp           12.4 KB  Modified                      │
│  [ ] include/api.h          2.1 KB   Added                          │
│  [ ] tests/test.cpp         8.7 KB   Modified                       │
│                                                                      │
│  Selected: 1/3  Total: ~23.2 KB                                      │
├──────────────────────────────────────────────────────────────────────┤
│  [Enter] Sync  [L] Log  [Esc] Quit                                  │
└──────────────────────────────────────────────────────────────────────┘
```

**Keyboard Shortcuts:**
- `Space` / `Enter` - Toggle selection / Start sync
- `F2` - Select all / Unselect all
- `C` - Open configuration management
- `L` - View transfer log
- `↑↓` - Navigate
- `Esc` - Quit

### Configuration File

Configuration is stored at `~/.config/diffsync/config.toml`:

```toml
# Current active profile
current_profile = "dev"

[[profiles]]
name = "dev"
host = "192.168.1.100"
user = "builder"
remote_path = "~/build"
port = 22
identity_file = "~/.ssh/id_rsa"

[[profiles]]
name = "production"
host = "build.example.com"
user = "deploy"
remote_path = "/opt/build/src"
port = 2222
identity_file = "~/.ssh/deploy_key"
```

### CLI Commands

```bash
# View changed files
diffsync status

# Sync all changes
diffsync sync

# Sync specific files
diffsync sync src/main.cpp include/hello.h

# Sync with specific profile
diffsync sync --profile production

# Preview sync (dry run)
diffsync sync --dry-run

# Configuration management
diffsync config list           # List all profiles
diffsync config use <name>     # Switch default profile
```

### Transfer Log

After sync completes, press `L` to view detailed transfer log:

```
┌─────────────────────────────────────────────┐
│           Transfer Log                      │
├─────────────────────────────────────────────┤
│  ✓ src/main.cpp              Success       │
│  ✓ include/api.h              Success       │
│  ✗ tests/test.cpp             Failed        │
│     → Permission denied                     │
│                                             │
│  [Files: 3]  [Success: 2]  [Failed: 1]     │
│                                             │
│  [R] Retry Selected  [A] Retry All Failed   │
│  [Esc] Close                                │
└─────────────────────────────────────────────┘
```

### Roadmap

- [ ] Bidirectional sync support
- [ ] Conflict resolution
- [ ] Multi-threaded transfer
- [ ] Bandwidth throttling

### License

MIT License
