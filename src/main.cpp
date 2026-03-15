#include "diffsync/cli/parser.hpp"
#include <iostream>

#ifdef BUILD_TUI
// TUI mode will be implemented in a future chunk
namespace diffsync {
CliResult runTuiMode(const ConfigManager& config) {
    std::cout << "TUI mode not yet implemented. Use --help for command line options.\n";
    return CliResult::Ok;
}
}
#endif

int main(int argc, char* argv[]) {
    using namespace diffsync;

    // Create CLI parser with TUI callback
    CLIParser parser([](const ConfigManager& config) -> CliResult {
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
