#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ProviderPaths
{
    std::vector<std::filesystem::path> claude_roots;
    std::vector<std::filesystem::path> codex_session_roots;
};

std::wstring GetEnvironmentValue(const wchar_t* name);
std::filesystem::path GetUserProfileDirectory();
ProviderPaths DiscoverDefaultPaths();
std::vector<std::filesystem::path> EnumerateJsonlFiles(const std::filesystem::path& root);
