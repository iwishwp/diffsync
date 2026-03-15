include(FetchContent)

# Set policy to suppress deprecation warnings in older CMake versions
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 NEW)
endif()
