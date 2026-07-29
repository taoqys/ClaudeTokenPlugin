#pragma once

#include <filesystem>
#include <vector>

#include "ScanTypes.h"
#include "TimeUtils.h"

ProviderScanResult ScanCodexToday(const std::vector<std::filesystem::path>& session_roots,
                                  const TimeContext& context);
