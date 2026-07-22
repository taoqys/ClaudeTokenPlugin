#include "ClaudeTokenPlugin.h"
#include "JsonlScanner.h"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

// Dialog control IDs
#define IDC_COMBO_DISPLAY   1001
#define IDC_COMBO_FORMAT    1002
#define IDC_CHECK_LABEL     1003
#define IDC_CHECK_TOOLTIP   1004
#define IDC_EDIT_CACHEPATH  1005
#define IDC_EDIT_REFRESH    1006
#define IDC_CHECK_CACHEREAD 1007

// Extended dialog template structures
#pragma pack(push, 1)
struct DLGTEMPLATEEX {
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x;
    short y;
    short cx;
    short cy;
};

struct DLGITEMTEMPLATEEX {
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    short x;
    short y;
    short cx;
    short cy;
    DWORD id;
};
#pragma pack(pop)

static std::wstring GetUserProfilePath()
{
    wchar_t buf[MAX_PATH]{};
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);
    return L"";
}

ClaudeTokenPlugin& ClaudeTokenPlugin::Instance()
{
    static ClaudeTokenPlugin inst;
    return inst;
}

ClaudeTokenPlugin::ClaudeTokenPlugin()
{
}

IPluginItem* ClaudeTokenPlugin::GetItem(int index)
{
    if (index == 0)
        return &m_items[m_activeItem];
    return nullptr;
}

void ClaudeTokenPlugin::DataRequired()
{
    // Only re-read at the configured refresh interval
    DWORD now = GetTickCount();
    if (m_lastRefreshTime == 0 || (now - m_lastRefreshTime) >= (DWORD)(m_settings.refreshInterval * 1000))
    {
        ScanAndUpdate();
        m_lastRefreshTime = now;
    }
}

void ClaudeTokenPlugin::ApplySettings()
{
    m_activeItem = m_settings.displayItem;
    for (int i = 0; i < ClaudeTokenItem::ITEM_COUNT; i++)
    {
        m_items[i].SetShowLabel(m_settings.showLabel != 0);
        m_items[i].SetNumberFormat(m_settings.numberFormat);
    }
}

void ClaudeTokenPlugin::ScanAndUpdate()
{
    // Scan JSONL files directly
    auto daily = JsonlScanner::ScanAll(m_projectsDir);

    long long totalInput = 0, totalOutput = 0, totalCacheRead = 0, totalCacheCreation = 0;
    int totalMsgs = 0;

    for (const auto& [key, stats] : daily)
    {
        totalInput += stats.input;
        totalOutput += stats.output;
        totalCacheRead += stats.cacheRead;
        totalCacheCreation += stats.cacheCreation;
        totalMsgs += stats.count;
    }

    m_totalInput = totalInput;
    m_totalOutput = totalOutput;
    m_totalCacheRead = totalCacheRead;
    m_totalCacheCreation = totalCacheCreation;
    m_totalMessages = totalMsgs;

    // Total = input + output + cache_creation (cache_read excluded by default)
    long long grandTotal = totalInput + totalOutput + totalCacheCreation;
    if (m_settings.includeCacheRead)
        grandTotal += totalCacheRead;

    m_items[ClaudeTokenItem::ITEM_TOTAL].SetValue(grandTotal);
    m_items[ClaudeTokenItem::ITEM_INPUT].SetValue(totalInput);
    m_items[ClaudeTokenItem::ITEM_OUTPUT].SetValue(totalOutput);
    m_items[ClaudeTokenItem::ITEM_CACHE].SetValue(totalCacheRead);
    m_items[ClaudeTokenItem::ITEM_MESSAGES].SetValue(totalMsgs);

    // Build tooltip
    if (m_settings.showTooltipDetail)
    {
        wchar_t tooltip[512];
        swprintf_s(tooltip,
            L"Claude Token Usage\nTotal: %lld\nInput: %lld\nOutput: %lld\nCache Read: %lld\nCache Write: %lld\nMessages: %d",
            grandTotal, totalInput, totalOutput, totalCacheRead, totalCacheCreation, totalMsgs);
        m_tooltipInfo = tooltip;
    }
    else
    {
        wchar_t tooltip[64];
        swprintf_s(tooltip, L"Claude Tokens: %lld", grandTotal);
        m_tooltipInfo = tooltip;
    }
}

// Options dialog
INT_PTR CALLBACK ClaudeTokenPlugin::DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static PluginSettings* pSettings = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        pSettings = reinterpret_cast<PluginSettings*>(lParam);

        HWND hComboDisplay = GetDlgItem(hDlg, IDC_COMBO_DISPLAY);
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Total Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Input Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Output Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Cache Read Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Message Count");
        SendMessageW(hComboDisplay, CB_SETCURSEL, pSettings->displayItem, 0);

        HWND hComboFormat = GetDlgItem(hDlg, IDC_COMBO_FORMAT);
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LRESULT)L"Short (1.2M)");
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LRESULT)L"Raw (1,234,567)");
        SendMessageW(hComboFormat, CB_SETCURSEL, pSettings->numberFormat, 0);

        CheckDlgButton(hDlg, IDC_CHECK_LABEL, pSettings->showLabel ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHECK_TOOLTIP, pSettings->showTooltipDetail ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHECK_CACHEREAD, pSettings->includeCacheRead ? BST_CHECKED : BST_UNCHECKED);

        SetDlgItemTextW(hDlg, IDC_EDIT_CACHEPATH, pSettings->cachePath);

        wchar_t refreshBuf[16];
        swprintf_s(refreshBuf, L"%d", pSettings->refreshInterval);
        SetDlgItemTextW(hDlg, IDC_EDIT_REFRESH, refreshBuf);

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            pSettings->displayItem = (int)SendMessageW(GetDlgItem(hDlg, IDC_COMBO_DISPLAY), CB_GETCURSEL, 0, 0);
            pSettings->numberFormat = (int)SendMessageW(GetDlgItem(hDlg, IDC_COMBO_FORMAT), CB_GETCURSEL, 0, 0);
            pSettings->showLabel = IsDlgButtonChecked(hDlg, IDC_CHECK_LABEL) == BST_CHECKED ? 1 : 0;
            pSettings->showTooltipDetail = IsDlgButtonChecked(hDlg, IDC_CHECK_TOOLTIP) == BST_CHECKED ? 1 : 0;
            pSettings->includeCacheRead = IsDlgButtonChecked(hDlg, IDC_CHECK_CACHEREAD) == BST_CHECKED ? 1 : 0;
            GetDlgItemTextW(hDlg, IDC_EDIT_CACHEPATH, pSettings->cachePath, MAX_PATH);

            wchar_t refreshBuf[16] = {};
            GetDlgItemTextW(hDlg, IDC_EDIT_REFRESH, refreshBuf, 16);
            int interval = _wtoi(refreshBuf);
            if (interval < 1) interval = 1;
            if (interval > 300) interval = 300;
            pSettings->refreshInterval = interval;

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

ITMPlugin::OptionReturn ClaudeTokenPlugin::ShowOptionsDialog(void* hParent)
{
    BYTE buffer[2048];
    ZeroMemory(buffer, sizeof(buffer));

    DLGTEMPLATEEX* pDlg = (DLGTEMPLATEEX*)buffer;
    pDlg->dlgVer = 1;
    pDlg->signature = 0xFFFF;
    pDlg->helpID = 0;
    pDlg->exStyle = 0;
    pDlg->style = DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    pDlg->cDlgItems = 11;
    pDlg->x = 0;
    pDlg->y = 0;
    pDlg->cx = 260;
    pDlg->cy = 220;

    BYTE* p = buffer + sizeof(DLGTEMPLATEEX);
    *(WORD*)p = 0; p += 2;
    *(WORD*)p = 0; p += 2;
    *(WORD*)p = 0; p += 2;
    *(WORD*)p = 8; p += 2;
    memcpy(p, L"MS Shell Dlg", 24); p += 24;

    auto addControl = [&](DWORD style, DWORD exStyle, short x, short y, short cx, short cy,
                          WORD id, WORD classAtom, const wchar_t* text) {
        while ((UINT_PTR)p % 4) p++;
        DLGITEMTEMPLATEEX* pItem = (DLGITEMTEMPLATEEX*)p;
        pItem->helpID = 0;
        pItem->exStyle = exStyle;
        pItem->style = style | WS_CHILD | WS_VISIBLE;
        pItem->x = x; pItem->y = y;
        pItem->cx = cx; pItem->cy = cy;
        pItem->id = id;
        p += sizeof(DLGITEMTEMPLATEEX);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = classAtom; p += 2;
        size_t len = wcslen(text) + 1;
        memcpy(p, text, len * 2); p += len * 2;
        *(WORD*)p = 0; p += 2;
    };

    addControl(SS_LEFT, 0, 7, 7, 60, 12, 0xFFFF, 0x0082, L"Display Item:");
    addControl(SS_LEFT, 0, 7, 27, 60, 12, 0xFFFF, 0x0082, L"Number Format:");
    addControl(SS_LEFT, 0, 7, 90, 60, 12, 0xFFFF, 0x0082, L"Cache Path:");
    addControl(SS_LEFT, 0, 7, 110, 60, 12, 0xFFFF, 0x0082, L"Refresh (s):");

    addControl(CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL, 0,
               75, 5, 175, 100, IDC_COMBO_DISPLAY, 0x0085, L"");
    addControl(CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL, 0,
               75, 25, 175, 100, IDC_COMBO_FORMAT, 0x0085, L"");

    addControl(BS_AUTOCHECKBOX, 0, 7, 50, 120, 12, IDC_CHECK_LABEL, 0x0080, L"Show label text");
    addControl(BS_AUTOCHECKBOX, 0, 130, 50, 120, 12, IDC_CHECK_TOOLTIP, 0x0080, L"Show detailed tooltip");
    addControl(BS_AUTOCHECKBOX, 0, 7, 68, 180, 12, IDC_CHECK_CACHEREAD, 0x0080, L"Include cache_read in total");

    addControl(ES_AUTOHSCROLL | WS_BORDER, 0,
               75, 88, 175, 14, IDC_EDIT_CACHEPATH, 0x0081, L"");
    addControl(ES_AUTOHSCROLL | WS_BORDER | ES_NUMBER, 0,
               75, 108, 50, 14, IDC_EDIT_REFRESH, 0x0081, L"");

    addControl(BS_DEFPUSHBUTTON, 0, 140, 135, 50, 14, IDOK, 0x0080, L"OK");
    addControl(0, 0, 195, 135, 50, 14, IDCANCEL, 0x0080, L"Cancel");

    PluginSettings tempSettings = m_settings;
    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE*)buffer,
        (HWND)hParent,
        DialogProc,
        (LPARAM)&tempSettings
    );

    if (result == IDOK)
    {
        bool changed = (tempSettings.displayItem != m_settings.displayItem ||
                        tempSettings.numberFormat != m_settings.numberFormat ||
                        tempSettings.showLabel != m_settings.showLabel ||
                        tempSettings.showTooltipDetail != m_settings.showTooltipDetail ||
                        tempSettings.refreshInterval != m_settings.refreshInterval ||
                        tempSettings.includeCacheRead != m_settings.includeCacheRead ||
                        wcscmp(tempSettings.cachePath, m_settings.cachePath) != 0);

        m_settings = tempSettings;
        m_settings.Save(m_iniPath);
        ApplySettings();

        // Force immediate rescan with new settings
        m_lastRefreshTime = 0;

        return changed ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
    }

    return OR_OPTION_UNCHANGED;
}

const wchar_t* ClaudeTokenPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"Claude Token Monitor";
    case TMI_DESCRIPTION: return L"Displays Claude Code daily token usage on the taskbar";
    case TMI_AUTHOR:      return L"Claude";
    case TMI_COPYRIGHT:   return L"MIT License";
    case TMI_VERSION:     return L"2.0.0";
    case TMI_URL:         return L"https://github.com/taoqys/ClaudeTokenPlugin";
    default:              return L"";
    }
}

const wchar_t* ClaudeTokenPlugin::GetTooltipInfo()
{
    return m_tooltipInfo.c_str();
}

void ClaudeTokenPlugin::OnInitialize(ITrafficMonitor* pApp)
{
    m_pApp = pApp;

    // Set up projects directory
    m_projectsDir = GetUserProfilePath() + L"\\.claude\\projects";

    // Get config directory and load settings
    if (pApp)
    {
        const wchar_t* configDir = pApp->GetPluginConfigDir();
        if (configDir && configDir[0])
        {
            m_iniPath = std::wstring(configDir) + L"ClaudeTokenPlugin.ini";
        }
    }

    if (m_iniPath.empty())
    {
        m_iniPath = GetUserProfilePath() + L"\\.claude\\ClaudeTokenPlugin.ini";
    }

    m_settings.Load(m_iniPath);
    ApplySettings();

    // Initial scan
    ScanAndUpdate();
}

// Export entry point
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &ClaudeTokenPlugin::Instance();
}
