#include "ClaudeTokenItem.h"

static const wchar_t* ITEM_NAMES[] = {
    L"Claude Tokens",
};

static const wchar_t* ITEM_IDS[] = {
    L"ClaudeTotalTokens",
};

static const wchar_t* ITEM_LABELS[] = {
    L"Tokens",
};

static const wchar_t* ITEM_SAMPLES[] = {
    L"999.9M",
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
    return ITEM_LABELS[m_type];
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
    FormatTokens(value, m_valueText);
}

const wchar_t* ClaudeTokenItem::FormatTokens(long long n, std::wstring& buf)
{
    if (n >= 1000000)
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
