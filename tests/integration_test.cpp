// Integration test for TUI components (without screen)
#include "diffsync/tui/file_list.hpp"
#include "diffsync/types.hpp"
#include <cassert>
#include <iostream>

using namespace diffsync;

void testFileList() {
    std::cout << "Testing FileList component...\n";

    // Create test files
    std::vector<ChangeFile> files = {
        {"src/main.cpp", FileStatus::Modified, 1024, false},
        {"include/header.hpp", FileStatus::Added, 512, false},
        {"docs/readme.md", FileStatus::Untracked, 2048, false},
        {"src/util.cpp", FileStatus::Modified, 768, false}
    };

    FileList fileList(files);

    // Test initial state
    assert(fileList.getFiles().size() == 4);
    assert(fileList.getSelectedCount() == 0);
    assert(fileList.getSelectedSize() == 0);

    // Test selection
    fileList.selectAll();
    assert(fileList.getSelectedCount() == 4);
    assert(fileList.getSelectedSize() == 1024 + 512 + 2048 + 768);

    // Test unselect
    fileList.unselectAll();
    assert(fileList.getSelectedCount() == 0);
    assert(fileList.getSelectedSize() == 0);

    // Test toggle
    fileList.setFocusedIndex(0);
    fileList.toggleSelection();
    assert(fileList.getSelectedCount() == 1);
    assert(fileList.getSelectedSize() == 1024);

    fileList.toggleSelection();
    assert(fileList.getSelectedCount() == 0);

    // Test get selected files
    files[0].selected = true;
    files[2].selected = true;
    fileList.updateFiles(files);

    auto selected = fileList.getSelectedFiles();
    assert(selected.size() == 2);
    assert(selected[0].path == "src/main.cpp");
    assert(selected[1].path == "docs/readme.md");

    // Test get selected indices
    auto indices = fileList.getSelectedIndices();
    assert(indices.size() == 2);
    assert(indices[0] == 0);
    assert(indices[1] == 2);

    std::cout << "  FileList component tests passed!\n";
}

void testFileListWithChanges() {
    std::cout << "Testing FileList with file changes...\n";

    std::vector<ChangeFile> files = {
        {"src/engine.cpp", FileStatus::Modified, 12400, false},
        {"include/api.h", FileStatus::Added, 2100, false},
        {"test/test.cpp", FileStatus::Modified, 8700, false},
        {"new/reader.cpp", FileStatus::Untracked, 5300, false}
    };

    FileList fileList(files);

    // Simulate user selection
    fileList.setFocusedIndex(0);
    fileList.toggleSelection();
    assert(fileList.getSelectedCount() == 1);

    fileList.setFocusedIndex(1);
    fileList.toggleSelection();
    assert(fileList.getSelectedCount() == 2);

    // Update files and verify selection is preserved
    files[2].selected = true;
    fileList.updateFiles(files);
    assert(fileList.getSelectedCount() == 3);

    std::cout << "  FileList change tests passed!\n";
}

int main() {
    std::cout << "Running TUI integration tests...\n\n";

    testFileList();
    testFileListWithChanges();

    std::cout << "\nAll integration tests passed!\n";
    return 0;
}
