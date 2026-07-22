#include "ClaudeTokenItem.h"

static const wchar_t* ITEM_NAMES[] = {
    L"Claude Tokens",
    L"Claude Input",
    L"Claude Output",
    L"Claude Cache",
    L"Claude Messages",
};

static const wchar_t* ITEM_IDS[] = {
    L"ClaudeTotalTokens",
    L"ClaudeInputTokens",
    L"ClaudeOutputTokens",
    L"ClaudeCacheTokens",
    L"ClaudeMessages",
};

static const wchar_t* ITEM_LABELS[] = {
    L"Tokens",
    L"In",
    L"Out",
    L"Cache",
    L"Msgs",
};

static const wchar_t* ITEM_SAMPLES[] = {
    L"999.9M",
    L"999.9M",
    L"999.9M",
    L"999.9M",
    L"9999",
};

ClaudeTokenItem::ClaudeTokenItem(ItemType type)
    : m_type(type)
{
}

const wchar_t* ClaudeTokenItem::GetItemName() const
{
    return ITEM_NAMES[m_type];
}

const wchar_t* ClaudeTokenItem::GetItemId() const
{
    return ITEM_IDS[m_type];
}

const wchar_t* ClaudeTokenItem::GetItemLableText() const
{
    if (m_showLabel)
        return ITEM_LABELS[m_type];
    return L"";
}

const wchar_t* ClaudeTokenItem::GetItemValueText() const
{
    return m_valueText.c_str();
}

const wchar_t* ClaudeTokenItem::GetItemValueSampleText() const
{
    return ITEM_SAMPLES[m_type];
}

void ClaudeTokenItem::SetValue(long long value)
{
    m_value = value;
    UpdateValueText();
}

void ClaudeTokenItem::SetShowLabel(bool show)
{
    m_showLabel = show;
}

void ClaudeTokenItem::SetNumberFormat(int fmt)
{
    m_numberFormat = fmt;
    UpdateValueText();
}

void ClaudeTokenItem::UpdateValueText()
{
    if (m_numberFormat == 1)
        FormatRaw(m_value, m_valueText);
    else
        FormatShort(m_value, m_valueText);
}

const wchar_t* ClaudeTokenItem::FormatShort(long long n, std::wstring& buf)
{
    if (n >= 1000000000)
    {
        wchar_t tmp[32];
        swprintf_s(tmp, L"%.1fG", n / 1000000000.0);
        buf = tmp;
    }
    else if (n >= 1000000)
    {
        wchar_t tmp[32];
        swprintf_s(tmp, L"%.1fM", n / 1000000.0);
        buf = tmp;
    }
    else if (n >= 1000)
    {
        wchar_t tmp[32];
        swprintf_s(tmp, L"%.0fK", n / 1000.0);
        buf = tmp;
    }
    else
    {
        wchar_t tmp[32];
        swprintf_s(tmp, L"%lld", n);
        buf = tmp;
    }
    return buf.c_str();
}

const wchar_t* ClaudeTokenItem::FormatRaw(long long n, std::wstring& buf)
{
    // Format with comma separators: 1,234,567
    wchar_t tmp[64];
    swprintf_s(tmp, L"%lld", n);
    std::wstring raw(tmp);

    buf.clear();
    int count = 0;
    for (int i = (int)raw.size() - 1; i >= 0; i--)
    {
        if (count > 0 && count % 3 == 0)
            buf.insert(0, L",");
        buf.insert(0, 1, raw[i]);
        count++;
    }
    return buf.c_str();
}
