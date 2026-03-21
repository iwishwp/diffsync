#include "diffsync/core/config.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <system_error>
#include <iostream>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <thread>

namespace diffsync {

namespace {
    std::string expandTilde(const std::string& path) {
        if (path.empty() || path[0] != '~') return path;
        const char* home = std::getenv("HOME");
        if (!home) home = std::getenv("USERPROFILE");
        if (home) return std::string(home) + path.substr(1);
        return path;
    }

    // Simple TOML value extractor (handles basic string and int values)
    std::string extractTomlValue(const std::string& line) {
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) return "";

        std::string value = line.substr(eqPos + 1);

        // Trim whitespace
        size_t start = value.find_first_not_of(" \t");
        size_t end = value.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            value = value.substr(start, end - start + 1);
        }

        // Remove quotes if present
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        return value;
    }

    // Safe integer parsing with error handling
    std::optional<int> parsePort(const std::string& value) {
        try {
            int port = std::stoi(value);
            if (port > 0 && port <= 65535) return port;
        } catch (...) {
            return std::nullopt;
        }
        return std::nullopt;
    }
}

ConfigManager::ConfigManager(const std::filesystem::path& configPath) {
    if (configPath.empty()) {
        configPath_ = getDefaultConfigPath();
    } else {
        configPath_ = configPath;
    }

    defaultExcludes_ = {".git/*", "*.o", "*.a", "build/*", "node_modules/*"};
}

std::filesystem::path ConfigManager::getDefaultConfigPath() const {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    if (!home) return ".diffsync/config.toml";

    std::filesystem::path configDir(home);
    configDir /= DEFAULT_CONFIG_DIR;
    configDir /= CONFIG_FILENAME;
    return configDir;
}

Error ConfigManager::ensureConfigDirectory() {
    std::filesystem::path dir = configPath_.parent_path();
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            return Error(ErrorCode::ConfigCorrupted,
                "failed to create config directory: " + ec.message());
        }
    }
    return Error();
}

bool ConfigManager::configExists() const {
    return std::filesystem::exists(configPath_);
}

Error ConfigManager::createDefault() {
    auto err = ensureConfigDirectory();
    if (err) return err;

    RemoteConfig example;
    example.name = "example";
    example.host = "build.example.com";
    example.user = "builder";
    example.remotePath = "~/projects/myapp";
    example.port = 22;

    profiles_.push_back(example);
    currentProfile_ = "example";

    return save();
}

Error ConfigManager::migrateFromVersion(int fromVersion) {
    // Schema version 1 is initial version, no migration needed yet
    // Future versions add migration logic here
    (void)fromVersion;  // Suppress unused parameter warning
    return Error();
}

bool ConfigManager::validateFilePermissions() {
    if (!configExists()) return true;

    auto perms = std::filesystem::status(configPath_).permissions();
    bool hasGroupRead = (perms & std::filesystem::perms::group_read) != std::filesystem::perms::none;
    bool hasOtherRead = (perms & std::filesystem::perms::others_read) != std::filesystem::perms::none;

    if (hasGroupRead || hasOtherRead) {
        std::cerr << "Warning: Config file has open permissions. Consider running: chmod 600 "
                  << configPath_ << "\n";
        return false;
    }
    return true;
}

Error ConfigManager::load() {
    if (!configExists()) {
        return Error(ErrorCode::ConfigCorrupted, "config file does not exist");
    }

    std::ifstream file(configPath_);
    if (!file.is_open()) {
        return Error(ErrorCode::ConfigCorrupted, "cannot open config file");
    }

    profiles_.clear();
    currentProfile_.clear();
    schemaVersion_ = 1;  // Default

    validateFilePermissions();

    std::string line;
    bool inProfiles = false;
    RemoteConfig currentProfile;
    currentProfile.port = 22;  // Default

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Parse schema_version
        if (line.find("schema_version") == 0) {
            auto value = extractTomlValue(line);
            try { schemaVersion_ = std::stoi(value); } catch (...) {}
            continue;
        }

        // Parse current_profile
        if (line.find("current_profile") == 0) {
            currentProfile_ = extractTomlValue(line);
            continue;
        }

        // Detect [[profiles]] section
        if (line.find("[[profiles]]") == 0) {
            if (currentProfile.isValid()) {
                profiles_.push_back(currentProfile);
            }
            currentProfile = RemoteConfig{};
            currentProfile.port = 22;
            inProfiles = true;
            continue;
        }

        // Parse profile fields
        if (inProfiles) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string valueStr = line.substr(eqPos + 1);

                size_t kEnd = key.find_last_not_of(" \t");
                if (kEnd != std::string::npos) key = key.substr(0, kEnd + 1);

                std::string value = extractTomlValue(line);

                if (key == "name") currentProfile.name = value;
                else if (key == "host") currentProfile.host = value;
                else if (key == "user") currentProfile.user = value;
                else if (key == "remote_path") currentProfile.remotePath = value;
                else if (key == "identity_file") currentProfile.identityFile = expandTilde(value);
                else if (key == "password") currentProfile.password = value;
                else if (key == "port") {
                    auto port = parsePort(value);
                    if (port) currentProfile.port = *port;
                }
            }
        }
    }

    // Add last profile
    if (currentProfile.isValid()) {
        profiles_.push_back(currentProfile);
    }

    // Set current profile if not set
    if (currentProfile_.empty() && !profiles_.empty()) {
        currentProfile_ = profiles_[0].name;
    }

    // Handle schema migration
    if (schemaVersion_ < CURRENT_SCHEMA_VERSION) {
        auto err = migrateFromVersion(schemaVersion_);
        if (err) return err;
    }

    // Validate all loaded profiles
    for (const auto& profile : profiles_) {
        if (!profile.isValid()) {
            return Error(ErrorCode::ConfigCorrupted, "invalid profile found in config");
        }
    }

    // Validate that current_profile exists
    if (!currentProfile_.empty()) {
        bool found = false;
        for (const auto& profile : profiles_) {
            if (profile.name == currentProfile_) {
                found = true;
                break;
            }
        }
        if (!found) {
            return Error(ErrorCode::ConfigCorrupted, "current_profile '" + currentProfile_ + "' not found in profiles");
        }
    }

    return Error();
}

Error ConfigManager::save() {
    auto err = ensureConfigDirectory();
    if (err) return err;

    // Open with timeout for file locking
    int fd = open(configPath_.c_str(), O_WRONLY | O_CREAT, 0600);
    if (fd == -1) {
        return Error(ErrorCode::ConfigCorrupted, "cannot open config for writing");
    }

    // Try to get lock with 5 second timeout
    int maxRetries = 50;  // 50 * 100ms = 5 seconds
    for (int i = 0; i < maxRetries; ++i) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) break;
        if (i == maxRetries - 1) {
            close(fd);
            return Error(ErrorCode::ConfigCorrupted, "config file is locked (timeout)");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Open file for writing (truncates existing content)
    // Note: fd is kept open for locking, file is used for actual I/O
    std::ofstream file(configPath_, std::ios::trunc);
    if (!file.is_open()) {
        flock(fd, LOCK_UN);
        close(fd);
        return Error(ErrorCode::ConfigCorrupted, "cannot write config file");
    }

    file << "# diffsync configuration\n";
    file << "schema_version = " << CURRENT_SCHEMA_VERSION << "\n";
    file << "current_profile = \"" << currentProfile_ << "\"\n\n";
    file << "default_excludes = [\n";
    for (const auto& excl : defaultExcludes_) {
        file << "    \"" << excl << "\",\n";
    }
    file << "]\n\n";

    for (const auto& profile : profiles_) {
        file << "[[profiles]]\n";
        file << "name = \"" << profile.name << "\"\n";
        file << "host = \"" << profile.host << "\"\n";
        file << "user = \"" << profile.user << "\"\n";
        file << "remote_path = \"" << profile.remotePath << "\"\n";
        file << "port = " << profile.port << "\n";
        if (!profile.identityFile.empty()) {
            file << "identity_file = \"" << profile.identityFile << "\"\n";
        }
        // SECURITY WARNING: Password is stored in plain text!
        // Use SSH keys instead for better security.
        if (!profile.password.empty()) {
            file << "password = \"" << profile.password << "\"\n";
        }
        file << "\n";
    }

    file.close();
    flock(fd, LOCK_UN);
    close(fd);

    // Set secure permissions
    std::filesystem::permissions(configPath_,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);

    return Error();
}

Error ConfigManager::setCurrentProfile(const std::string& name) {
    for (const auto& profile : profiles_) {
        if (profile.name == name) {
            currentProfile_ = name;
            return Error();
        }
    }
    return Error(ErrorCode::ConfigCorrupted, "profile not found: " + name);
}

std::optional<RemoteConfig> ConfigManager::getCurrentConfig() const {
    for (const auto& profile : profiles_) {
        if (profile.name == currentProfile_) {
            return profile;
        }
    }
    return std::nullopt;
}

Error ConfigManager::addProfile(const RemoteConfig& config) {
    // Validate profile name is not empty
    if (config.name.empty()) {
        return Error(ErrorCode::ConfigCorrupted, "profile name cannot be empty");
    }

    // Validate before adding
    if (!config.isValid()) {
        return Error(ErrorCode::ConfigCorrupted, "invalid config: missing required fields");
    }

    // Remove existing profile with same name
    auto it = std::remove_if(profiles_.begin(), profiles_.end(),
        [&config](const RemoteConfig& p) { return p.name == config.name; });
    profiles_.erase(it, profiles_.end());

    profiles_.push_back(config);

    if (profiles_.size() == 1 || currentProfile_.empty()) {
        currentProfile_ = config.name;
    }

    return Error();
}

Error ConfigManager::removeProfile(const std::string& name) {
    if (name == currentProfile_) {
        return Error(ErrorCode::ConfigCorrupted, "cannot remove current profile");
    }

    size_t oldSize = profiles_.size();
    auto it = std::remove_if(profiles_.begin(), profiles_.end(),
        [&name](const RemoteConfig& p) { return p.name == name; });
    profiles_.erase(it, profiles_.end());

    if (profiles_.size() == oldSize) {
        return Error(ErrorCode::ConfigCorrupted, "profile not found: " + name);
    }

    return Error();
}

} // namespace diffsync
