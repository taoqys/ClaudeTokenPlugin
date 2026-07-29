#include "PluginMain.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void RequireText(const wchar_t* actual, const wchar_t* expected, const char* message)
{
    Require(actual != nullptr && std::wstring(actual) == expected, message);
}

void TestItems()
{
    auto& plugin = PluginMain::Instance();
    IPluginItem* claude = plugin.GetItem(0);
    IPluginItem* codex = plugin.GetItem(1);
    IPluginItem* total = plugin.GetItem(2);

    Require(claude != nullptr && codex != nullptr && total != nullptr, "Plugin must expose three items");
    Require(plugin.GetItem(-1) == nullptr, "Negative item index must return null");
    Require(plugin.GetItem(3) == nullptr, "Out-of-range item index must return null");

    RequireText(claude->GetItemId(), L"ClaudeTodayTokens", "Claude item ID mismatch");
    RequireText(codex->GetItemId(), L"CodexTodayTokens", "Codex item ID mismatch");
    RequireText(total->GetItemId(), L"TotalTodayTokens", "Total item ID mismatch");
    RequireText(claude->GetItemLableText(), L"Claude", "Claude label mismatch");
    RequireText(codex->GetItemLableText(), L"Codex", "Codex label mismatch");
    RequireText(total->GetItemLableText(), L"Total", "Total label mismatch");
    RequireText(claude->GetItemValueText(), L"0", "Initial Claude value must be zero");
    RequireText(codex->GetItemValueText(), L"0", "Initial Codex value must be zero");
    RequireText(total->GetItemValueText(), L"0", "Initial total value must be zero");
}

void TestPublishedSnapshot()
{
    auto& plugin = PluginMain::Instance();
    const UsageSnapshot snapshot{11, 22, 33};
    plugin.PublishSnapshotForTesting(snapshot);

    RequireText(plugin.GetItem(0)->GetItemValueText(), L"11", "Published Claude value mismatch");
    RequireText(plugin.GetItem(1)->GetItemValueText(), L"22", "Published Codex value mismatch");
    RequireText(plugin.GetItem(2)->GetItemValueText(), L"33", "Published total value mismatch");
    RequireText(plugin.GetTooltipInfo(), L"Claude: 11\nCodex: 22\nTotal: 33", "Tooltip must use cached snapshot");
}

void TestPluginInformation()
{
    auto& plugin = PluginMain::Instance();
    Require(plugin.GetAPIVersion() == 8, "Plugin must retain official API version");
    RequireText(plugin.GetInfo(ITMPlugin::TMI_NAME), L"AI CLI Token Tracker", "Plugin name mismatch");
    RequireText(plugin.GetInfo(ITMPlugin::TMI_VERSION), L"2.0.0", "Plugin version mismatch");
}
}

int main()
{
    try
    {
        TestItems();
        TestPublishedSnapshot();
        TestPluginInformation();
        std::cout << "All plugin ABI tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
