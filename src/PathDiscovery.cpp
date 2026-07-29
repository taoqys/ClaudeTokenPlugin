#include "PathDiscovery.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cwctype>
#include <limits>
#include <set>

namespace
{
std::wstring Trim(std::wstring value)
{
    const auto not_space = [](wchar_t character) { return !iswspace(character); };
    const auto first = std::find_if(value.begin(), value.end(), not_space);
    const auto last = std::find_if(value.rbegin(), value.rend(), not_space).base();
    if (first >= last)
        return L"";
    return std::wstring(first, last);
}

std::wstring CanonicalKey(const std::filesystem::path& path)
{
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    if (error)
        absolute = path;
    auto canonical = std::filesystem::weakly_canonical(absolute, error);
    if (!error)
        absolute = canonical;

    auto key = absolute.lexically_normal().wstring();
    std::transform(key.begin(), key.end(), key.begin(), towlower);
    return key;
}

void AddUniquePath(std::vector<std::filesystem::path>& paths,
                   std::set<std::wstring>& seen,
                   const std::filesystem::path& candidate)
{
    if (candidate.empty())
        return;
    const auto key = CanonicalKey(candidate);
    if (seen.insert(key).second)
        paths.push_back(candidate);
}
}

std::wstring GetEnvironmentValue(const wchar_t* name)
{
    DWORD capacity = GetEnvironmentVariableW(name, nullptr, 0);
    if (capacity == 0)
        return L"";

    for (;;)
    {
        std::wstring result(capacity, L'\0');
        const DWORD written = GetEnvironmentVariableW(name, result.data(), capacity);
        if (written == 0)
            return L"";
        if (written < capacity)
        {
            result.resize(written);
            return result;
        }
        if (written >= std::numeric_limits<DWORD>::max() - 1)
            return L"";
        capacity = written + 1;
    }
}

std::filesystem::path GetUserProfileDirectory()
{
    PWSTR value = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &value)) && value)
    {
        const std::filesystem::path profile(value);
        CoTaskMemFree(value);
        return profile;
    }

    const auto environment_profile = GetEnvironmentValue(L"USERPROFILE");
    return environment_profile.empty() ? std::filesystem::path{} : std::filesystem::path(environment_profile);
}

ProviderPaths DiscoverDefaultPaths()
{
    ProviderPaths result;
    std::set<std::wstring> claude_seen;
    std::set<std::wstring> codex_seen;

    const auto claude_config = GetEnvironmentValue(L"CLAUDE_CONFIG_DIR");
    std::size_t position = 0;
    while (position <= claude_config.size())
    {
        const auto delimiter = claude_config.find(L',', position);
        const auto length = delimiter == std::wstring::npos ? std::wstring::npos : delimiter - position;
        const auto config_root = Trim(claude_config.substr(position, length));
        if (!config_root.empty())
            AddUniquePath(result.claude_roots, claude_seen, std::filesystem::path(config_root) / L"projects");
        if (delimiter == std::wstring::npos)
            break;
        position = delimiter + 1;
    }

    const auto profile = GetUserProfileDirectory();
    if (!profile.empty())
    {
        AddUniquePath(result.claude_roots, claude_seen, profile / L".claude" / L"projects");
        AddUniquePath(result.claude_roots, claude_seen, profile / L".config" / L"claude" / L"projects");
    }

    const auto codex_home = Trim(GetEnvironmentValue(L"CODEX_HOME"));
    if (!codex_home.empty())
        AddUniquePath(result.codex_session_roots, codex_seen, std::filesystem::path(codex_home) / L"sessions");
    else if (!profile.empty())
        AddUniquePath(result.codex_session_roots, codex_seen, profile / L".codex" / L"sessions");

    return result;
}

std::vector<std::filesystem::path> EnumerateJsonlFiles(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> result;
    std::error_code error;
    if (root.empty() || !std::filesystem::is_directory(root, error))
        return result;

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::recursive_directory_iterator iterator(root, options, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end)
    {
        const auto& entry = *iterator;
        std::error_code status_error;
        if (entry.is_regular_file(status_error) && !status_error)
        {
            const auto extension = entry.path().extension().wstring();
            std::wstring lower_extension = extension;
            std::transform(lower_extension.begin(), lower_extension.end(), lower_extension.begin(), towlower);
            if (lower_extension == L".jsonl")
                result.push_back(entry.path());
        }
        iterator.increment(error);
        if (error)
            error.clear();
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
