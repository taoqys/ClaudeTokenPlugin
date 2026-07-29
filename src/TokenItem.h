#pragma once

#include "PluginInterface.h"

#include <string>

class TokenItem final : public IPluginItem
{
public:
    TokenItem(const wchar_t* name, const wchar_t* id, const wchar_t* label,
              const wchar_t* sample);

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;

    void SetValue(std::wstring value);

private:
    const wchar_t* m_name;
    const wchar_t* m_id;
    const wchar_t* m_label;
    const wchar_t* m_sample;
    std::wstring m_value{L"0"};
};
