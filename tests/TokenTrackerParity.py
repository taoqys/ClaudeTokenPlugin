#!/usr/bin/env python3
"""Compare current-day totals from this scanner with token-tracker.

The test requires a checkout pinned to 4a3882bfef23be84652242622cc90debe0d70a5a,
the local ScannerParityHarness executable, and disposable fixture roots supplied
through environment variables. Missing optional prerequisites exit 77 so CTest
reports the integration test as skipped rather than passed.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

EXPECTED_COMMIT = "4a3882bfef23be84652242622cc90debe0d70a5a"
REQUIRED_ENVIRONMENT = (
    "TOKEN_TRACKER_SOURCE",
    "SCANNER_PARITY_HARNESS",
    "PARITY_CLAUDE_PROJECTS_ROOT",
    "PARITY_CODEX_SESSIONS_ROOT",
)


def skip(message: str) -> None:
    print(f"SKIPPED: {message}")
    raise SystemExit(77)


def read_required_environment() -> dict[str, str]:
    values: dict[str, str] = {}
    for name in REQUIRED_ENVIRONMENT:
        value = os.environ.get(name)
        if not value:
            skip(f"{name} is not set")
        values[name] = value
    return values


def verify_checkout(repository: Path) -> None:
    required_sources = (
        repository / "src" / "token_tracker" / "adapters" / "claude.py",
        repository / "src" / "token_tracker" / "adapters" / "codex.py",
        repository / "src" / "token_tracker" / "analyzer" / "aggregator.py",
    )
    if not repository.is_dir() or not all(path.is_file() for path in required_sources):
        skip("TOKEN_TRACKER_SOURCE is not the expected token-tracker source checkout")

    try:
        commit = subprocess.check_output(
            ["git", "-C", str(repository), "rev-parse", "HEAD"], text=True
        ).strip()
    except (OSError, subprocess.CalledProcessError) as error:
        skip(f"cannot determine token-tracker revision: {error}")
    if commit != EXPECTED_COMMIT:
        skip(f"token-tracker revision is {commit}, expected {EXPECTED_COMMIT}")


def daily_total(entries: object, aggregate_daily: object, daily_stats: object, today: str) -> int:
    stats = next((stat for stat in aggregate_daily(entries) if stat.date == today), daily_stats(date=today))
    return int(stats.total_tokens)


def main() -> int:
    values = read_required_environment()
    repository = Path(values["TOKEN_TRACKER_SOURCE"]).expanduser().resolve()
    harness = Path(values["SCANNER_PARITY_HARNESS"]).expanduser().resolve()
    claude_root = Path(values["PARITY_CLAUDE_PROJECTS_ROOT"]).expanduser().resolve()
    codex_root = Path(values["PARITY_CODEX_SESSIONS_ROOT"]).expanduser().resolve()
    verify_checkout(repository)

    if not harness.is_file() or not claude_root.is_dir() or not codex_root.is_dir():
        skip("scanner harness or fixture roots are unavailable")

    previous_claude = os.environ.get("CLAUDE_CONFIG_DIR")
    previous_codex = os.environ.get("CODEX_HOME")
    os.environ["CLAUDE_CONFIG_DIR"] = str(claude_root.parent)
    os.environ["CODEX_HOME"] = str(codex_root.parent)
    try:
        sys.path.insert(0, str(repository / "src"))
        try:
            from token_tracker.adapters import claude, codex
            from token_tracker.adapters.types import DailyStats
            from token_tracker.analyzer.aggregator import aggregate_daily
            from token_tracker.tz import system_tz
        except ModuleNotFoundError as error:
            skip(f"token-tracker Python dependency unavailable: {error}")

        today = datetime.now(system_tz()).strftime("%Y-%m-%d")
        upstream_claude = daily_total(claude.load_entries(hours_back=25), aggregate_daily, DailyStats, today)
        upstream_codex = daily_total(codex.load_entries(hours_back=25), aggregate_daily, DailyStats, today)
    finally:
        if previous_claude is None:
            os.environ.pop("CLAUDE_CONFIG_DIR", None)
        else:
            os.environ["CLAUDE_CONFIG_DIR"] = previous_claude
        if previous_codex is None:
            os.environ.pop("CODEX_HOME", None)
        else:
            os.environ["CODEX_HOME"] = previous_codex

    completed = subprocess.run(
        [str(harness), str(claude_root), str(codex_root)],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"ScannerParityHarness failed ({completed.returncode}): {completed.stderr.strip()}")

    try:
        scanner_claude, scanner_codex, scanner_total = (int(value) for value in completed.stdout.split())
    except ValueError as error:
        raise RuntimeError(f"invalid ScannerParityHarness output: {completed.stdout!r}") from error

    upstream_total = upstream_claude + upstream_codex
    result = {
        "scanner": {"claude": scanner_claude, "codex": scanner_codex, "total": scanner_total},
        "upstream": {"claude": upstream_claude, "codex": upstream_codex, "total": upstream_total},
    }
    print(json.dumps(result, sort_keys=True))
    if result["scanner"] != result["upstream"]:
        raise AssertionError("scanner totals differ from token-tracker current-day totals")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
