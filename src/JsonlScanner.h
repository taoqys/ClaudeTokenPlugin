#pragma once
#include <windows.h>
#include <string>
#include <map>
#include <vector>

struct TokenStats
{
    long long input{};
    long long output{};
    long long cacheRead{};
    long long cacheCreation{};
    int count{};
};

// Aggregated data: key = "date|model"
using DailyMap = std::map<std::string, TokenStats>;

class JsonlScanner
{
public:
    // Scan all JSONL files and return aggregated daily stats
    static DailyMap ScanAll(const std::wstring& projectsDir);

private:
    // Scan a single JSONL file, yield entries via callback
    // Callback(msg_id, date_str, model, input, output, cacheRead, cacheCreation)
    using EntryCallback = void(const std::string& msgId,
                               const std::string& date,
                               const std::string& model,
                               long long input, long long output,
                               long long cacheRead, long long cacheCreation);

    static void ScanFile(const std::wstring& filepath,
                         const std::string& today,
                         std::map<std::string, TokenStats>& daily,
                         std::map<std::string, std::pair<std::string, TokenStats>>& seen);

    // Minimal JSON helpers
    static bool FindStringValue(const std::string& json, size_t start, size_t end,
                                const std::string& key, std::string& value);
    static long long FindIntValue(const std::string& json, size_t start, size_t end,
                                  const std::string& key);
    static size_t FindObjectEnd(const std::string& json, size_t start);
    static std::string GetTodayDate();
    static std::vector<std::wstring> GlobJsonl(const std::wstring& dir);
};
