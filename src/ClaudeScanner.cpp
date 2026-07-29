#include "ClaudeScanner.h"

#include "JsonlReader.h"
#include "PathDiscovery.h"

#include <limits>
#include <string>
#include <unordered_set>

namespace
{
bool CheckedAdd(std::uint64_t& total, std::uint64_t value)
{
    if (total > std::numeric_limits<std::uint64_t>::max() - value)
        return false;
    total += value;
    return true;
}

bool TryReadUnsigned(const nlohmann::json& object, const char* key, std::uint64_t& value)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || iterator->is_null())
    {
        value = 0;
        return true;
    }
    if (!iterator->is_number_unsigned() && !iterator->is_number_integer())
        return false;
    if (iterator->is_number_integer() && iterator->get<std::int64_t>() < 0)
        return false;
    value = iterator->get<std::uint64_t>();
    return true;
}

bool IsFreshFile(const std::filesystem::path& path, const TimeContext& context)
{
    std::uint64_t modified_ticks = 0;
    return TryGetFileWriteTime(path, modified_ticks) && modified_ticks >= context.cutoff_ticks;
}
}

ProviderScanResult ScanClaudeToday(const std::vector<std::filesystem::path>& roots,
                                   const TimeContext& context)
{
    ProviderScanResult result{};
    std::unordered_set<std::string> seen;

    for (const auto& root : roots)
    {
        for (const auto& path : EnumerateJsonlFiles(root))
        {
            if (!IsFreshFile(path, context))
                continue;

            ++result.files_scanned;
            ReadJsonlObjects(path, [&](const nlohmann::json& data) {
                try
                {
                    const auto type = data.find("type");
                    if (type == data.end() || !type->is_string() || type->get<std::string>() != "assistant")
                        return;

                    const auto message = data.find("message");
                    if (message == data.end() || !message->is_object())
                        return;
                    const auto usage = message->find("usage");
                    if (usage == message->end() || !usage->is_object())
                        return;

                    const auto timestamp = data.find("timestamp");
                    if (timestamp == data.end() || !timestamp->is_string())
                        return;
                    const auto instant = ParseIso8601ToFileTimeTicks(timestamp->get<std::string>());
                    if (!instant || !IsAtOrAfterCutoff(*instant, context) || !IsOnLocalToday(*instant, context))
                        return;

                    std::uint64_t input = 0;
                    std::uint64_t output = 0;
                    std::uint64_t cache_creation = 0;
                    std::uint64_t cache_read = 0;
                    if (!TryReadUnsigned(*usage, "input_tokens", input) ||
                        !TryReadUnsigned(*usage, "output_tokens", output) ||
                        !TryReadUnsigned(*usage, "cache_creation_input_tokens", cache_creation) ||
                        !TryReadUnsigned(*usage, "cache_read_input_tokens", cache_read))
                    {
                        return;
                    }
                    if (input == 0 && output == 0 && cache_creation == 0 && cache_read == 0)
                        return;

                    const auto message_id = message->find("id");
                    const auto request_id = data.find("requestId");
                    const std::string id = message_id != message->end() && message_id->is_string()
                        ? message_id->get<std::string>() : "";
                    const std::string request = request_id != data.end() && request_id->is_string()
                        ? request_id->get<std::string>() : "";
                    const std::string dedup_key = id + ":" + request;
                    if (!seen.insert(dedup_key).second)
                        return;

                    std::uint64_t entry_total = 0;
                    if (!CheckedAdd(entry_total, input) || !CheckedAdd(entry_total, output) ||
                        !CheckedAdd(entry_total, cache_creation) || !CheckedAdd(entry_total, cache_read) ||
                        !CheckedAdd(result.tokens, entry_total))
                    {
                        result.completed = false;
                        return;
                    }
                    ++result.valid_entries;
                }
                catch (const nlohmann::json::exception&)
                {
                    // A malformed field must not abort the remaining JSONL stream.
                }
            });

            if (!result.completed)
                return result;
        }
    }

    return result;
}
