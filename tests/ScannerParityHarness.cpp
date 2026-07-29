#include "ClaudeScanner.h"
#include "CodexScanner.h"
#include "TokenScanner.h"

#include <cstdint>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: ScannerParityHarness <claude-projects-root> <codex-sessions-root>\n";
        return 2;
    }

    ProviderPaths paths{};
    paths.claude_roots.emplace_back(argv[1]);
    paths.codex_session_roots.emplace_back(argv[2]);

    UsageSnapshot snapshot{};
    if (!TokenScanner().Scan(paths, MakeTimeContext(), snapshot))
    {
        std::cerr << "Scanner failed to produce a complete snapshot\n";
        return 1;
    }

    std::cout << snapshot.claude_tokens << ' ' << snapshot.codex_tokens << ' '
              << snapshot.total_tokens << '\n';
    return 0;
}
