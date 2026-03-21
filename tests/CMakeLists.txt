# Simple test executable
add_executable(test_types
    unit/test_types.cpp
    ../src/types.cpp
)

target_include_directories(test_types PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME test_types COMMAND test_types)

# Git scanner integration test
add_executable(test_git_scanner_integration
    integration/test_git_scanner_integration.cpp
    ../src/types.cpp
    ../src/core/git_scanner.cpp
)

target_include_directories(test_git_scanner_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME test_git_scanner_integration COMMAND test_git_scanner_integration)

# Config test
add_executable(test_config
    unit/test_config.cpp
    ../src/types.cpp
    ../src/core/config.cpp
)

target_include_directories(test_config PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME test_config COMMAND test_config)

# Transfer log test
add_executable(test_transfer_log
    unit/test_transfer_log.cpp
    ../src/types.cpp
    ../src/core/transfer_log.cpp
)

target_include_directories(test_transfer_log PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME test_transfer_log COMMAND test_transfer_log)
