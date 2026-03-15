// Simple unit tests for TUI logic
#include "diffsync/types.hpp"
#include <cassert>
#include <iostream>

using namespace diffsync;

void testTypes() {
    std::cout << "Testing types...\n";

    // Test FileStatus
    assert(toString(FileStatus::Modified) == "M");
    assert(toString(FileStatus::Added) == "A");
    assert(toString(FileStatus::Untracked) == "??");

    // Test ChangeFile
    ChangeFile file{"test.cpp", FileStatus::Modified, 1024, true};
    assert(file.statusString() == "M");
    assert(file.size == 1024);
    assert(file.selected == true);

    // Test RemoteConfig
    RemoteConfig config{"test", "example.com", "user", "/path"};
    assert(config.isValid() == true);
    assert(config.toString().find("example.com") != std::string::npos);

    // Test SyncResult
    SyncResult result;
    assert(result.totalFiles == 0);
    assert(result.hasErrors() == false);

    std::cout << "  All types tests passed!\n";
}

void testFileOperations() {
    std::cout << "Testing file operations...\n";

    std::vector<ChangeFile> files = {
        {"src/main.cpp", FileStatus::Modified, 1024, true},
        {"include/header.hpp", FileStatus::Added, 512, false},
        {"docs/readme.md", FileStatus::Untracked, 2048, false}
    };

    // Count selected
    int selectedCount = 0;
    uint64_t selectedSize = 0;
    for (const auto& file : files) {
        if (file.selected) {
            selectedCount++;
            selectedSize += file.size;
        }
    }

    assert(selectedCount == 1);
    assert(selectedSize == 1024);

    std::cout << "  All file operations tests passed!\n";
}

int main() {
    std::cout << "Running TUI unit tests...\n\n";

    testTypes();
    testFileOperations();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
