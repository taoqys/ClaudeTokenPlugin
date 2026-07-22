#include "ClaudeTokenPlugin.h"
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <ctime>

// Minimal JSON value extraction for the cache file format.
// Finds a key in a JSON object string and returns the integer value.
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

// Read the entire file into a string
static std::string ReadFileContent(const std::wstring& path)
{
    // Try UTF-8 path first
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        // Fallback: convert wstring path
        char buf[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, buf, MAX_PATH, nullptr, nullptr);
        file.open(buf, std::ios::binary);
    }
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Get today's date as YYYY-MM-DD
static std::string GetTodayDate()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_s(&t, &now);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return buf;
}

// Get user profile path (C:\Users\<name>)
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
    if (index >= 0 && index < ClaudeTokenItem::ITEM_COUNT)
        return &m_items[index];
    return nullptr;
}

void ClaudeTokenPlugin::DataRequired()
{
    LoadCache();
}

void ClaudeTokenPlugin::LoadCache()
{
    std::wstring cachePath = GetUserProfilePath() + L"\\.claude\\.token-cache.json";
    std::string content = ReadFileContent(cachePath);
    if (content.empty()) return;

    std::string today = GetTodayDate();

    // Parse the "daily" object. Format: "date|model": { "input": N, "output": N, ... }
    // We iterate through all keys matching today's date.
    long long totalInput = 0, totalOutput = 0, totalCache = 0;

    // Find the "daily" section
    size_t dailyPos = content.find("\"daily\"");
    if (dailyPos == std::string::npos) return;

    // Find the opening brace of daily object
    size_t objStart = content.find('{', dailyPos);
    if (objStart == std::string::npos) return;

    // Walk through key-value pairs in the daily object
    size_t pos = objStart + 1;
    int depth = 1;
    while (pos < content.size() && depth > 0)
    {
        // Skip whitespace
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n' || content[pos] == '\r' || content[pos] == '\t')) pos++;
        if (pos >= content.size()) break;

        if (content[pos] == '{') { depth++; pos++; continue; }
        if (content[pos] == '}') { depth--; pos++; continue; }
        if (content[pos] == ',') { pos++; continue; }
        if (content[pos] == ']') { depth--; pos++; continue; }

        if (content[pos] != '"') { pos++; continue; }

        // Parse key: "date|model"
        pos++; // skip opening quote
        size_t keyEnd = content.find('"', pos);
        if (keyEnd == std::string::npos) break;
        std::string key = content.substr(pos, keyEnd - pos);
        pos = keyEnd + 1;

        // Check if this key starts with today's date
        if (key.size() > today.size() && key.substr(0, today.size()) == today && key[today.size()] == '|')
        {
            // Find the value object { "input": N, "output": N, ... }
            size_t valStart = content.find('{', pos);
            if (valStart == std::string::npos || valStart - pos > 10) { pos = keyEnd + 1; continue; }
            size_t valEnd = content.find('}', valStart);
            if (valEnd == std::string::npos) break;

            std::string valObj = content.substr(valStart, valEnd - valStart + 1);
            totalInput += ExtractIntValue(valObj, "input");
            totalOutput += ExtractIntValue(valObj, "output");
            totalCache += ExtractIntValue(valObj, "cache_read");

            pos = valEnd + 1;
        }
        else
        {
            // Skip the value
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

    long long grandTotal = totalInput + totalOutput + totalCache;

    m_items[ClaudeTokenItem::ITEM_TOTAL].SetValue(grandTotal);

    // Build tooltip
    wchar_t tooltip[256];
    swprintf_s(tooltip,
        L"Claude Token Usage\nTotal: %lld\nInput: %lld\nOutput: %lld\nCache Read: %lld",
        grandTotal, totalInput, totalOutput, totalCache);
    m_tooltipInfo = tooltip;
}

const wchar_t* ClaudeTokenPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"Claude Token Monitor";
    case TMI_DESCRIPTION: return L"Displays Claude Code daily token usage on the taskbar";
    case TMI_AUTHOR:      return L"Claude";
    case TMI_COPYRIGHT:   return L"MIT License";
    case TMI_VERSION:     return L"1.0.0";
    case TMI_URL:         return L"https://github.com/anthropics/claude-code";
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
}

// Export entry point
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &ClaudeTokenPlugin::Instance();
}
