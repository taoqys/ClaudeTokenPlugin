#include "JsonlScanner.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>

// Get today's date as YYYY-MM-DD
std::string JsonlScanner::GetTodayDate()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_s(&t, &now);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return buf;
}

// Find the end of a JSON object starting at position of '{'
size_t JsonlScanner::FindObjectEnd(const std::string& json, size_t start)
{
    if (start >= json.size() || json[start] != '{') return std::string::npos;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = start; i < json.size(); i++)
    {
        char c = json[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') { inString = !inString; continue; }
        if (inString) continue;
        if (c == '{') depth++;
        else if (c == '}') { depth--; if (depth == 0) return i; }
    }
    return std::string::npos;
}

// Find a string value by key in a JSON object range [start, end)
bool JsonlScanner::FindStringValue(const std::string& json, size_t start, size_t end,
                                    const std::string& key, std::string& value)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search, start);
    if (pos == std::string::npos || pos >= end) return false;
    pos += search.size();
    while (pos < end && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= end || json[pos] != ':') return false;
    pos++;
    while (pos < end && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= end || json[pos] != '"') return false;
    pos++;
    size_t valEnd = json.find('"', pos);
    if (valEnd == std::string::npos || valEnd >= end) return false;
    value = json.substr(pos, valEnd - pos);
    return true;
}

// Find an integer value by key in a JSON object range [start, end)
long long JsonlScanner::FindIntValue(const std::string& json, size_t start, size_t end,
                                      const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search, start);
    if (pos == std::string::npos || pos >= end) return 0;
    pos += search.size();
    while (pos < end && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= end || json[pos] != ':') return 0;
    pos++;
    while (pos < end && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    size_t valEnd = pos;
    if (valEnd < end && json[valEnd] == '-') valEnd++;
    while (valEnd < end && json[valEnd] >= '0' && json[valEnd] <= '9') valEnd++;
    if (valEnd == pos) return 0;
    return _strtoi64(json.substr(pos, valEnd - pos).c_str(), nullptr, 10);
}

// Glob for *.jsonl files in subdirectories
std::vector<std::wstring> JsonlScanner::GlobJsonl(const std::wstring& dir)
{
    std::vector<std::wstring> result;
    WIN32_FIND_DATAW fd;

    // First level: project directories
    std::wstring pattern = dir + L"\\*";
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return result;

    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        // Find JSONL files in this project dir
        std::wstring jsonlPattern = dir + L"\\" + fd.cFileName + L"\\*.jsonl";
        WIN32_FIND_DATAW jfd;
        HANDLE hJsonl = FindFirstFileW(jsonlPattern.c_str(), &jfd);
        if (hJsonl == INVALID_HANDLE_VALUE) continue;

        do
        {
            if (!(jfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                result.push_back(dir + L"\\" + fd.cFileName + L"\\" + jfd.cFileName);
            }
        } while (FindNextFileW(hJsonl, &jfd));
        FindClose(hJsonl);

    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    return result;
}

// Scan a single JSONL file with deduplication
void JsonlScanner::ScanFile(const std::wstring& filepath, const std::string& today,
                             std::map<std::string, TokenStats>& daily,
                             std::map<std::string, std::pair<std::string, TokenStats>>& seen)
{
    // Read file
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        // Try UTF-8 conversion
        char buf[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, filepath.c_str(), -1, buf, MAX_PATH, nullptr, nullptr);
        file.open(buf, std::ios::binary);
    }
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;

        // Quick check: must contain "assistant"
        if (line.find("\"assistant\"") == std::string::npos) continue;

        // Find "type":"assistant"
        size_t typePos = line.find("\"type\"");
        if (typePos == std::string::npos) continue;
        size_t typeValStart = line.find('"', typePos + 6);
        if (typeValStart == std::string::npos) continue;
        typeValStart++;
        size_t typeValEnd = line.find('"', typeValStart);
        if (typeValEnd == std::string::npos) continue;
        if (line.substr(typeValStart, typeValEnd - typeValStart) != "assistant") continue;

        // Find "message":{...}
        size_t msgPos = line.find("\"message\"");
        if (msgPos == std::string::npos) continue;
        size_t msgObjStart = line.find('{', msgPos + 9);
        if (msgObjStart == std::string::npos) continue;
        size_t msgObjEnd = FindObjectEnd(line, msgObjStart);
        if (msgObjEnd == std::string::npos) continue;

        // Extract msg.id for dedup
        std::string msgId;
        FindStringValue(line, msgObjStart, msgObjEnd, "id", msgId);

        // Find "usage":{...} inside message
        size_t usagePos = line.find("\"usage\"", msgObjStart);
        if (usagePos == std::string::npos || usagePos >= msgObjEnd) continue;
        size_t usageObjStart = line.find('{', usagePos + 7);
        if (usageObjStart == std::string::npos || usageObjStart >= msgObjEnd) continue;
        size_t usageObjEnd = FindObjectEnd(line, usageObjStart);
        if (usageObjEnd == std::string::npos || usageObjEnd > msgObjEnd) continue;

        // Extract token values
        long long inputTokens = FindIntValue(line, usageObjStart, usageObjEnd, "input_tokens");
        long long outputTokens = FindIntValue(line, usageObjStart, usageObjEnd, "output_tokens");
        long long cacheRead = FindIntValue(line, usageObjStart, usageObjEnd, "cache_read_input_tokens");
        long long cacheCreation = FindIntValue(line, usageObjStart, usageObjEnd, "cache_creation_input_tokens");

        // Extract model
        std::string model;
        FindStringValue(line, msgObjStart, msgObjEnd, "model", model);
        if (model.empty()) model = "unknown";

        // Extract timestamp from the outer object (not message)
        std::string timestamp;
        FindStringValue(line, 0, line.size(), "timestamp", timestamp);

        // Parse date from timestamp (format: 2026-07-22T00:35:36.123Z)
        std::string date;
        if (timestamp.size() >= 10)
        {
            date = timestamp.substr(0, 10);
        }
        if (date.empty() || date != today) continue;

        // Build aggregation key
        std::string key = date + "|" + model;

        // Deduplicate by msg_id: remove old entry if exists
        if (!msgId.empty())
        {
            auto it = seen.find(msgId);
            if (it != seen.end())
            {
                // Subtract old values
                const auto& old = it->second;
                auto& oldStats = daily[old.first];
                oldStats.input -= old.second.input;
                oldStats.output -= old.second.output;
                oldStats.cacheRead -= old.second.cacheRead;
                oldStats.cacheCreation -= old.second.cacheCreation;
                oldStats.count--;
            }
            TokenStats entry{inputTokens, outputTokens, cacheRead, cacheCreation, 1};
            seen[msgId] = {key, entry};
        }

        // Add new values
        auto& stats = daily[key];
        stats.input += inputTokens;
        stats.output += outputTokens;
        stats.cacheRead += cacheRead;
        stats.cacheCreation += cacheCreation;
        stats.count++;
    }
}

// Main scan function
DailyMap JsonlScanner::ScanAll(const std::wstring& projectsDir)
{
    std::string today = GetTodayDate();
    DailyMap daily;
    std::map<std::string, std::pair<std::string, TokenStats>> seen;

    auto files = GlobJsonl(projectsDir);
    for (const auto& f : files)
    {
        ScanFile(f, today, daily, seen);
    }

    // Remove entries with zero or negative counts (from dedup subtraction)
    DailyMap result;
    for (auto& [key, stats] : daily)
    {
        if (stats.count > 0)
            result[key] = stats;
    }
    return result;
}
