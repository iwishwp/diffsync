#include <cassert>
#include <iostream>
#include <stdexcept>
#include "diffsync/types.hpp"

using namespace diffsync;

void testFileStatus() {
    assert(toString(FileStatus::Modified) == "M");
    assert(toString(FileStatus::Staged) == "M ");
    assert(toString(FileStatus::Added) == "A");
    assert(toString(FileStatus::Untracked) == "??");
    assert(toString(FileStatus::Deleted) == "D");

    assert(parseFileStatus("M") == FileStatus::Modified);
    assert(parseFileStatus("M ") == FileStatus::Staged);
    assert(parseFileStatus("A") == FileStatus::Added);
    assert(parseFileStatus("??") == FileStatus::Untracked);
    assert(parseFileStatus("D") == FileStatus::Deleted);

    // Test invalid input throws exception
    bool exceptionThrown = false;
    try {
        parseFileStatus("invalid");
    } catch (const std::invalid_argument&) {
        exceptionThrown = true;
    }
    assert(exceptionThrown);

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

    // Test toString()
    assert(config.toString() == "user@localhost:22/path");

    // Test invalid port
    config.port = 0;
    assert(!config.isValid());
    config.port = 65536;  // This will overflow in uint16_t
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

void testSyncResult() {
    SyncResult result;
    assert(!result.hasErrors());  // No errors initially

    result.totalFiles = 10;
    result.successCount = 8;
    result.failedCount = 1;
    result.skippedCount = 1;

    assert(result.totalFiles == 10);
    assert(result.successCount == 8);
    assert(result.failedCount == 1);
    assert(result.skippedCount == 1);

    // Test hasErrors()
    assert(!result.hasErrors());
    result.errors.push_back("Error 1");
    assert(result.hasErrors());

    std::cout << "SyncResult tests passed\n";
}

int main() {
    testFileStatus();
    testRemoteConfig();
    testChangeFile();
    testSyncResult();
    std::cout << "\nAll tests passed!\n";
    return 0;
}
