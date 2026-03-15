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
