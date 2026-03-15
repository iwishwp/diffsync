# Simple test executable
add_executable(test_types
    unit/test_types.cpp
    ../src/types.cpp
)

target_include_directories(test_types PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME test_types COMMAND test_types)
