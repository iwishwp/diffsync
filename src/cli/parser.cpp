#include "diffsync/cli/parser.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <cstdlib>

namespace diffsync {

CLIParser::CLIParser(TuiCallback tuiCallback)
    : tuiCallback_(std::move(tuiCallback))
{
    app_ = std::make_unique<CLI::App>("DiffSync - Git-aware file synchronization tool");
    app_->require_subcommand(0, 1);  // 0-1 subcommands (default to TUI)

    // Load environment variables first
    loadEnvironmentVariables();

    // Setup all components
    setupGlobalOptions();
    setupStatusCommand();
    setupSyncCommand();
    setupTuiCommand();
    setupConfigCommand();
}

void CLIParser::loadEnvironmentVariables() {
    profileEnv_ = getEnvVar("DIFFSYNC_PROFILE");
    configPathOverride_ = getEnvVar("DIFFSYNC_CONFIG");
}

std::string CLIParser::getEnvVar(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

void CLIParser::setupGlobalOptions() {
    // Global options available to all subcommands
    app_->add_option_function<std::string>(
        "--profile",
        [this](const std::string& profile) {
            syncOptions_.profile = profile;
        },
        "Configuration profile to use"
    )->envname("DIFFSYNC_PROFILE");

    app_->add_option_function<std::string>(
        "-c,--config",
        [this](const std::string& path) {
            configPathOverride_ = path;
        },
        "Custom config file path"
    )->envname("DIFFSYNC_CONFIG");

    app_->add_flag("-v,--verbose",
        [this](bool verbose) {
            syncOptions_.verbose = verbose;
            statusOptions_.verbose = verbose;
        },
        "Verbose output"
    );
}

void CLIParser::setupStatusCommand() {
    statusCmd_ = app_->add_subcommand("status", "List changed files")
        ->callback([this]() {
            handleStatus(statusOptions_);
        });

    statusCmd_->add_flag("-s,--short",
        statusOptions_.shortFormat,
        "Short output format"
    );
}

void CLIParser::setupSyncCommand() {
    syncCmd_ = app_->add_subcommand("sync", "Sync files to remote")
        ->callback([this]() {
            handleSync(syncOptions_);
        });

    syncCmd_->add_option(
        "--remote",
        syncOptions_.remoteOverride,
        "Override remote (host:path)"
    );

    syncCmd_->add_option(
        "-e,--exclude",
        syncOptions_.excludes,
        "Exclude patterns"
    )->allow_extra_args(false);

    syncCmd_->add_option(
        "-i,--include",
        syncOptions_.includes,
        "Include patterns"
    )->allow_extra_args(false);

    syncCmd_->add_flag(
        "-n,--dry-run",
        syncOptions_.dryRun,
        "Preview mode (show what would be synced)"
    );

    syncCmd_->add_option(
        "files",
        syncOptions_.files,
        "Specific files to sync"
    )->allow_extra_args(true);
}

void CLIParser::setupTuiCommand() {
    tuiCmd_ = app_->add_subcommand("tui", "Launch TUI mode")
        ->callback([this]() {
            handleTui();
        });
}

void CLIParser::setupConfigCommand() {
    configCmd_ = app_->add_subcommand("config", "Configuration management");

    // Config add subcommand
    std::string addName;
    configAddCmd_ = configCmd_->add_subcommand("add", "Add or update a profile")
        ->callback([this, &addName]() {
            handleConfigAdd(addName);
        });
    configAddCmd_->add_option("name", addName, "Profile name")->required();

    // Config list subcommand
    configListCmd_ = configCmd_->add_subcommand("list", "List all profiles")
        ->callback([this]() {
            handleConfigList();
        });

    // Config remove subcommand
    std::string removeName;
    configRemoveCmd_ = configCmd_->add_subcommand("remove", "Remove a profile")
        ->callback([this, &removeName]() {
            handleConfigRemove(removeName);
        });
    configRemoveCmd_->add_option("name", removeName, "Profile name")->required();

    // Config use subcommand
    std::string useName;
    configUseCmd_ = configCmd_->add_subcommand("use", "Set current profile")
        ->callback([this, &useName]() {
            handleConfigUse(useName);
        });
    configUseCmd_->add_option("name", useName, "Profile name")->required();
}

CliResult CLIParser::parse(int argc, char* argv[]) {
    try {
        // Pre-parse to check if we have a subcommand
        if (argc == 1) {
            // No arguments - default to TUI mode
            return handleTui();
        }

        app_->parse(argc, argv);

        // If no subcommand was provided, launch TUI
        if (app_->get_subcommands().empty()) {
            return handleTui();
        }

        return CliResult::Ok;

    } catch (const CLI::ParseError& e) {
        // Handle help and version
        if (e.get_name() == "CallForHelp") {
            std::cout << app_->help();
            return CliResult::Help;
        }
        if (e.get_name() == "CallForAllHelp") {
            std::cout << app_->help("", CLI::AppFormatMode::All);
            return CliResult::Help;
        }
        std::cerr << "Parse error: " << e.what() << "\n";
        return CliResult::Error;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return CliResult::Error;
    }
}

std::string CLIParser::getSelectedProfile() const {
    if (!syncOptions_.profile.empty()) {
        return syncOptions_.profile;
    }
    return profileEnv_;
}

ConfigManager CLIParser::createConfigManager() const {
    if (!configPathOverride_.empty()) {
        return ConfigManager(std::filesystem::path(configPathOverride_));
    }
    return ConfigManager();
}

CliResult CLIParser::handleStatus(const StatusOptions& options) {
    auto config = createConfigManager();
    auto error = config.load();
    if (error && config.configExists()) {
        std::cerr << "Warning: Could not load config: " << error.message << "\n";
    }

    GitScanner scanner;
    return commands::statusCommand(scanner, options);
}

CliResult CLIParser::handleSync(const SyncOptions& options) {
    auto config = createConfigManager();
    auto error = config.load();
    if (error && config.configExists()) {
        std::cerr << "Warning: Could not load config: " << error.message << "\n";
    }

    GitScanner scanner;
    return commands::syncCommand(scanner, config, options);
}

CliResult CLIParser::handleTui() {
    if (tuiCallback_) {
        auto config = createConfigManager();
        auto error = config.load();
        if (error && config.configExists()) {
            std::cerr << "Warning: Could not load config: " << error.message << "\n";
        }
        return tuiCallback_(config);
    } else {
        std::cerr << "Error: TUI mode not available (BUILD_TUI=OFF)\n";
        return CliResult::Error;
    }
}

CliResult CLIParser::handleConfigAdd(const std::string& name) {
    auto config = createConfigManager();
    auto error = config.load();
    // It's ok if config doesn't exist yet
    if (error && config.configExists()) {
        std::cerr << "Error loading config: " << error.message << "\n";
        return CliResult::Error;
    }

    return commands::configAddCommand(config, name);
}

CliResult CLIParser::handleConfigList() {
    auto config = createConfigManager();
    auto error = config.load();
    if (error && config.configExists()) {
        std::cerr << "Error loading config: " << error.message << "\n";
        return CliResult::Error;
    }

    return commands::configListCommand(config);
}

CliResult CLIParser::handleConfigRemove(const std::string& name) {
    auto config = createConfigManager();
    auto error = config.load();
    if (error && config.configExists()) {
        std::cerr << "Error loading config: " << error.message << "\n";
        return CliResult::Error;
    }

    return commands::configRemoveCommand(config, name);
}

CliResult CLIParser::handleConfigUse(const std::string& name) {
    auto config = createConfigManager();
    auto error = config.load();
    if (error && config.configExists()) {
        std::cerr << "Error loading config: " << error.message << "\n";
        return CliResult::Error;
    }

    return commands::configUseCommand(config, name);
}

} // namespace diffsync
