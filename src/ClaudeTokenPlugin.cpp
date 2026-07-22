#include "ClaudeTokenPlugin.h"
#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "comctl32.lib")

// Dialog control IDs
#define IDC_COMBO_DISPLAY   1001
#define IDC_COMBO_FORMAT    1002
#define IDC_CHECK_LABEL     1003
#define IDC_CHECK_TOOLTIP   1004
#define IDC_EDIT_CACHEPATH  1005
#define IDC_EDIT_REFRESH    1006

// Extended dialog template structures (not in standard headers)
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

// Minimal JSON value extraction for the cache file format.
static long long ExtractIntValue(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) return 0;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    size_t end = pos;
    if (end < json.size() && json[end] == '-') end++;
    while (end < json.size() && json[end] >= '0' && json[end] <= '9') end++;
    if (end == pos) return 0;
    return _strtoi64(json.substr(pos, end - pos).c_str(), nullptr, 10);
}

static std::string ReadFileContent(const std::wstring& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        char buf[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, buf, MAX_PATH, nullptr, nullptr);
        file.open(buf, std::ios::binary);
    }
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static std::string GetTodayDate()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_s(&t, &now);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return buf;
}

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
    // Only re-read cache at the configured refresh interval
    DWORD now = GetTickCount();
    if (m_lastRefreshTime == 0 || (now - m_lastRefreshTime) >= (DWORD)(m_settings.refreshInterval * 1000))
    {
        LoadCache();
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

void ClaudeTokenPlugin::LoadCache()
{
    // Determine cache path
    std::wstring cachePath;
    if (m_settings.cachePath[0] != L'\0')
    {
        cachePath = m_settings.cachePath;
    }
    else
    {
        cachePath = GetUserProfilePath() + L"\\.claude\\.token-cache.json";
    }

    std::string content = ReadFileContent(cachePath);
    if (content.empty()) return;

    std::string today = GetTodayDate();

    long long totalInput = 0, totalOutput = 0, totalCache = 0, totalMsgs = 0;

    size_t dailyPos = content.find("\"daily\"");
    if (dailyPos == std::string::npos) return;

    size_t objStart = content.find('{', dailyPos);
    if (objStart == std::string::npos) return;

    size_t pos = objStart + 1;
    int depth = 1;
    while (pos < content.size() && depth > 0)
    {
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n' || content[pos] == '\r' || content[pos] == '\t')) pos++;
        if (pos >= content.size()) break;

        if (content[pos] == '{') { depth++; pos++; continue; }
        if (content[pos] == '}') { depth--; pos++; continue; }
        if (content[pos] == ',') { pos++; continue; }
        if (content[pos] == ']') { depth--; pos++; continue; }

        if (content[pos] != '"') { pos++; continue; }

        pos++;
        size_t keyEnd = content.find('"', pos);
        if (keyEnd == std::string::npos) break;
        std::string key = content.substr(pos, keyEnd - pos);
        pos = keyEnd + 1;

        if (key.size() > today.size() && key.substr(0, today.size()) == today && key[today.size()] == '|')
        {
            size_t valStart = content.find('{', pos);
            if (valStart == std::string::npos || valStart - pos > 10) { pos = keyEnd + 1; continue; }
            size_t valEnd = content.find('}', valStart);
            if (valEnd == std::string::npos) break;

            std::string valObj = content.substr(valStart, valEnd - valStart + 1);
            totalInput += ExtractIntValue(valObj, "input");
            totalOutput += ExtractIntValue(valObj, "output");
            totalCache += ExtractIntValue(valObj, "cache_read");
            totalMsgs += ExtractIntValue(valObj, "count");

            pos = valEnd + 1;
        }
        else
        {
            size_t valStart = content.find(':', pos);
            if (valStart == std::string::npos) break;
            valStart++;
            while (valStart < content.size() && content[valStart] == ' ') valStart++;
            if (valStart >= content.size()) break;

            if (content[valStart] == '{')
            {
                int d = 1;
                valStart++;
                while (valStart < content.size() && d > 0)
                {
                    if (content[valStart] == '{') d++;
                    else if (content[valStart] == '}') d--;
                    valStart++;
                }
                pos = valStart;
            }
            else if (content[valStart] == '"')
            {
                valStart++;
                pos = content.find('"', valStart);
                if (pos == std::string::npos) break;
                pos++;
            }
            else
            {
                pos = content.find_first_of(",}", valStart);
                if (pos == std::string::npos) break;
                pos++;
            }
        }
    }

    m_rawInput = totalInput;
    m_rawOutput = totalOutput;
    m_rawCache = totalCache;
    m_rawMessages = totalMsgs;

    long long grandTotal = totalInput + totalOutput + totalCache;

    m_items[ClaudeTokenItem::ITEM_TOTAL].SetValue(grandTotal);
    m_items[ClaudeTokenItem::ITEM_INPUT].SetValue(totalInput);
    m_items[ClaudeTokenItem::ITEM_OUTPUT].SetValue(totalOutput);
    m_items[ClaudeTokenItem::ITEM_CACHE].SetValue(totalCache);
    m_items[ClaudeTokenItem::ITEM_MESSAGES].SetValue(totalMsgs);

    // Build tooltip
    if (m_settings.showTooltipDetail)
    {
        wchar_t tooltip[512];
        swprintf_s(tooltip,
            L"Claude Token Usage\nTotal: %lld\nInput: %lld\nOutput: %lld\nCache Read: %lld\nMessages: %lld",
            grandTotal, totalInput, totalOutput, totalCache, totalMsgs);
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

        // Populate display item combo
        HWND hComboDisplay = GetDlgItem(hDlg, IDC_COMBO_DISPLAY);
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Total Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Input Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Output Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Cache Read Tokens");
        SendMessageW(hComboDisplay, CB_ADDSTRING, 0, (LRESULT)L"Message Count");
        SendMessageW(hComboDisplay, CB_SETCURSEL, pSettings->displayItem, 0);

        // Populate format combo
        HWND hComboFormat = GetDlgItem(hDlg, IDC_COMBO_FORMAT);
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LRESULT)L"Short (1.2M)");
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LRESULT)L"Raw (1,234,567)");
        SendMessageW(hComboFormat, CB_SETCURSEL, pSettings->numberFormat, 0);

        // Checkboxes
        CheckDlgButton(hDlg, IDC_CHECK_LABEL, pSettings->showLabel ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHECK_TOOLTIP, pSettings->showTooltipDetail ? BST_CHECKED : BST_UNCHECKED);

        // Cache path
        SetDlgItemTextW(hDlg, IDC_EDIT_CACHEPATH, pSettings->cachePath);

        // Refresh interval
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
            GetDlgItemTextW(hDlg, IDC_EDIT_CACHEPATH, pSettings->cachePath, MAX_PATH);

            // Read refresh interval
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
    // Build in-memory dialog template
    // Using a simple template with: 2 combos, 2 checks, 1 edit, OK/Cancel buttons
    struct DialogTemplateBuilder
    {
        // Helper to build DLGTEMPLATEEX-like structure
        // For simplicity, use a pre-built template
    };

    // Dialog resource (in-memory)
    // DLGTEMPLATEEX format
    WORD dlgTemplate[] = {
        0xFFFF, 0x0001,  // dlgVer = 1, signature = 0xFFFF
    };

    // Use DialogBoxIndirectParamW with a manually constructed template
    // Simpler approach: use CreateDialogIndirect or a static template

    // Actually, let's use a simpler approach with a static dialog template
    // defined as a struct

    // Simpler: just build the template as bytes
    struct {
        DLGTEMPLATE dt;
        WORD menu;
        WORD windowClass;
        wchar_t title[1];
    } template_ = {};

    template_.dt.style = DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    template_.dt.cdit = 0;
    template_.dt.x = 0;
    template_.dt.y = 0;
    template_.dt.cx = 250;
    template_.dt.cy = 180;
    template_.menu = 0;
    template_.windowClass = 0;
    template_.title[0] = L'\0';

    // We need a more complex template with controls.
    // Use a byte buffer approach.
    BYTE buffer[2048];
    ZeroMemory(buffer, sizeof(buffer));

    // Header
    DLGTEMPLATEEX* pDlg = (DLGTEMPLATEEX*)buffer;
    pDlg->dlgVer = 1;
    pDlg->signature = 0xFFFF;
    pDlg->helpID = 0;
    pDlg->exStyle = 0;
    pDlg->style = DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    pDlg->cDlgItems = 7;  // 2 combos + 2 checks + 1 edit + 2 buttons
    pDlg->x = 0;
    pDlg->y = 0;
    pDlg->cx = 260;
    pDlg->cy = 200;

    // Point to end of header for variable-length data
    BYTE* p = buffer + sizeof(DLGTEMPLATEEX);

    // menu: none
    *(WORD*)p = 0; p += 2;
    // window class: none
    *(WORD*)p = 0; p += 2;
    // title: empty
    *(WORD*)p = 0; p += 2;
    // font size
    *(WORD*)p = 8; p += 2;
    // font name: "MS Shell Dlg"
    memcpy(p, L"MS Shell Dlg", 24); p += 24;

    // Helper lambda to add a control
    auto addControl = [&](DWORD style, DWORD exStyle, short x, short y, short cx, short cy,
                          WORD id, WORD classAtom, const wchar_t* text) {
        // Align to DWORD
        while ((UINT_PTR)p % 4) p++;

        DLGITEMTEMPLATEEX* pItem = (DLGITEMTEMPLATEEX*)p;
        pItem->helpID = 0;
        pItem->exStyle = exStyle;
        pItem->style = style | WS_CHILD | WS_VISIBLE;
        pItem->x = x;
        pItem->y = y;
        pItem->cx = cx;
        pItem->cy = cy;
        pItem->id = id;
        p += sizeof(DLGITEMTEMPLATEEX);

        // window class atom
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = classAtom; p += 2;

        // title
        size_t len = wcslen(text) + 1;
        memcpy(p, text, len * 2); p += len * 2;

        // creation data
        *(WORD*)p = 0; p += 2;
    };

    // Labels
    addControl(SS_LEFT, 0, 7, 7, 60, 12, 0xFFFF, 0x0082, L"Display Item:");
    addControl(SS_LEFT, 0, 7, 27, 60, 12, 0xFFFF, 0x0082, L"Number Format:");
    addControl(SS_LEFT, 0, 7, 90, 60, 12, 0xFFFF, 0x0082, L"Cache Path:");
    addControl(SS_LEFT, 0, 7, 110, 60, 12, 0xFFFF, 0x0082, L"Refresh (s):");

    // ComboBox: Display Item
    addControl(CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL, 0,
               75, 5, 175, 100, IDC_COMBO_DISPLAY, 0x0085, L"");

    // ComboBox: Number Format
    addControl(CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL, 0,
               75, 25, 175, 100, IDC_COMBO_FORMAT, 0x0085, L"");

    // Checkbox: Show Label
    addControl(BS_AUTOCHECKBOX, 0, 7, 50, 120, 12, IDC_CHECK_LABEL, 0x0080, L"Show label text");

    // Checkbox: Show Tooltip Detail
    addControl(BS_AUTOCHECKBOX, 0, 7, 68, 120, 12, IDC_CHECK_TOOLTIP, 0x0080, L"Show detailed tooltip");

    // Edit: Custom cache path
    addControl(ES_AUTOHSCROLL | WS_BORDER, 0,
               75, 88, 175, 14, IDC_EDIT_CACHEPATH, 0x0081, L"");

    // Edit: Refresh interval
    addControl(ES_AUTOHSCROLL | WS_BORDER | ES_NUMBER, 0,
               75, 108, 50, 14, IDC_EDIT_REFRESH, 0x0081, L"");

    // OK button
    addControl(BS_DEFPUSHBUTTON, 0, 140, 135, 50, 14, IDOK, 0x0080, L"OK");

    // Cancel button
    addControl(0, 0, 195, 135, 50, 14, IDCANCEL, 0x0080, L"Cancel");

    // Update item count
    pDlg->cDlgItems = 11;

    // Make dialog taller
    pDlg->cy = 220;

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
                        wcscmp(tempSettings.cachePath, m_settings.cachePath) != 0);

        m_settings = tempSettings;
        m_settings.Save(m_iniPath);
        ApplySettings();

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
    case TMI_VERSION:     return L"1.1.0";
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
}

// Export entry point
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &ClaudeTokenPlugin::Instance();
}
