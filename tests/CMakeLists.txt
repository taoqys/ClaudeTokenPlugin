function(configure_plugin_target target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/TrafficMonitor/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    )

    target_compile_definitions(${target} PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()

    target_link_libraries(${target} PRIVATE shell32 ole32)
endfunction()

function(add_plugin_test target)
    add_executable(${target} ${ARGN})
    configure_plugin_target(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_plugin_test(ClaudeTokenPluginScannerTests
    ScannerTests.cpp
    ../src/ClaudeScanner.cpp
    ../src/CodexScanner.cpp
    ../src/JsonlReader.cpp
    ../src/PathDiscovery.cpp
    ../src/TimeUtils.cpp
    ../src/TokenScanner.cpp
)

add_plugin_test(ClaudeTokenPluginTimeUtilsTests
    TimeUtilsTests.cpp
    ../src/TimeUtils.cpp
)

add_plugin_test(ClaudeTokenPluginAbiTests
    PluginAbiTests.cpp
    ../src/ClaudeScanner.cpp
    ../src/CodexScanner.cpp
    ../src/JsonlReader.cpp
    ../src/PathDiscovery.cpp
    ../src/PluginMain.cpp
    ../src/TimeUtils.cpp
    ../src/TokenItem.cpp
    ../src/TokenScanner.cpp
)
target_compile_definitions(ClaudeTokenPluginAbiTests PRIVATE CLAUDE_TOKEN_PLUGIN_TESTING)

add_executable(ScannerParityHarness
    ScannerParityHarness.cpp
    ../src/ClaudeScanner.cpp
    ../src/CodexScanner.cpp
    ../src/JsonlReader.cpp
    ../src/PathDiscovery.cpp
    ../src/TimeUtils.cpp
    ../src/TokenScanner.cpp
)
configure_plugin_target(ScannerParityHarness)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME TokenTrackerParity
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/TokenTrackerParity.py"
    )
    set_tests_properties(TokenTrackerParity PROPERTIES
        SKIP_RETURN_CODE 77
        ENVIRONMENT "SCANNER_PARITY_HARNESS=$<TARGET_FILE:ScannerParityHarness>"
    )
endif()
