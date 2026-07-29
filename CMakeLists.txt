cmake_minimum_required(VERSION 3.25)

project(ClaudeTokenPlugin VERSION 2.0.0 LANGUAGES CXX)

option(CLAUDE_TOKEN_PLUGIN_BUILD_TESTS "Build native scanner tests" ON)

add_library(ClaudeTokenPlugin SHARED
    src/ClaudeScanner.cpp
    src/CodexScanner.cpp
    src/JsonlReader.cpp
    src/PathDiscovery.cpp
    src/PluginMain.cpp
    src/TimeUtils.cpp
    src/TokenItem.cpp
    src/TokenScanner.cpp
)

set_target_properties(ClaudeTokenPlugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    OUTPUT_NAME "ClaudeTokenPlugin"
)

target_include_directories(ClaudeTokenPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/TrafficMonitor/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_definitions(ClaudeTokenPlugin PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(ClaudeTokenPlugin PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(ClaudeTokenPlugin PRIVATE shell32 ole32)

if(CLAUDE_TOKEN_PLUGIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
