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

int main() {
    testFileStatus();
    testRemoteConfig();
    testChangeFile();
    std::cout << "\nAll tests passed!\n";
    return 0;
}
