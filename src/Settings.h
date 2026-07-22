#pragma once
#include <windows.h>
#include <string>

struct PluginSettings
{
    int displayItem = 0;        // 0=Total, 1=Input, 2=Output, 3=Cache, 4=Messages
    int numberFormat = 0;       // 0=Short (1.2M), 1=Raw (1234567)
    int showLabel = 1;          // 0=hide, 1=show
    int showTooltipDetail = 1;  // 0=hide, 1=show
    int refreshInterval = 10;   // seconds between cache re-reads
    int includeCacheRead = 0;   // 0=exclude cache_read from total, 1=include
    wchar_t cachePath[MAX_PATH] = L"";  // Empty = default path

    void Load(const std::wstring& iniPath)
    {
        displayItem = GetPrivateProfileIntW(L"Settings", L"DisplayItem", 0, iniPath.c_str());
        numberFormat = GetPrivateProfileIntW(L"Settings", L"NumberFormat", 0, iniPath.c_str());
        showLabel = GetPrivateProfileIntW(L"Settings", L"ShowLabel", 1, iniPath.c_str());
        showTooltipDetail = GetPrivateProfileIntW(L"Settings", L"ShowTooltipDetail", 1, iniPath.c_str());
        refreshInterval = GetPrivateProfileIntW(L"Settings", L"RefreshInterval", 10, iniPath.c_str());
        if (refreshInterval < 1) refreshInterval = 1;
        if (refreshInterval > 300) refreshInterval = 300;
        includeCacheRead = GetPrivateProfileIntW(L"Settings", L"IncludeCacheRead", 0, iniPath.c_str());
        GetPrivateProfileStringW(L"Settings", L"CachePath", L"", cachePath, MAX_PATH, iniPath.c_str());
    }

    void Save(const std::wstring& iniPath)
    {
        wchar_t buf[16];
        swprintf_s(buf, L"%d", displayItem);
        WritePrivateProfileStringW(L"Settings", L"DisplayItem", buf, iniPath.c_str());
        swprintf_s(buf, L"%d", numberFormat);
        WritePrivateProfileStringW(L"Settings", L"NumberFormat", buf, iniPath.c_str());
        swprintf_s(buf, L"%d", showLabel);
        WritePrivateProfileStringW(L"Settings", L"ShowLabel", buf, iniPath.c_str());
        swprintf_s(buf, L"%d", showTooltipDetail);
        WritePrivateProfileStringW(L"Settings", L"ShowTooltipDetail", buf, iniPath.c_str());
        swprintf_s(buf, L"%d", refreshInterval);
        WritePrivateProfileStringW(L"Settings", L"RefreshInterval", buf, iniPath.c_str());
        swprintf_s(buf, L"%d", includeCacheRead);
        WritePrivateProfileStringW(L"Settings", L"IncludeCacheRead", buf, iniPath.c_str());
        WritePrivateProfileStringW(L"Settings", L"CachePath", cachePath, iniPath.c_str());
    }
};
