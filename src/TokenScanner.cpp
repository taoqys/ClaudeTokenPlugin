#include "TokenScanner.h"

#include "ClaudeScanner.h"
#include "CodexScanner.h"

#include <limits>

bool TokenScanner::ScanDefault(UsageSnapshot& snapshot) const
{
    const auto paths = DiscoverDefaultPaths();
    return Scan(paths, MakeTimeContext(), snapshot);
}

bool TokenScanner::Scan(const ProviderPaths& paths, const TimeContext& context,
                        UsageSnapshot& snapshot) const
{
    const auto claude = ScanClaudeToday(paths.claude_roots, context);
    const auto codex = ScanCodexToday(paths.codex_session_roots, context);
    if (!claude.completed || !codex.completed ||
        claude.tokens > std::numeric_limits<std::uint64_t>::max() - codex.tokens)
    {
        return false;
    }

    snapshot.claude_tokens = claude.tokens;
    snapshot.codex_tokens = codex.tokens;
    snapshot.total_tokens = claude.tokens + codex.tokens;
    return true;
}
