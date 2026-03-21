#include "diffsync/cli/parser.hpp"
#include <iostream>

#ifdef BUILD_TUI
#include "diffsync/tui/main_screen.hpp"

namespace diffsync {
CliResult runTuiMode(ConfigManager& config) {
    // Create git scanner for current directory
    GitScanner scanner;

    // Check if we're in a git repo
    if (!scanner.isGitRepo()) {
        std::cerr << "Error: Not a git repository\n";
        return CliResult::Error;
    }

    // Check if there are any changes
    auto files = scanner.scan();
    if (files.empty()) {
        std::cout << "No changes detected in repository.\n";
        return CliResult::Ok;
    }

    // Launch TUI (config modal allows selecting profile)
    MainScreen screen(scanner, config);
    SyncResult result = screen.run();

    return result.hasErrors() ? CliResult::Error : CliResult::Ok;
}
}
#endif

int main(int argc, char* argv[]) {
    using namespace diffsync;

    // Create CLI parser with TUI callback
    CLIParser parser([](ConfigManager& config) -> CliResult {
#ifdef BUILD_TUI
        return runTuiMode(config);
#else
        std::cerr << "Error: TUI mode not available (BUILD_TUI=OFF)\n";
        return CliResult::Error;
#endif
    });

    // Parse command line arguments
    CliResult result = parser.parse(argc, argv);

    // Handle result
    switch (result) {
        case CliResult::Ok:
            return 0;
        case CliResult::Continue:
            // Continue to TUI (shouldn't happen with current design)
            return 0;
        case CliResult::Error:
            return 1;
        case CliResult::Help:
            return 0;
    }

    return 0;
}
