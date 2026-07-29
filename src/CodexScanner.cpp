#include "CodexScanner.h"

#include "JsonlReader.h"
#include "PathDiscovery.h"

#include <limits>
#include <string>
#include <unordered_set>

namespace
{
struct SessionUsage
{
    std::string session_id;
    std::string session_timestamp;
    bool has_last_usage{};
    std::uint64_t input{};
    std::uint64_t cached_input{};
    std::uint64_t output{};
    std::uint64_t reasoning{};
};

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

ProviderScanResult ScanCodexToday(const std::vector<std::filesystem::path>& session_roots,
                                  const TimeContext& context)
{
    ProviderScanResult result{};
    std::unordered_set<std::string> seen;

    for (const auto& root : session_roots)
    {
        for (const auto& path : EnumerateJsonlFiles(root))
        {
            if (!IsFreshFile(path, context))
                continue;

            ++result.files_scanned;
            SessionUsage session{};
            ReadJsonlObjects(path, [&](const nlohmann::json& data) {
                try
                {
                    const auto type = data.find("type");
                    if (type == data.end() || !type->is_string())
                        return;

                    if (type->get<std::string>() == "session_meta")
                    {
                        const auto payload = data.find("payload");
                        if (payload == data.end() || !payload->is_object())
                            return;
                        const auto id = payload->find("id");
                        const auto timestamp = payload->find("timestamp");
                        if (id != payload->end() && id->is_string())
                            session.session_id = id->get<std::string>();
                        if (timestamp != payload->end() && timestamp->is_string())
                            session.session_timestamp = timestamp->get<std::string>();
                        return;
                    }

                    if (type->get<std::string>() != "event_msg")
                        return;
                    const auto payload = data.find("payload");
                    if (payload == data.end() || !payload->is_object())
                        return;
                    const auto payload_type = payload->find("type");
                    if (payload_type == payload->end() || !payload_type->is_string() ||
                        payload_type->get<std::string>() != "token_count")
                    {
                        return;
                    }
                    const auto info = payload->find("info");
                    if (info == payload->end() || !info->is_object())
                        return;
                    const auto total_usage = info->find("total_token_usage");
                    if (total_usage == info->end() || !total_usage->is_object())
                        return;

                    std::uint64_t input = 0;
                    std::uint64_t cached_input = 0;
                    std::uint64_t output = 0;
                    std::uint64_t reasoning = 0;
                    if (!TryReadUnsigned(*total_usage, "input_tokens", input) ||
                        !TryReadUnsigned(*total_usage, "cached_input_tokens", cached_input) ||
                        !TryReadUnsigned(*total_usage, "output_tokens", output) ||
                        !TryReadUnsigned(*total_usage, "reasoning_output_tokens", reasoning))
                    {
                        return;
                    }

                    session.input = input;
                    session.cached_input = cached_input;
                    session.output = output;
                    session.reasoning = reasoning;
                    session.has_last_usage = true;
                }
                catch (const nlohmann::json::exception&)
                {
                    // Ignore one malformed event and retain a previously valid token_count.
                }
            });

            if (!session.has_last_usage || session.session_id.empty())
                continue;

            // token-tracker computes ordinary input as input - cached. A cached-only
            // session therefore has neither ordinary input nor output and is skipped.
            if (session.input == session.cached_input && session.output == 0 && session.reasoning == 0)
                continue;

            const auto instant = ParseIso8601ToFileTimeTicks(session.session_timestamp);
            if (!instant || !IsAtOrAfterCutoff(*instant, context) || !IsOnLocalToday(*instant, context))
                continue;
            if (!seen.insert(session.session_id).second)
                continue;

            std::uint64_t session_total = 0;
            if (!CheckedAdd(session_total, session.input) || !CheckedAdd(session_total, session.output) ||
                !CheckedAdd(session_total, session.reasoning) || !CheckedAdd(result.tokens, session_total))
            {
                result.completed = false;
                return result;
            }
            ++result.valid_entries;
        }
    }

    return result;
}
