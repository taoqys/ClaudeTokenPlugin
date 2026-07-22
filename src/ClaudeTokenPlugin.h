#pragma once
#include "PluginInterface.h"
#include "ClaudeTokenItem.h"
#include "Settings.h"

class ClaudeTokenPlugin : public ITMPlugin
{
public:
    static ClaudeTokenPlugin& Instance();

    // ITMPlugin
    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    void OnInitialize(ITrafficMonitor* pApp) override;

private:
    ClaudeTokenPlugin();
    ~ClaudeTokenPlugin() = default;
    ClaudeTokenPlugin(const ClaudeTokenPlugin&) = delete;
    ClaudeTokenPlugin& operator=(const ClaudeTokenPlugin&) = delete;

    void ScanAndUpdate();
    void ApplySettings();

    // All 5 possible display items
    ClaudeTokenItem m_items[ClaudeTokenItem::ITEM_COUNT] = {
        ClaudeTokenItem(ClaudeTokenItem::ITEM_TOTAL),
        ClaudeTokenItem(ClaudeTokenItem::ITEM_INPUT),
        ClaudeTokenItem(ClaudeTokenItem::ITEM_OUTPUT),
        ClaudeTokenItem(ClaudeTokenItem::ITEM_CACHE),
        ClaudeTokenItem(ClaudeTokenItem::ITEM_MESSAGES),
    };

    // Active item index (only this one is returned by GetItem)
    int m_activeItem = 0;

    ITrafficMonitor* m_pApp{};
    std::wstring m_tooltipInfo;
    std::wstring m_iniPath;
    std::wstring m_projectsDir;

    // Aggregated data
    long long m_totalInput{};
    long long m_totalOutput{};
    long long m_totalCacheRead{};
    long long m_totalCacheCreation{};
    int m_totalMessages{};

    PluginSettings m_settings;

    // Refresh interval tracking
    DWORD m_lastRefreshTime = 0;

    // Dialog callback
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
};
