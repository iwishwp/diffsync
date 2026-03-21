#pragma once

#include "diffsync/types.hpp"
#include "diffsync/core/config.hpp"
#include "diffsync/core/git_scanner.hpp"
#include <CLI/CLI.hpp>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <memory>

namespace diffsync {

/**
 * Command line options for sync operations
 */
struct SyncOptions {
    std::string profile;              // Profile name to use
    std::string remoteOverride;       // Override remote (host:path)
    std::vector<std::string> excludes; // Exclude patterns
    std::vector<std::string> includes; // Include patterns
    bool dryRun = false;              // Preview only
    bool verbose = false;             // Verbose output
    std::vector<std::string> files;   // Specific files to sync
};

/**
 * Command line options for status command
 */
struct StatusOptions {
    bool shortFormat = false;  // Short output format
    bool verbose = false;      // Verbose output
};

/**
 * CLI result codes
 */
enum class CliResult {
    Ok,           // Success
    Continue,     // Continue to TUI (default mode)
    Error,        // Error occurred
    Help,         // Help was shown
};

/**
 * CLIParser - Command line interface parser using CLI11
 *
 * Parses command line arguments and dispatches to appropriate handlers.
 * Default behavior (no args): Launch TUI mode.
 */
class CLIParser {
public:
    /**
     * Callback type for TUI mode
     */
    using TuiCallback = std::function<CliResult(ConfigManager&)>;

    /**
     * Create parser with optional TUI callback
     */
    explicit CLIParser(TuiCallback tuiCallback = nullptr);

    /**
     * Parse command line arguments
     * @return Result code
     */
    CliResult parse(int argc, char* argv[]);

    /**
     * Get the parsed sync options (valid after parse)
     */
    const SyncOptions& syncOptions() const { return syncOptions_; }

    /**
     * Get the parsed status options (valid after parse)
     */
    const StatusOptions& statusOptions() const { return statusOptions_; }

    /**
     * Get selected profile name from environment or args
     */
    std::string getSelectedProfile() const;

private:
    std::unique_ptr<CLI::App> app_;
    TuiCallback tuiCallback_;
    SyncOptions syncOptions_;
    StatusOptions statusOptions_;
    std::string configPathOverride_;
    std::string profileEnv_;

    // Subcommands
    CLI::App* statusCmd_ = nullptr;
    CLI::App* syncCmd_ = nullptr;
    CLI::App* tuiCmd_ = nullptr;
    CLI::App* configCmd_ = nullptr;

    // Config subcommands
    CLI::App* configAddCmd_ = nullptr;
    CLI::App* configListCmd_ = nullptr;
    CLI::App* configRemoveCmd_ = nullptr;
    CLI::App* configUseCmd_ = nullptr;

    // Setup methods
    void setupGlobalOptions();
    void setupStatusCommand();
    void setupSyncCommand();
    void setupTuiCommand();
    void setupConfigCommand();

    // Command handlers
    CliResult handleStatus(const StatusOptions& options);
    CliResult handleSync(const SyncOptions& options);
    CliResult handleTui();
    CliResult handleConfigAdd(const std::string& name);
    CliResult handleConfigList();
    CliResult handleConfigRemove(const std::string& name);
    CliResult handleConfigUse(const std::string& name);

    // Environment variable helpers
    void loadEnvironmentVariables();
    std::string getEnvVar(const char* name);
    ConfigManager createConfigManager() const;
};

/**
 * Command implementations
 */
namespace commands {

/**
 * Status command - List changed files
 * @param scanner Git scanner instance
 * @param options Status options
 * @return CliResult::Ok on success, CliResult::Error on failure
 */
CliResult statusCommand(GitScanner& scanner, const StatusOptions& options);

/**
 * Sync command - Sync files to remote
 * @param scanner Git scanner instance
 * @param config Config manager
 * @param options Sync options
 * @return CliResult::Ok on success, CliResult::Error on failure
 */
CliResult syncCommand(GitScanner& scanner, const ConfigManager& config, const SyncOptions& options);

/**
 * Config add command - Add or update a profile
 * @param config Config manager
 * @param name Profile name
 * @return CliResult::Ok on success, CliResult::Error on failure
 */
CliResult configAddCommand(ConfigManager& config, const std::string& name);

/**
 * Config list command - List all profiles
 * @param config Config manager
 * @return CliResult::Ok
 */
CliResult configListCommand(const ConfigManager& config);

/**
 * Config remove command - Remove a profile
 * @param config Config manager
 * @param name Profile name
 * @return CliResult::Ok on success, CliResult::Error on failure
 */
CliResult configRemoveCommand(ConfigManager& config, const std::string& name);

/**
 * Config use command - Set current profile
 * @param config Config manager
 * @param name Profile name
 * @return CliResult::Ok on success, CliResult::Error on failure
 */
CliResult configUseCommand(ConfigManager& config, const std::string& name);

} // namespace commands

} // namespace diffsync
