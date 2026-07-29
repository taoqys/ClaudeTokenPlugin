#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct LocalDate
{
    unsigned short year{};
    unsigned short month{};
    unsigned short day{};

    bool operator==(const LocalDate& other) const
    {
        return year == other.year && month == other.month && day == other.day;
    }
};

struct TimeContext
{
    std::uint64_t now_ticks{};
    std::uint64_t cutoff_ticks{};
    LocalDate local_today{};
};

TimeContext MakeTimeContext();
std::optional<std::uint64_t> ParseIso8601ToFileTimeTicks(std::string_view value);
std::optional<LocalDate> GetLocalDate(std::uint64_t file_time_ticks);
bool IsAtOrAfterCutoff(std::uint64_t file_time_ticks, const TimeContext& context);
bool IsOnLocalToday(std::uint64_t file_time_ticks, const TimeContext& context);
bool TryGetFileWriteTime(const std::filesystem::path& path, std::uint64_t& file_time_ticks);
