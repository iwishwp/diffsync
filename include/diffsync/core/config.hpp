#pragma once

#include "diffsync/types.hpp"
#include <vector>
#include <optional>
#include <filesystem>

namespace diffsync {

/**
 * ConfigManager - Manages remote connection profiles
 *
 * Config file location: ~/.config/diffsync/config.toml
 */
class ConfigManager {
public:
    static constexpr const char* DEFAULT_CONFIG_DIR = ".config/diffsync";
    static constexpr const char* CONFIG_FILENAME = "config.toml";
    static constexpr int CURRENT_SCHEMA_VERSION = 1;

    /**
     * Create config manager
     * @param configPath Custom config path (optional)
     */
    explicit ConfigManager(const std::filesystem::path& configPath = {});

    /**
     * Load config from file
     * @return Error if config is corrupted
     */
    Error load();

    /**
     * Save config to file
     * @return Error if save fails
     */
    Error save();

    /**
     * Get all profiles
     */
    const std::vector<RemoteConfig>& profiles() const { return profiles_; }

    /**
     * Get current profile name
     */
    const std::string& currentProfile() const { return currentProfile_; }

    /**
     * Set current profile by name
     * @return Error if profile not found
     */
    Error setCurrentProfile(const std::string& name);

    /**
     * Get current profile config
     */
    std::optional<RemoteConfig> getCurrentConfig() const;

    /**
     * Add or update a profile (validates before adding)
     * @return Error if config is invalid
     */
    Error addProfile(const RemoteConfig& config);

    /**
     * Remove a profile
     * @return Error if removing current profile or profile not found
     */
    Error removeProfile(const std::string& name);

    /**
     * Get default exclude patterns
     */
    const std::vector<std::string>& defaultExcludes() const { return defaultExcludes_; }

    /**
     * Check if config file exists
     */
    bool configExists() const;

    /**
     * Get config file path
     */
    const std::filesystem::path& configPath() const { return configPath_; }

    /**
     * Create default config directory and file
     */
    Error createDefault();

private:
    std::filesystem::path configPath_;
    std::vector<RemoteConfig> profiles_;
    std::string currentProfile_;
    std::vector<std::string> defaultExcludes_;
    int schemaVersion_ = CURRENT_SCHEMA_VERSION;

    Error ensureConfigDirectory();
    std::filesystem::path getDefaultConfigPath() const;
    Error migrateFromVersion(int fromVersion);
    bool validateFilePermissions();
};

} // namespace diffsync
