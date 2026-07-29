#include "TimeUtils.h"

#include <windows.h>

#include <array>
#include <cctype>
#include <limits>

namespace
{
constexpr std::uint64_t kTicksPerSecond = 10'000'000ULL;
constexpr std::uint64_t kTicksPerHour = 60ULL * 60ULL * kTicksPerSecond;

bool IsDigit(char value)
{
    return value >= '0' && value <= '9';
}

bool ReadFixedDigits(std::string_view value, std::size_t& position, std::size_t count, int& result)
{
    if (position + count > value.size())
        return false;

    result = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const char digit = value[position + index];
        if (!IsDigit(digit))
            return false;
        result = result * 10 + (digit - '0');
    }
    position += count;
    return true;
}

bool Consume(std::string_view value, std::size_t& position, char expected)
{
    if (position >= value.size() || value[position] != expected)
        return false;
    ++position;
    return true;
}

std::uint64_t ToTicks(const FILETIME& value)
{
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

FILETIME ToFileTime(std::uint64_t value)
{
    ULARGE_INTEGER integer{};
    integer.QuadPart = value;
    FILETIME file_time{};
    file_time.dwLowDateTime = integer.LowPart;
    file_time.dwHighDateTime = integer.HighPart;
    return file_time;
}

bool BuildUtcTicks(const SYSTEMTIME& utc, std::uint64_t& ticks)
{
    FILETIME file_time{};
    if (!SystemTimeToFileTime(&utc, &file_time))
        return false;
    ticks = ToTicks(file_time);
    return true;
}

bool BuildLocalTicks(const SYSTEMTIME& local, std::uint64_t& ticks)
{
    DYNAMIC_TIME_ZONE_INFORMATION dynamic_zone{};
    if (GetDynamicTimeZoneInformation(&dynamic_zone) == TIME_ZONE_ID_INVALID)
        return false;

    SYSTEMTIME utc{};
    if (!TzSpecificLocalTimeToSystemTimeEx(&dynamic_zone, &local, &utc))
        return false;
    return BuildUtcTicks(utc, ticks);
}

bool IsLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth(int year, int month)
{
    static constexpr std::array<int, 12> kDays = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (month < 1 || month > 12)
        return 0;
    if (month == 2 && IsLeapYear(year))
        return 29;
    return kDays[month - 1];
}
}

TimeContext MakeTimeContext()
{
    FILETIME now_file_time{};
    GetSystemTimeAsFileTime(&now_file_time);

    TimeContext context{};
    context.now_ticks = ToTicks(now_file_time);
    context.cutoff_ticks = context.now_ticks > 25ULL * kTicksPerHour
        ? context.now_ticks - 25ULL * kTicksPerHour
        : 0;

    const auto today = GetLocalDate(context.now_ticks);
    if (today)
        context.local_today = *today;
    return context;
}

std::optional<std::uint64_t> ParseIso8601ToFileTimeTicks(std::string_view value)
{
    std::size_t position = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (!ReadFixedDigits(value, position, 4, year) || !Consume(value, position, '-') ||
        !ReadFixedDigits(value, position, 2, month) || !Consume(value, position, '-') ||
        !ReadFixedDigits(value, position, 2, day) || position >= value.size() ||
        (value[position] != 'T' && value[position] != 't' && value[position] != ' '))
    {
        return std::nullopt;
    }
    ++position;

    if (!ReadFixedDigits(value, position, 2, hour) || !Consume(value, position, ':') ||
        !ReadFixedDigits(value, position, 2, minute))
    {
        return std::nullopt;
    }

    if (position < value.size() && value[position] == ':')
    {
        ++position;
        if (!ReadFixedDigits(value, position, 2, second))
            return std::nullopt;
    }

    std::uint64_t fractional_ticks = 0;
    if (position < value.size() && value[position] == '.')
    {
        ++position;
        int fraction_digits = 0;
        while (position < value.size() && IsDigit(value[position]))
        {
            // Python datetime retains microsecond precision and truncates extra digits.
            if (fraction_digits < 6)
            {
                fractional_ticks = fractional_ticks * 10ULL +
                    static_cast<unsigned int>(value[position] - '0');
                ++fraction_digits;
            }
            ++position;
        }
        if (fraction_digits == 0)
            return std::nullopt;
        while (fraction_digits < 6)
        {
            fractional_ticks *= 10ULL;
            ++fraction_digits;
        }
        fractional_ticks *= 10ULL;
    }

    if (year < 1601 || month < 1 || month > 12 || day < 1 || day > DaysInMonth(year, month) ||
        hour > 23 || minute > 59 || second > 59)
    {
        return std::nullopt;
    }

    SYSTEMTIME civil{};
    civil.wYear = static_cast<WORD>(year);
    civil.wMonth = static_cast<WORD>(month);
    civil.wDay = static_cast<WORD>(day);
    civil.wHour = static_cast<WORD>(hour);
    civil.wMinute = static_cast<WORD>(minute);
    civil.wSecond = static_cast<WORD>(second);
    civil.wMilliseconds = 0;

    if (position == value.size())
    {
        civil.wMilliseconds = static_cast<WORD>(fractional_ticks / 10'000ULL);
        const std::uint64_t sub_millisecond_ticks = fractional_ticks % 10'000ULL;
        std::uint64_t local_ticks = 0;
        if (!BuildLocalTicks(civil, local_ticks) ||
            local_ticks > std::numeric_limits<std::uint64_t>::max() - sub_millisecond_ticks)
        {
            return std::nullopt;
        }
        return local_ticks + sub_millisecond_ticks;
    }

    int offset_minutes = 0;
    if (value[position] == 'Z' || value[position] == 'z')
    {
        ++position;
    }
    else if (value[position] == '+' || value[position] == '-')
    {
        const int sign = value[position] == '+' ? 1 : -1;
        ++position;
        int offset_hours = 0;
        int offset_mins = 0;
        if (!ReadFixedDigits(value, position, 2, offset_hours) || !Consume(value, position, ':') ||
            !ReadFixedDigits(value, position, 2, offset_mins) || offset_hours > 23 || offset_mins > 59)
        {
            return std::nullopt;
        }
        offset_minutes = sign * (offset_hours * 60 + offset_mins);
    }
    else
    {
        return std::nullopt;
    }

    if (position != value.size())
        return std::nullopt;

    std::uint64_t ticks = 0;
    if (!BuildUtcTicks(civil, ticks) || ticks > std::numeric_limits<std::uint64_t>::max() - fractional_ticks)
        return std::nullopt;
    ticks += fractional_ticks;

    const std::int64_t offset_ticks = static_cast<std::int64_t>(offset_minutes) * 60LL *
                                      static_cast<std::int64_t>(kTicksPerSecond);
    if (offset_ticks >= 0)
    {
        const auto magnitude = static_cast<std::uint64_t>(offset_ticks);
        if (ticks < magnitude)
            return std::nullopt;
        ticks -= magnitude;
    }
    else
    {
        const auto magnitude = static_cast<std::uint64_t>(-offset_ticks);
        if (ticks > std::numeric_limits<std::uint64_t>::max() - magnitude)
            return std::nullopt;
        ticks += magnitude;
    }
    return ticks;
}

std::optional<LocalDate> GetLocalDate(std::uint64_t file_time_ticks)
{
    const FILETIME utc_file_time = ToFileTime(file_time_ticks);
    SYSTEMTIME utc{};
    if (!FileTimeToSystemTime(&utc_file_time, &utc))
        return std::nullopt;

    DYNAMIC_TIME_ZONE_INFORMATION dynamic_zone{};
    const DWORD zone_id = GetDynamicTimeZoneInformation(&dynamic_zone);
    if (zone_id == TIME_ZONE_ID_INVALID)
        return std::nullopt;

    SYSTEMTIME local{};
    if (!SystemTimeToTzSpecificLocalTimeEx(&dynamic_zone, &utc, &local))
        return std::nullopt;

    return LocalDate{local.wYear, local.wMonth, local.wDay};
}

bool IsAtOrAfterCutoff(std::uint64_t file_time_ticks, const TimeContext& context)
{
    return file_time_ticks >= context.cutoff_ticks;
}

bool IsOnLocalToday(std::uint64_t file_time_ticks, const TimeContext& context)
{
    const auto local_date = GetLocalDate(file_time_ticks);
    return local_date && *local_date == context.local_today;
}

bool TryGetFileWriteTime(const std::filesystem::path& path, std::uint64_t& file_time_ticks)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
        return false;
    file_time_ticks = ToTicks(attributes.ftLastWriteTime);
    return true;
}
