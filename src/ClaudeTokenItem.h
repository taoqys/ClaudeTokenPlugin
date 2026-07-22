#pragma once
#include "PluginInterface.h"
#include <string>

class ClaudeTokenItem : public IPluginItem
{
public:
    enum ItemType
    {
        ITEM_TOTAL,     // Today's total tokens
        ITEM_COUNT = 1
    };

    explicit ClaudeTokenItem(ItemType type);

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    int IsDoubleLineExclusive() const override { return 1; }

    // Called by plugin to update data
    void SetValue(long long value);

private:
    ItemType m_type;
    long long m_value{};
    std::wstring m_valueText;
    std::wstring m_name;
    std::wstring m_id;
    std::wstring m_label;

    static const wchar_t* FormatTokens(long long n, std::wstring& buf);
};
