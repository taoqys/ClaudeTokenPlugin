#pragma once
#include "PluginInterface.h"
#include "ClaudeTokenItem.h"

class ClaudeTokenPlugin : public ITMPlugin
{
public:
    static ClaudeTokenPlugin& Instance();

    // ITMPlugin
    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    void OnInitialize(ITrafficMonitor* pApp) override;

private:
    ClaudeTokenPlugin();
    ~ClaudeTokenPlugin() = default;
    ClaudeTokenPlugin(const ClaudeTokenPlugin&) = delete;
    ClaudeTokenPlugin& operator=(const ClaudeTokenPlugin&) = delete;

    void LoadCache();

    ClaudeTokenItem m_items[1] = {
        ClaudeTokenItem(ClaudeTokenItem::ITEM_TOTAL),
    };

    ITrafficMonitor* m_pApp{};
    std::wstring m_tooltipInfo;
};
