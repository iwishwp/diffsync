#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include "diffsync/core/config.hpp"

using namespace diffsync;

void testConfigManager() {
    std::filesystem::path tempConfig = "/tmp/diffsync_test_config.toml";
    std::filesystem::remove(tempConfig);

    // Test create default
    {
        ConfigManager config(tempConfig);
        auto err = config.createDefault();
        assert(!err && "createDefault should succeed");
        assert(config.configExists());
        assert(!config.profiles().empty());
    }

    // Test load and validate
    {
        ConfigManager config(tempConfig);
        auto err = config.load();
        assert(!err && "load should succeed");

        // Test add profile with validation
        RemoteConfig valid;
        valid.name = "test";
        valid.host = "test.com";
        valid.user = "testuser";
        valid.remotePath = "/test";
        valid.port = 2222;

        err = config.addProfile(valid);
        assert(!err && "addProfile with valid config should succeed");

        // Save the config so it persists for the next test
        err = config.save();
        assert(!err && "save should succeed");

        // Test invalid profile (missing required field)
        RemoteConfig invalid;
        invalid.name = "invalid";
        // Missing host - should fail
        invalid.user = "user";
        invalid.remotePath = "/path";
        invalid.port = 22;

        err = config.addProfile(invalid);
        assert(err && "addProfile with invalid config should fail");

        // Test port validation
        RemoteConfig badPort;
        badPort.name = "badport";
        badPort.host = "host";
        badPort.user = "user";
        badPort.remotePath = "/path";
        badPort.port = 0;

        err = config.addProfile(badPort);
        assert(err && "addProfile with port=0 should fail");

        // Test empty profile name validation
        RemoteConfig emptyName;
        emptyName.name = "";
        emptyName.host = "host";
        emptyName.user = "user";
        emptyName.remotePath = "/path";
        emptyName.port = 22;

        err = config.addProfile(emptyName);
        assert(err && "addProfile with empty name should fail");
    }

    // Test current profile
    {
        ConfigManager config(tempConfig);
        config.load();

        auto current = config.getCurrentConfig();
        assert(current.has_value());

        auto err = config.setCurrentProfile("test");
        assert(!err && "setCurrentProfile should succeed");
        assert(config.currentProfile() == "test");

        err = config.removeProfile("test");
        assert(err && "cannot remove current profile");

        err = config.setCurrentProfile("example");
        err = config.removeProfile("test");
        assert(!err && "removeProfile should succeed");

        // Test removeProfile with non-existent profile
        err = config.removeProfile("nonexistent");
        assert(err && "removeProfile with non-existent profile should fail");
    }

    // Test malformed config handling
    {
        std::ofstream badConfig("/tmp/bad_config.toml");
        badConfig << "current_profile = \"incomplete\"\n";
        badConfig << "[[profiles]]\n";
        badConfig << "name = \"incomplete\"\n";
        badConfig << "# Missing host, user, remote_path\n";
        badConfig.close();

        ConfigManager bad("/tmp/bad_config.toml");
        auto err = bad.load();
        // The parser should reject this because the profile is invalid
        // If it doesn't reject during load, getCurrentConfig will fail
        assert(err && "loading malformed config should fail");

        std::filesystem::remove("/tmp/bad_config.toml");
    }

    std::filesystem::remove(tempConfig);
    std::cout << "ConfigManager tests passed\n";
}

int main() {
    testConfigManager();
    return 0;
}
