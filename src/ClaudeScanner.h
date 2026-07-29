#pragma once

#include <filesystem>
#include <vector>

#include "ScanTypes.h"
#include "TimeUtils.h"

ProviderScanResult ScanClaudeToday(const std::vector<std::filesystem::path>& roots,
                                   const TimeContext& context);
