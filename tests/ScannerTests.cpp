#include "ClaudeScanner.h"
#include "CodexScanner.h"
#include "PathDiscovery.h"
#include "TokenScanner.h"

#include <windows.h>

#include <cstdio>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
constexpr std::uint64_t kTicksPerHour = 60ULL * 60ULL * 10'000'000ULL;

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        const auto base = std::filesystem::temp_directory_path();
        m_path = base / ("ClaudeTokenPluginTests-" + std::to_string(GetCurrentProcessId()) +
                         "-" + std::to_string(GetTickCount64()));
        std::filesystem::create_directories(m_path);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    const std::filesystem::path& Path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

void WriteText(const std::filesystem::path& path, const std::string& content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output)
        throw std::runtime_error("Cannot write test fixture: " + path.string());
    output << content;
}

void SetFileTime(const std::filesystem::path& path, std::uint64_t file_time_ticks)
{
    HANDLE handle = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open fixture to set mtime");

    ULARGE_INTEGER raw{};
    raw.QuadPart = file_time_ticks;
    FILETIME file_time{};
    file_time.dwLowDateTime = raw.LowPart;
    file_time.dwHighDateTime = raw.HighPart;
    const BOOL succeeded = SetFileTime(handle, nullptr, nullptr, &file_time);
    CloseHandle(handle);
    if (!succeeded)
        throw std::runtime_error("Cannot set fixture mtime");
}

std::string FormatUtcIso8601(std::uint64_t file_time_ticks)
{
    ULARGE_INTEGER raw{};
    raw.QuadPart = file_time_ticks;
    FILETIME file_time{};
    file_time.dwLowDateTime = raw.LowPart;
    file_time.dwHighDateTime = raw.HighPart;

    SYSTEMTIME utc{};
    if (!FileTimeToSystemTime(&file_time, &utc))
        throw std::runtime_error("Cannot format test timestamp");

    char timestamp[32]{};
    std::snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                  utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond);
    return timestamp;
}

TimeContext TestContext()
{
    const auto context = MakeTimeContext();
    if (context.now_ticks == 0 || context.local_today.year == 0)
        throw std::runtime_error("Cannot create time context");
    return context;
}

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void SetEnvironmentValue(const wchar_t* name, const std::wstring& value)
{
    if (!SetEnvironmentVariableW(name, value.c_str()))
        throw std::runtime_error("Cannot set test environment variable");
}

class EnvironmentValueGuard
{
public:
    explicit EnvironmentValueGuard(const wchar_t* name)
        : m_name(name)
    {
        const DWORD capacity = GetEnvironmentVariableW(m_name.c_str(), nullptr, 0);
        if (capacity == 0)
            return;

        std::wstring original(capacity, L'\0');
        const DWORD written = GetEnvironmentVariableW(m_name.c_str(), original.data(), capacity);
        if (written == 0 || written >= capacity)
            throw std::runtime_error("Cannot read test environment variable");
        original.resize(written);
        m_original = std::move(original);
    }

    ~EnvironmentValueGuard()
    {
        SetEnvironmentVariableW(m_name.c_str(), m_original ? m_original->c_str() : nullptr);
    }

private:
    std::wstring m_name;
    std::optional<std::wstring> m_original;
};

void TestEnvironmentDiscovery()
{
    TemporaryDirectory temporary;
    const auto claude_config = temporary.Path() / L"claude-config";
    const auto codex_home = temporary.Path() / L"codex-home";
    EnvironmentValueGuard claude_guard(L"CLAUDE_CONFIG_DIR");
    EnvironmentValueGuard codex_guard(L"CODEX_HOME");

    SetEnvironmentValue(L"CLAUDE_CONFIG_DIR", claude_config.wstring());
    SetEnvironmentValue(L"CODEX_HOME", codex_home.wstring());
    const auto paths = DiscoverDefaultPaths();

    Require(!paths.claude_roots.empty() &&
                std::find(paths.claude_roots.begin(), paths.claude_roots.end(), claude_config / L"projects") !=
                    paths.claude_roots.end(),
            "CLAUDE_CONFIG_DIR should produce its projects root");
    Require(paths.codex_session_roots.size() == 1 && paths.codex_session_roots.front() == codex_home / L"sessions",
            "CODEX_HOME should produce its sessions root");
}

void TestClaude()
{
    TemporaryDirectory temporary;
    const auto context = TestContext();
    const auto timestamp = FormatUtcIso8601(context.now_ticks);
    const auto old_timestamp = FormatUtcIso8601(context.cutoff_ticks - kTicksPerHour);
    const auto root = temporary.Path() / L"claude" / L"projects";
    const auto first = root / L"project-a" / L"one.jsonl";
    const auto second = root / L"project-b" / L"two.jsonl";

    WriteText(first,
        std::string("{\"type\":\"assistant\",\"timestamp\":\"") + timestamp +
        "\",\"requestId\":\"req-1\",\"message\":{\"id\":\"msg-1\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"cache_creation_input_tokens\":30,\"cache_read_input_tokens\":40}}}\n" +
        "{\"type\":\"user\",\"timestamp\":\"" + timestamp + "\"}\n" +
        "{not-json}\n" +
        "{\"type\":\"assistant\",\"timestamp\":\"" + old_timestamp +
        "\",\"requestId\":\"old\",\"message\":{\"id\":\"old\",\"usage\":{\"input_tokens\":999}}}\n");
    WriteText(second,
        std::string("{\"type\":\"assistant\",\"timestamp\":\"") + timestamp +
        "\",\"requestId\":\"req-1\",\"message\":{\"id\":\"msg-1\",\"usage\":{\"input_tokens\":999}}}\n" +
        "{\"type\":\"assistant\",\"timestamp\":\"" + timestamp +
        "\",\"requestId\":\"req-2\",\"message\":{\"id\":\"msg-1\",\"usage\":{\"input_tokens\":7}}}\n" +
        "{\"type\":\"assistant\",\"timestamp\":\"" + timestamp +
        "\",\"requestId\":\"zero\",\"message\":{\"id\":\"zero\",\"usage\":{\"input_tokens\":0}}}\n" +
        "{\"type\":\"assistant\",\"timestamp\":\"" + timestamp +
        "\",\"requestId\":\"comment\",\"message\":{\"id\":\"comment\",\"usage\":{\"input_tokens\":500}}}// not JSONL\n");
    SetFileTime(first, context.now_ticks);
    SetFileTime(second, context.now_ticks);

    const auto result = ScanClaudeToday({root}, context);
    Require(result.completed, "Claude scan should complete");
    Require(result.tokens == 107, "Claude total should include all four usage fields and deduplicate");
    Require(result.valid_entries == 2, "Claude should retain two valid unique entries");
}

void TestCodex()
{
    TemporaryDirectory temporary;
    const auto context = TestContext();
    const auto timestamp = FormatUtcIso8601(context.now_ticks);
    const auto root = temporary.Path() / L"codex" / L"sessions";
    const auto first = root / L"session-one.jsonl";
    const auto second = root / L"session-two.jsonl";

    WriteText(first,
        std::string("{\"type\":\"session_meta\",\"payload\":{\"id\":\"session-1\",\"timestamp\":\"") + timestamp +
        "\"}}\n" +
        "{\"type\":\"event_msg\",\"payload\":{\"type\":\"token_count\",\"info\":{\"total_token_usage\":{\"input_tokens\":10,\"cached_input_tokens\":3,\"output_tokens\":4,\"reasoning_output_tokens\":5}}}}\n" +
        "{\"type\":\"event_msg\",\"payload\":{\"type\":\"token_count\",\"info\":{\"total_token_usage\":{\"input_tokens\":100,\"cached_input_tokens\":30,\"output_tokens\":40,\"reasoning_output_tokens\":50}}}}\n");
    WriteText(second,
        std::string("{\"type\":\"session_meta\",\"payload\":{\"id\":\"session-2\",\"timestamp\":\"") + timestamp +
        "\"}}\n" +
        "{\"type\":\"event_msg\",\"payload\":{\"type\":\"token_count\",\"info\":{\"total_token_usage\":{\"input_tokens\":2,\"cached_input_tokens\":2,\"output_tokens\":0,\"reasoning_output_tokens\":0}}}}\n");
    SetFileTime(first, context.now_ticks);
    SetFileTime(second, context.now_ticks);

    const auto result = ScanCodexToday({root}, context);
    Require(result.completed, "Codex scan should complete");
    Require(result.tokens == 190, "Codex must only use the final cumulative token_count");
    Require(result.valid_entries == 1, "Cached-only Codex session should match token-tracker skip behavior");
}

void TestCombinedSnapshot()
{
    TemporaryDirectory temporary;
    const auto context = TestContext();
    const auto timestamp = FormatUtcIso8601(context.now_ticks);
    const auto claude_root = temporary.Path() / L"claude" / L"projects";
    const auto codex_root = temporary.Path() / L"codex" / L"sessions";
    const auto claude_file = claude_root / L"project" / L"usage.jsonl";
    const auto codex_file = codex_root / L"session.jsonl";

    WriteText(claude_file,
        std::string("{\"type\":\"assistant\",\"timestamp\":\"") + timestamp +
        "\",\"requestId\":\"r\",\"message\":{\"id\":\"m\",\"usage\":{\"input_tokens\":11}}}\n");
    WriteText(codex_file,
        std::string("{\"type\":\"session_meta\",\"payload\":{\"id\":\"s\",\"timestamp\":\"") + timestamp +
        "\"}}\n" +
        "{\"type\":\"event_msg\",\"payload\":{\"type\":\"token_count\",\"info\":{\"total_token_usage\":{\"input_tokens\":22,\"cached_input_tokens\":0,\"output_tokens\":0,\"reasoning_output_tokens\":0}}}}\n");
    SetFileTime(claude_file, context.now_ticks);
    SetFileTime(codex_file, context.now_ticks);

    ProviderPaths paths{};
    paths.claude_roots.push_back(claude_root);
    paths.codex_session_roots.push_back(codex_root);
    UsageSnapshot snapshot{};
    Require(TokenScanner().Scan(paths, context, snapshot), "Combined scan should succeed");
    Require(snapshot.claude_tokens == 11, "Combined scan Claude value");
    Require(snapshot.codex_tokens == 22, "Combined scan Codex value");
    Require(snapshot.total_tokens == 33, "Combined scan total must be provider sum");
}
}

int main()
{
    try
    {
        TestEnvironmentDiscovery();
        TestClaude();
        TestCodex();
        TestCombinedSnapshot();
        std::cout << "All scanner tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
