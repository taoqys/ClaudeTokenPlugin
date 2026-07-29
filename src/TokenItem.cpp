#include "TokenItem.h"

#include <utility>

TokenItem::TokenItem(const wchar_t* name, const wchar_t* id, const wchar_t* label,
                     const wchar_t* sample)
    : m_name(name), m_id(id), m_label(label), m_sample(sample)
{
}

const wchar_t* TokenItem::GetItemName() const
{
    return m_name;
}

const wchar_t* TokenItem::GetItemId() const
{
    return m_id;
}

const wchar_t* TokenItem::GetItemLableText() const
{
    return m_label;
}

const wchar_t* TokenItem::GetItemValueText() const
{
    return m_value.c_str();
}

const wchar_t* TokenItem::GetItemValueSampleText() const
{
    return m_sample;
}

void TokenItem::SetValue(std::wstring value)
{
    m_value = std::move(value);
}
