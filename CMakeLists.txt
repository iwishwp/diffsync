cmake_minimum_required(VERSION 3.16)
project(diffsync VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_TUI "Build TUI components" ON)

# Dependencies
include(cmake/FetchContent.cmake)

# FTXUI for TUI
if(BUILD_TUI)
    FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v5.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(ftxui)
endif()

# toml++ for config
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# CLI11 for CLI parsing
FetchContent_Declare(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cli11)

# Source files
set(SOURCES
    src/main.cpp
    src/types.cpp
    src/core/git_scanner.cpp
    src/core/config.cpp
    src/core/rsync.cpp
    src/core/transfer_log.cpp
    src/cli/parser.cpp
    src/cli/commands.cpp
)

# TUI source files
if(BUILD_TUI)
    list(APPEND SOURCES
        src/tui/file_list.cpp
        src/tui/config_modal.cpp
        src/tui/log_modal.cpp
        src/tui/main_screen.cpp
    )
endif()

# Header files
set(HEADERS
    include/diffsync/types.hpp
    include/diffsync/core/git_scanner.hpp
    include/diffsync/core/config.hpp
    include/diffsync/core/rsync.hpp
    include/diffsync/core/transfer_log.hpp
    include/diffsync/cli/parser.hpp
)

# TUI header files
if(BUILD_TUI)
    list(APPEND HEADERS
        include/diffsync/tui/file_list.hpp
        include/diffsync/tui/config_modal.hpp
        include/diffsync/tui/log_modal.hpp
        include/diffsync/tui/main_screen.hpp
    )
endif()

# Main executable
add_executable(diffsync ${SOURCES} ${HEADERS})

target_include_directories(diffsync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(diffsync PRIVATE
    CLI11::CLI11
    tomlplusplus::tomlplusplus
)

if(BUILD_TUI)
    target_link_libraries(diffsync PRIVATE ftxui::screen ftxui::component)
    target_compile_definitions(diffsync PRIVATE BUILD_TUI=1)
endif()

# Testing
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(diffsync PRIVATE /W4)
else()
    target_compile_options(diffsync PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS diffsync
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
