#include "diffsync/core/transfer_log.hpp"
#include <cassert>
#include <iostream>

namespace diffsync {
namespace test {

void testFailedFileLogging() {
    std::cout << "Running testFailedFileLogging...\n";

    TransferLog log;

    // 初始状态：没有失败
    assert(log.hasFailures() == false);
    assert(log.getCount(TransferStatus::Failed) == 0);
    assert(log.getEntries().empty());

    // 记录一个失败的文件
    log.logFile("src/main.cpp", TransferStatus::Failed, "Connection timeout");

    // 验证失败被记录
    assert(log.hasFailures() == true);
    assert(log.getCount(TransferStatus::Failed) == 1);
    assert(log.getCount(TransferStatus::Success) == 0);

    // 验证失败文件列表
    auto failedFiles = log.getFailedFiles();
    assert(failedFiles.size() == 1);
    assert(failedFiles[0].filename == "src/main.cpp");
    assert(failedFiles[0].status == TransferStatus::Failed);
    assert(failedFiles[0].errorMessage == "Connection timeout");

    std::cout << "  ✓ Failed file logged correctly\n";
}

void testMultipleFileStatuses() {
    std::cout << "Running testMultipleFileStatuses...\n";

    TransferLog log;

    // 记录多个不同状态的文件
    log.logFile("file1.txt", TransferStatus::Success, "");
    log.logFile("file2.txt", TransferStatus::Failed, "Permission denied");
    log.logFile("file3.txt", TransferStatus::Success, "");
    log.logFile("file4.txt", TransferStatus::Skipped, "");
    log.logFile("file5.txt", TransferStatus::Failed, "Host unreachable");

    // 验证计数
    assert(log.getCount(TransferStatus::Success) == 2);
    assert(log.getCount(TransferStatus::Failed) == 2);
    assert(log.getCount(TransferStatus::Skipped) == 1);
    assert(log.getEntries().size() == 5);

    // 验证失败检查
    assert(log.hasFailures() == true);

    // 验证失败文件列表
    auto failedFiles = log.getFailedFiles();
    assert(failedFiles.size() == 2);
    assert(failedFiles[0].filename == "file2.txt");
    assert(failedFiles[1].filename == "file5.txt");

    std::cout << "  ✓ Multiple file statuses tracked correctly\n";
}

void testRetrySuccess() {
    std::cout << "Running testRetrySuccess...\n";

    TransferLog log;

    // 第一次传输失败
    log.logFile("test.txt", TransferStatus::Failed, "Connection timeout");
    assert(log.hasFailures() == true);
    assert(log.getCount(TransferStatus::Failed) == 1);

    // 重试成功
    log.updateEntry("test.txt", TransferStatus::Success, "");

    // 验证状态更新
    assert(log.hasFailures() == false);
    assert(log.getCount(TransferStatus::Success) == 1);
    assert(log.getCount(TransferStatus::Failed) == 0);

    std::cout << "  ✓ Retry success updates log correctly\n";
}

void testRetryStillFails() {
    std::cout << "Running testRetryStillFails...\n";

    TransferLog log;

    // 第一次失败
    log.logFile("test.txt", TransferStatus::Failed, "Connection timeout");

    // 重试仍然失败（不同的错误）
    log.updateEntry("test.txt", TransferStatus::Failed, "Permission denied");

    // 验证仍然是失败状态
    assert(log.hasFailures() == true);
    assert(log.getCount(TransferStatus::Failed) == 1);

    // 验证错误消息已更新
    auto failedFiles = log.getFailedFiles();
    assert(failedFiles[0].errorMessage == "Permission denied");

    std::cout << "  ✓ Retry failure updates error message\n";
}

void testUpdateNonExistentEntry() {
    std::cout << "Running testUpdateNonExistentEntry...\n";

    TransferLog log;

    // 更新不存在的条目应该创建新条目
    log.updateEntry("newfile.txt", TransferStatus::Success, "");

    assert(log.getEntries().size() == 1);
    assert(log.getCount(TransferStatus::Success) == 1);

    std::cout << "  ✓ Update non-existent entry creates new one\n";
}

void testClearLog() {
    std::cout << "Running testClearLog...\n";

    TransferLog log;

    // 添加一些记录
    log.logFile("file1.txt", TransferStatus::Success, "");
    log.logFile("file2.txt", TransferStatus::Failed, "Error");
    log.logFile("file3.txt", TransferStatus::Skipped, "");

    assert(log.getEntries().size() == 3);
    assert(log.hasFailures() == true);

    // 清空日志
    log.clear();

    assert(log.getEntries().empty());
    assert(log.getCount(TransferStatus::Success) == 0);
    assert(log.getCount(TransferStatus::Failed) == 0);
    assert(!log.hasFailures());

    std::cout << "  ✓ Log cleared successfully\n";
}

void testLogAfterRetry() {
    std::cout << "Running testLogAfterRetry...\n";

    TransferLog log;

    // 模拟首次传输：3个成功，2个失败
    log.logFile("file1.txt", TransferStatus::Success, "");
    log.logFile("file2.txt", TransferStatus::Success, "");
    log.logFile("file3.txt", TransferStatus::Failed, "Error 1");
    log.logFile("file4.txt", TransferStatus::Success, "");
    log.logFile("file5.txt", TransferStatus::Failed, "Error 2");

    assert(log.getCount(TransferStatus::Success) == 3);
    assert(log.getCount(TransferStatus::Failed) == 2);

    // 重试失败的两个文件：一个成功，一个仍然失败
    log.updateEntry("file3.txt", TransferStatus::Success, "");
    log.updateEntry("file5.txt", TransferStatus::Failed, "Still failing");

    // 验证最终状态
    assert(log.getCount(TransferStatus::Success) == 4);  // 原来的3个 + 重试成功的1个
    assert(log.getCount(TransferStatus::Failed) == 1);   // 只有file5还失败
    assert(log.getEntries().size() == 5);  // 总共5个条目

    // 验证失败文件列表
    auto failedFiles = log.getFailedFiles();
    assert(failedFiles.size() == 1);
    assert(failedFiles[0].filename == "file5.txt");

    std::cout << "  ✓ Log state after partial retry correct\n";
}

} // namespace test
} // namespace diffsync

int main() {
    std::cout << "\n=== Transfer Log Unit Tests ===\n\n";

    diffsync::test::testFailedFileLogging();
    diffsync::test::testMultipleFileStatuses();
    diffsync::test::testRetrySuccess();
    diffsync::test::testRetryStillFails();
    diffsync::test::testUpdateNonExistentEntry();
    diffsync::test::testClearLog();
    diffsync::test::testLogAfterRetry();

    std::cout << "\n=== All Tests Passed! ===\n\n";
    return 0;
}
