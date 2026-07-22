#pragma once
#include "PluginInterface.h"
#include <string>

class ClaudeTokenItem : public IPluginItem
{
public:
    enum ItemType
    {
        ITEM_TOTAL,     // Today's total tokens
        ITEM_INPUT,     // Today's input tokens
        ITEM_OUTPUT,    // Today's output tokens
        ITEM_CACHE,     // Today's cache read tokens
        ITEM_MESSAGES,  // Today's message count
        ITEM_COUNT
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
    void SetShowLabel(bool show);
    void SetNumberFormat(int fmt);  // 0=short, 1=raw

private:
    ItemType m_type;
    long long m_value{};
    std::wstring m_valueText;
    bool m_showLabel = true;
    int m_numberFormat = 0;  // 0=short, 1=raw

    void UpdateValueText();
    static const wchar_t* FormatShort(long long n, std::wstring& buf);
    static const wchar_t* FormatRaw(long long n, std::wstring& buf);
};
