#pragma once

#include "PluginInterface.h"
#include "TokenItem.h"
#include "TokenScanner.h"

#include <array>
#include <cstdint>
#include <string>

class PluginMain final : public ITMPlugin
{
public:
    static PluginMain& Instance();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;

#ifdef CLAUDE_TOKEN_PLUGIN_TESTING
    void PublishSnapshotForTesting(const UsageSnapshot& snapshot);
#endif

private:
    PluginMain();

    void Publish(const UsageSnapshot& snapshot);

    static constexpr std::uint64_t kRefreshIntervalMilliseconds = 20'000ULL;

    TokenScanner m_scanner;
    std::array<TokenItem, 3> m_items;
    std::wstring m_tooltip{L"Claude: 0\nCodex: 0\nTotal: 0"};
    std::uint64_t m_last_refresh_milliseconds{};
};
