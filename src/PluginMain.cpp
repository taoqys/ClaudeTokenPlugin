#include "PluginMain.h"

#include <windows.h>

#include <string>

namespace
{
std::uint64_t MonotonicMilliseconds()
{
    return GetTickCount64();
}

std::wstring ToText(std::uint64_t value)
{
    return std::to_wstring(value);
}
}

PluginMain& PluginMain::Instance()
{
    static PluginMain instance;
    return instance;
}

PluginMain::PluginMain()
    : m_items{
        TokenItem(L"Claude Code Today Tokens", L"ClaudeTodayTokens", L"Claude", L"18446744073709551615"),
        TokenItem(L"Codex Today Tokens", L"CodexTodayTokens", L"Codex", L"18446744073709551615"),
        TokenItem(L"AI CLI Today Tokens", L"TotalTodayTokens", L"Total", L"18446744073709551615"),
    }
{
}

IPluginItem* PluginMain::GetItem(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size()))
        return nullptr;
    return &m_items[static_cast<std::size_t>(index)];
}

void PluginMain::DataRequired()
{
    const auto now = MonotonicMilliseconds();
    if (m_last_refresh_milliseconds != 0 &&
        now - m_last_refresh_milliseconds < kRefreshIntervalMilliseconds)
    {
        return;
    }

    m_last_refresh_milliseconds = now;
    try
    {
        UsageSnapshot snapshot{};
        if (m_scanner.ScanDefault(snapshot))
            Publish(snapshot);
    }
    catch (...)
    {
        // Keep the most recent complete snapshot. Exceptions must not cross the host ABI.
    }
}

const wchar_t* PluginMain::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"AI CLI Token Tracker";
    case TMI_DESCRIPTION:
        return L"Displays Claude Code, Codex, and combined current-day token totals.";
    case TMI_AUTHOR:
        return L"ClaudeTokenPlugin";
    case TMI_COPYRIGHT:
        return L"MIT License";
    case TMI_VERSION:
        return L"2.0.0";
    case TMI_URL:
        return L"https://github.com/stormzhang/token-tracker";
    default:
        return L"";
    }
}

const wchar_t* PluginMain::GetTooltipInfo()
{
    return m_tooltip.c_str();
}

#ifdef CLAUDE_TOKEN_PLUGIN_TESTING
void PluginMain::PublishSnapshotForTesting(const UsageSnapshot& snapshot)
{
    Publish(snapshot);
}
#endif

void PluginMain::Publish(const UsageSnapshot& snapshot)
{
    m_items[0].SetValue(ToText(snapshot.claude_tokens));
    m_items[1].SetValue(ToText(snapshot.codex_tokens));
    m_items[2].SetValue(ToText(snapshot.total_tokens));

    m_tooltip = L"Claude: " + ToText(snapshot.claude_tokens) +
                L"\nCodex: " + ToText(snapshot.codex_tokens) +
                L"\nTotal: " + ToText(snapshot.total_tokens);
}

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &PluginMain::Instance();
}
