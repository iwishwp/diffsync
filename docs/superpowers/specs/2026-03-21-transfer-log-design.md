# Transfer Log Feature Design

## Overview
Add a transfer log feature to display file-by-file transfer results and allow retrying failed files.

## Requirements
- View detailed transfer log for current session
- Show filename and status (Success/Failed/Skipped)
- Retry individual or all failed files
- Log data cleared on program exit (no persistence)
- Press `L` to open log window
- Prompt "Press [L] to view transfer log" when failures exist

## Architecture

### Components
```
MainScreen
  ├── FileList
  ├── SyncModal (progress/completion)
  ├── ConfigModal
  ├── HelpModal
  └── LogModal (NEW) - Transfer log viewer
```

### Data Structures
```cpp
enum class FileStatus { Success, Failed, Skipped };

struct FileTransferLog {
    std::string filename;
    FileStatus status;
    std::string errorMessage;  // For failed files
};

class TransferLog {
    std::vector<FileTransferLog> entries_;

public:
    void logFile(const std::string& name, FileStatus status,
                 const std::string& error = "");
    bool hasFailures() const;
    std::vector<FileTransferLog> getFailedFiles() const;
    void clear();
    const std::vector<FileTransferLog>& getEntries() const;
};
```

## UI Design

### Log Modal Window
```
┌─────────────────────────────────────────────┐
│           Transfer Log                      │
├─────────────────────────────────────────────┤
│                                             │
│  ✓ src/main.cpp              Success       │
│  ✓ src/tui/main_screen.cpp     Success     │
│  ✗ src/core/module.cpp         Failed      │
│     → SSH connection timeout               │
│  ○ docs/README.md             Skipped      │
│                                             │
│  [Files: 5]  [Success: 3]  [Failed: 1]     │
│                                             │
├─────────────────────────────────────────────┤
│  [↑/↓] Select  [R] Retry Selected          │
│  [A] Retry All Failed  [Esc] Close          │
└─────────────────────────────────────────────┘
```

### Status Colors
- ✓ Success = Green
- ✗ Failed = Red
- ○ Skipped = Yellow/Gray

### Sync Completion (With Failures)
```
┌─────────────────────────────────────────┐
│         Sync Complete                    │
├─────────────────────────────────────────┤
│  Total: 5 files                          │
│  Success: 3                              │
│  Failed: 1  (red highlight)              │
│                                          │
│  Press [L] to view transfer log          │
│  Press [Esc] to close                    │
└─────────────────────────────────────────┘
```

## Interaction Flow

### Opening Log
- Press `L` from main screen or sync completion screen
- Only enabled when log has entries

### Log Window Actions
| Key | Action |
|-----|--------|
| ↑/↓ | Select file |
| R | Retry selected failed file |
| A | Retry all failed files |
| Esc | Close log |

### Retry Flow
1. Select failed file(s) and press R (or A for all)
2. Log window closes
3. Execute rsync for selected files only
4. Update log entries with new results
5. Reopen log window automatically

## Data Flow

### During Transfer
1. User selects files and presses Enter
2. `startSync()` clears old log, creates new session
3. rsync executes with callback for each file:
   - Success → `logFile(name, FileStatus::Success)`
   - Failed → `logFile(name, FileStatus::Failed, error)`
   - Skipped → `logFile(name, FileStatus::Skipped)`
4. Check `hasFailures()` after completion
5. If failures exist, show "Press [L]" prompt

### During Retry
1. Get list of failed files from log
2. Execute rsync for those files only
3. For each result:
   - Success → Update entry to Success
   - Failed → Keep Failed, update error message
4. Reopen log window

## File Changes

### New Files
- `include/diffsync/tui/log_modal.hpp` - LogModal class
- `src/tui/log_modal.cpp` - Implementation

### Modified Files
- `include/diffsync/tui/main_screen.hpp` - Add log members
- `src/tui/main_screen.cpp` - Integrate LogModal, handle L key
- `include/diffsync/core/rsync.hpp` - Return file-level results
- `src/core/rsync.cpp` - Track individual file results

## Implementation Notes

### Modal Nesting
The modal stack will be:
```cpp
auto withLogModal = Modal(withHelpModal, logModal, &logModalShown_);
```

### Key Handling
Add `L` key handler in `MainScreen::buildShortcuts()`:
```cpp
if (event == Event::Character('l') || event == Event::Character('L')) {
    if (!transferLog_.getEntries().empty()) {
        logModalShown_ = true;
    }
    return true;
}
```

### SyncResult Enhancement
Need to add file-level results to `SyncResult`:
```cpp
struct SyncResult {
    int totalFiles = 0;
    int successCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    std::vector<std::string> errors;

    // NEW: Detailed file results
    std::vector<FileTransferLog> fileResults;
};
```

## Error Handling

| Situation | Behavior |
|-----------|----------|
| No log entries | Show "No transfer records" |
| Retry with no failures | Prompt "No files to retry" |
| Retry still fails | Keep failed status, allow retry again |
| Transfer interrupted | Preserve logged entries |

## Testing
1. Transfer files with mix of success/fail/skip
2. Verify log shows correct status and colors
3. Retry individual failed file → verify success
4. Retry all failed files → verify all updated
5. Check "Press [L]" prompt appears with failures
6. Verify log cleared on new transfer
7. Verify log doesn't persist across program restarts
