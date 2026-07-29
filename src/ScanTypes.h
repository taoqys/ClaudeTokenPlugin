#pragma once

#include <cstddef>
#include <cstdint>

struct ProviderScanResult
{
    bool completed{true};
    std::uint64_t tokens{};
    std::size_t files_scanned{};
    std::size_t valid_entries{};
};

struct UsageSnapshot
{
    std::uint64_t claude_tokens{};
    std::uint64_t codex_tokens{};
    std::uint64_t total_tokens{};
};
