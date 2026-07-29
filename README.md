# AI CLI Token Tracker for TrafficMonitor

Windows TrafficMonitor plugin DLL that reports **today's** local token totals for Claude Code, Codex, and their sum.

## Semantics

The scanner follows the current-day aggregation behavior in [`stormzhang/token-tracker`](https://github.com/stormzhang/token-tracker), with its `hours_back=25` loading window:

- **Claude Code**: recursively reads JSONL under `CLAUDE_CONFIG_DIR\projects` (for every comma-separated root), `%USERPROFILE%\.claude\projects`, and `%USERPROFILE%\.config\claude\projects`. It counts assistant usage as `input_tokens + output_tokens + cache_creation_input_tokens + cache_read_input_tokens`, de-duplicated by `message.id:requestId`.
- **Codex**: recursively reads JSONL under `CODEX_HOME\sessions`, or `%USERPROFILE%\.codex\sessions` when the environment variable is absent. Each session uses only its last `token_count` cumulative record and counts `input_tokens + output_tokens + reasoning_output_tokens` exactly once per session ID.
- For both providers, the timestamp must be within the last 25 hours and fall on the current **Windows local date**.

The DLL exposes these TrafficMonitor items:

| Item ID | Label |
| --- | --- |
| `ClaudeTodayTokens` | Claude |
| `CodexTodayTokens` | Codex |
| `TotalTodayTokens` | Total |

The first `DataRequired()` call scans immediately; subsequent calls are throttled to a 20-second interval. Item getters return cached strings only and perform no I/O.

## Build

Requires Visual Studio 2022 C++ tools and CMake 3.25+.

```powershell
cmake -S C:\path\to\ClaudeTokenPlugin -B C:\path\to\ClaudeTokenPlugin.build -G "Visual Studio 17 2022" -A x64
cmake --build C:\path\to\ClaudeTokenPlugin.build --config Release
ctest --test-dir C:\path\to\ClaudeTokenPlugin.build -C Release --output-on-failure
```

The generated `Release\ClaudeTokenPlugin.dll` must match the architecture of the installed TrafficMonitor. Copy it into TrafficMonitor's plugin directory, then enable its three items in TrafficMonitor's display-item settings.

## Dependencies

- TrafficMonitor `PluginInterface.h` is bundled from the official project interface.
- [nlohmann/json](https://github.com/nlohmann/json) v3.12.0 is bundled as a header-only MIT dependency.

## Optional upstream parity test

`TokenTrackerParity` is registered with CTest but intentionally reports **SKIPPED** unless its explicit, disposable-fixture prerequisites are configured. It never scans or modifies live Claude/Codex data for this test.

Set these environment variables before running CTest:

- `TOKEN_TRACKER_SOURCE`: a checkout pinned to `4a3882bfef23be84652242622cc90debe0d70a5a`;
- `PARITY_CLAUDE_PROJECTS_ROOT`: a fixture `projects` directory;
- `PARITY_CODEX_SESSIONS_ROOT`: a fixture `sessions` directory.

CTest injects the locally built `ScannerParityHarness`. The test compares separate Claude, Codex, and combined current-day totals against token-tracker's public adapters and `aggregate_daily()` path. Missing source, dependencies, harness, or fixture roots exits with code 77 and is marked skipped; a real mismatch fails the test.

The DLL makes no network requests, opens no SQLite database, and writes no cache or log file in Release builds.
