#pragma once

#include "PathDiscovery.h"
#include "ScanTypes.h"
#include "TimeUtils.h"

class TokenScanner
{
public:
    bool ScanDefault(UsageSnapshot& snapshot) const;
    bool Scan(const ProviderPaths& paths, const TimeContext& context,
              UsageSnapshot& snapshot) const;
};
