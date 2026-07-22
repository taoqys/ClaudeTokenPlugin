cmake_minimum_required(VERSION 3.15)
project(ClaudeTokenPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ClaudeTokenPlugin SHARED
    src/ClaudeTokenPlugin.cpp
    src/ClaudeTokenItem.cpp
    src/JsonlScanner.cpp
)

target_include_directories(ClaudeTokenPlugin PRIVATE src)

# Export the entry point
set_target_properties(ClaudeTokenPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
