#include "TimeUtils.h"

#include <windows.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
constexpr std::uint64_t kTicksPerSecond = 10'000'000ULL;
constexpr std::uint64_t kTicksPerHour = 60ULL * 60ULL * kTicksPerSecond;

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::uint64_t MakeUtcTicks(unsigned short year, unsigned short month, unsigned short day,
                           unsigned short hour, unsigned short minute, unsigned short second,
                           unsigned short milliseconds = 0)
{
    SYSTEMTIME utc{};
    utc.wYear = year;
    utc.wMonth = month;
    utc.wDay = day;
    utc.wHour = hour;
    utc.wMinute = minute;
    utc.wSecond = second;
    utc.wMilliseconds = milliseconds;

    FILETIME file_time{};
    if (!SystemTimeToFileTime(&utc, &file_time))
        throw std::runtime_error("Cannot construct UTC test time");

    ULARGE_INTEGER raw{};
    raw.LowPart = file_time.dwLowDateTime;
    raw.HighPart = file_time.dwHighDateTime;
    return raw.QuadPart;
}

void TestExplicitOffsets()
{
    const auto utc = MakeUtcTicks(2025, 1, 2, 3, 4, 5);
    const auto zulu = ParseIso8601ToFileTimeTicks("2025-01-02T03:04:05Z");
    const auto plus_offset = ParseIso8601ToFileTimeTicks("2025-01-02T05:34:05+02:30");
    const auto minus_offset = ParseIso8601ToFileTimeTicks("2025-01-01T22:04:05-05:00");

    Require(zulu && *zulu == utc, "Z timestamp should preserve UTC instant");
    Require(plus_offset && *plus_offset == utc, "Positive offset should convert to UTC");
    Require(minus_offset && *minus_offset == utc, "Negative offset should convert to UTC");
}

void TestFractionsAndValidation()
{
    const auto exact = MakeUtcTicks(2025, 1, 2, 3, 4, 5, 120);
    const auto fractional = ParseIso8601ToFileTimeTicks("2025-01-02T03:04:05.12Z");
    const auto microsecond = ParseIso8601ToFileTimeTicks("2025-01-02T03:04:05.120001Z");
    const auto truncated = ParseIso8601ToFileTimeTicks("2025-01-02T03:04:05.1200019Z");
    Require(fractional && *fractional == exact, "Fractions should normalize to 100-nanosecond ticks");
    Require(microsecond && *microsecond == exact + 10, "Six fractional digits must retain microsecond precision");
    Require(truncated && *truncated == exact + 10, "Fractions beyond microseconds must match Python truncation");

    Require(!ParseIso8601ToFileTimeTicks("2025-02-29T00:00:00Z"), "Invalid non-leap date must fail");
    Require(!ParseIso8601ToFileTimeTicks("2024-02-30T00:00:00Z"), "Invalid leap-month date must fail");
    Require(!ParseIso8601ToFileTimeTicks("2025-01-02T24:00:00Z"), "Invalid hour must fail");
    Require(!ParseIso8601ToFileTimeTicks("2025-01-02T03:04:05+24:00"), "Invalid offset must fail");
    Require(!ParseIso8601ToFileTimeTicks("not-a-timestamp"), "Malformed timestamp must fail");
}

void TestCutoffAndLocalDate()
{
    const auto now = MakeUtcTicks(2025, 6, 1, 12, 0, 0);
    const auto today = GetLocalDate(now);
    Require(today.has_value(), "Current local date conversion must work");

    TimeContext context{};
    context.now_ticks = now;
    context.cutoff_ticks = now - 25ULL * kTicksPerHour;
    context.local_today = *today;

    Require(IsAtOrAfterCutoff(context.cutoff_ticks, context), "Cutoff boundary must be included");
    Require(!IsAtOrAfterCutoff(context.cutoff_ticks - 1, context), "Time before cutoff must be excluded");
    Require(IsOnLocalToday(now, context), "Context now must be local today");
}
}

int main()
{
    try
    {
        TestExplicitOffsets();
        TestFractionsAndValidation();
        TestCutoffAndLocalDate();
        std::cout << "All time utility tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
