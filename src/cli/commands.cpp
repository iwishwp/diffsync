#include "diffsync/cli/parser.hpp"
#include "diffsync/core/rsync.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace diffsync {

namespace {

// Helper function to format file size
std::string formatSize(uint64_t size) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double dsize = static_cast<double>(size);

    while (dsize >= 1024.0 && unit < 3) {
        dsize /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << dsize << units[unit];
    return oss.str();
}

} // anonymous namespace

namespace commands {

CliResult statusCommand(GitScanner& scanner, const StatusOptions& options) {
    if (!scanner.isGitRepo()) {
        std::cerr << "Error: Not a git repository\n";
        return CliResult::Error;
    }

    auto changedFiles = scanner.scan();
    if (changedFiles.empty()) {
        std::cout << "No changes detected.\n";
        return CliResult::Ok;
    }

    // Count files by status
    int modified = 0, added = 0, untracked = 0;

    if (options.shortFormat) {
        // Short format: just list files
        for (const auto& file : changedFiles) {
            std::cout << file.statusString() << " " << file.path.string() << "\n";
            switch (file.status) {
                case FileStatus::Modified:
                case FileStatus::Staged:
                    modified++;
                    break;
                case FileStatus::Added:
                    added++;
                    break;
                case FileStatus::Untracked:
                    untracked++;
                    break;
                default:
                    break;
            }
        }
    } else {
        // Verbose format: with sizes
        for (const auto& file : changedFiles) {
            std::cout << file.statusString() << " "
                      << std::setw(8) << formatSize(file.size) << " "
                      << file.path.string() << "\n";
            switch (file.status) {
                case FileStatus::Modified:
                case FileStatus::Staged:
                    modified++;
                    break;
                case FileStatus::Added:
                    added++;
                    break;
                case FileStatus::Untracked:
                    untracked++;
                    break;
                default:
                    break;
            }
        }
    }

    // Summary
    std::cout << "\nSummary: " << changedFiles.size() << " file";
    if (changedFiles.size() != 1) std::cout << "s";
    std::cout << " changed";
    if (options.verbose) {
        std::cout << " (";
        if (modified > 0) std::cout << modified << " modified";
        if (modified > 0 && (added > 0 || untracked > 0)) std::cout << ", ";
        if (added > 0) std::cout << added << " added";
        if (added > 0 && untracked > 0) std::cout << ", ";
        if (untracked > 0) std::cout << untracked << " untracked";
        std::cout << ")";
    }
    std::cout << "\n";

    return CliResult::Ok;
}

CliResult syncCommand(GitScanner& scanner, const ConfigManager& config,
                     const SyncOptions& options) {
    if (!scanner.isGitRepo()) {
        std::cerr << "Error: Not a git repository\n";
        return CliResult::Error;
    }

    // Get the remote configuration
    RemoteConfig remoteConfig;
    std::string profileName;

    if (!options.remoteOverride.empty()) {
        // Parse remote override (format: host:path or user@host:path)
        profileName = "override";
        // TODO: Parse host:path format
        std::cerr << "Error: Remote override parsing not yet implemented\n";
        return CliResult::Error;
    } else {
        // Use profile from config
        profileName = options.profile;
        if (profileName.empty()) {
            profileName = config.currentProfile();
        }

        auto cfg = config.getCurrentConfig();
        if (!cfg) {
            std::cerr << "Error: No profile selected. Use --profile or 'diffsync config use <name>'\n";
            return CliResult::Error;
        }
        remoteConfig = *cfg;
    }

    // Get files to sync
    std::vector<ChangeFile> filesToSync;
    if (options.files.empty()) {
        // Sync all changed files
        filesToSync = scanner.scan();
    } else {
        // Sync specific files
        auto allFiles = scanner.scan();
        for (const auto& file : allFiles) {
            for (const auto& pattern : options.files) {
                if (file.path.string().find(pattern) != std::string::npos) {
                    filesToSync.push_back(file);
                    break;
                }
            }
        }
    }

    if (filesToSync.empty()) {
        std::cout << "No files to sync.\n";
        return CliResult::Ok;
    }

    // Apply exclude patterns
    if (!options.excludes.empty()) {
        auto it = filesToSync.begin();
        while (it != filesToSync.end()) {
            bool excluded = false;
            for (const auto& pattern : options.excludes) {
                // Simple wildcard matching
                if (it->path.string().find(pattern) != std::string::npos) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) {
                it = filesToSync.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (filesToSync.empty()) {
        std::cout << "No files to sync after applying filters.\n";
        return CliResult::Ok;
    }

    // Build rsync options
    RsyncExecutor rsyncExecutor;
    RsyncExecutor::Options rsyncOptions;
    rsyncOptions.dryRun = options.dryRun;
    rsyncOptions.compress = true;
    rsyncOptions.verbose = options.verbose;
    rsyncOptions.sshPort = remoteConfig.port;
    rsyncOptions.identityFile = remoteConfig.identityFile;

    // Destination
    std::string dest = remoteConfig.user + "@" + remoteConfig.host + ":" + remoteConfig.remotePath + "/";

    std::cout << "Syncing " << filesToSync.size() << " file(s) to " << profileName << "\n";

    if (options.verbose || options.dryRun) {
        std::cout << "Destination: " << dest << "\n";
        for (const auto& file : filesToSync) {
            std::cout << "  " << file.path.string() << "\n";
        }
    }

    // Setup progress callback
    auto progressCallback = [&](const RsyncExecutor::Progress& progress) {
        if (options.verbose && progress.percent > 0) {
            std::cout << "\r  Progress: " << progress.percent << "% "
                      << RsyncExecutor::formatBytes(progress.bytesTransferred) << " "
                      << RsyncExecutor::formatSpeed(progress.speed);
            if (!progress.eta.empty()) {
                std::cout << " ETA: " << progress.eta;
            }
            std::cout << "        " << std::flush;
        }
    };

    // Execute rsync
    SyncResult syncResult = rsyncExecutor.execute(
        filesToSync,
        scanner.repoRoot(),
        remoteConfig,
        rsyncOptions,
        options.verbose ? progressCallback : std::function<void(const RsyncExecutor::Progress&)>()
    );

    // Handle results
    if (options.verbose) {
        std::cout << "\n";
    }

    if (syncResult.hasErrors()) {
        std::cerr << "Sync failed:\n";
        for (const auto& error : syncResult.errors) {
            std::cerr << "  " << error << "\n";
        }
        return CliResult::Error;
    }

    // Show summary
    std::cout << "Sync complete: " << syncResult.successCount << " file(s) transferred\n";
    if (syncResult.failedCount > 0) {
        std::cout << "  Failed: " << syncResult.failedCount << "\n";
    }

    return CliResult::Ok;
}

CliResult configAddCommand(ConfigManager& config, const std::string& name) {
    std::cout << "Adding profile: " << name << "\n";

    RemoteConfig newConfig;
    newConfig.name = name;

    // TODO: Implement interactive profile creation
    // For now, show what would be needed
    std::cout << "Please provide the following information:\n";
    std::cout << "  Host: ";
    std::string host;
    std::getline(std::cin, host);
    newConfig.host = host;

    std::cout << "  User: ";
    std::string user;
    std::getline(std::cin, user);
    newConfig.user = user;

    std::cout << "  Remote path: ";
    std::string path;
    std::getline(std::cin, path);
    newConfig.remotePath = path;

    std::cout << "  SSH port [22]: ";
    std::string portStr;
    std::getline(std::cin, portStr);
    if (!portStr.empty()) {
        newConfig.port = static_cast<uint16_t>(std::stoi(portStr));
    }

    std::cout << "  Identity file (optional, press Enter to skip): ";
    std::string keyFile;
    std::getline(std::cin, keyFile);
    if (!keyFile.empty()) {
        newConfig.identityFile = keyFile;
    }

    // Validate
    if (!newConfig.isValid()) {
        std::cerr << "Error: Invalid configuration\n";
        return CliResult::Error;
    }

    // Add to config
    auto error = config.addProfile(newConfig);
    if (error) {
        std::cerr << "Error: " << error.message << "\n";
        return CliResult::Error;
    }

    // Save
    error = config.save();
    if (error) {
        std::cerr << "Error saving config: " << error.message << "\n";
        return CliResult::Error;
    }

    std::cout << "Profile '" << name << "' added successfully.\n";
    return CliResult::Ok;
}

CliResult configListCommand(const ConfigManager& config) {
    const auto& profiles = config.profiles();

    if (profiles.empty()) {
        std::cout << "No profiles configured.\n";
        std::cout << "Use 'diffsync config add <name>' to add a profile.\n";
        return CliResult::Ok;
    }

    std::cout << "Configured profiles:\n\n";

    for (const auto& profile : profiles) {
        std::string prefix = "  ";
        if (profile.name == config.currentProfile()) {
            prefix = "* ";
        }
        std::cout << prefix << profile.name << "\n";
        std::cout << "    " << profile.user << "@" << profile.host
                  << ":" << profile.remotePath << "\n";
        if (!profile.identityFile.empty()) {
            std::cout << "    key: " << profile.identityFile << "\n";
        }
        std::cout << "\n";
    }

    return CliResult::Ok;
}

CliResult configRemoveCommand(ConfigManager& config, const std::string& name) {
    // Check if profile exists
    bool found = false;
    for (const auto& profile : config.profiles()) {
        if (profile.name == name) {
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "Error: Profile '" << name << "' not found.\n";
        return CliResult::Error;
    }

    // Confirm
    std::cout << "Remove profile '" << name << "'? [y/N] ";
    std::string response;
    std::getline(std::cin, response);
    if (response != "y" && response != "Y") {
        std::cout << "Cancelled.\n";
        return CliResult::Ok;
    }

    // Remove
    auto error = config.removeProfile(name);
    if (error) {
        std::cerr << "Error: " << error.message << "\n";
        return CliResult::Error;
    }

    // Save
    error = config.save();
    if (error) {
        std::cerr << "Error saving config: " << error.message << "\n";
        return CliResult::Error;
    }

    std::cout << "Profile '" << name << "' removed successfully.\n";
    return CliResult::Ok;
}

CliResult configUseCommand(ConfigManager& config, const std::string& name) {
    auto error = config.setCurrentProfile(name);
    if (error) {
        std::cerr << "Error: " << error.message << "\n";
        return CliResult::Error;
    }

    // Save
    error = config.save();
    if (error) {
        std::cerr << "Error saving config: " << error.message << "\n";
        return CliResult::Error;
    }

    std::cout << "Using profile: " << name << "\n";
    return CliResult::Ok;
}

} // namespace commands
} // namespace diffsync
