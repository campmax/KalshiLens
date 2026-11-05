# Validation record — version 1.0

Validated in the supplied Linux environment on 2026-10-02.

## Passed

- Modular C++17 source compiled using GCC 13.3 with `-O2 -Wall -Wextra -Wpedantic`, linked to libcurl; no compiler warnings reported.
- Generated standalone KalshiLens.cpp compiled separately using GCC C++17 and libcurl.
- All 49 C++ tests passed. Tests include decimal precision, complementary asks, aggregate levels, fee rounding and multipliers, partial fills, unsupported metadata, stale/crossed/invalid books, conditional settlement bounds, calibration, fake API pagination/error handling and snapshot comparison.
- End-to-end CLI checks passed: demo, calibration, same-snapshot comparison, configured analysis, output collision refusal, negative quantity rejection, embedded HTML/script escaping and all-invalid-book nonzero exit with a preserved diagnostic report.
- Dashboard JavaScript executed in jsdom: all 12 synthetic inspectors, probability and side changes, partial-fill scenario suppression, search, empty result state, sorting and navigation passed with no JavaScript errors. This checks DOM behavior, not rendered layout.
- Real public API capture completed for three KXHIGHNY markets, including order books and event/series metadata. One event metadata request timed out; that record correctly retained an unknown-fee warning. A later event request succeeded for the remaining records. The unmodified raw audit and capture are included in examples/live_capture. It is an archived observation, not current data or evidence of profitability.

## Not verified here

- Native MSVC/Windows compilation, the WinHTTP transport and Windows browser auto-launch. Build/run scripts and the single-file distribution are provided, but this environment is Linux. Run build_windows.cmd; it builds and executes the accounting test suite locally.
- Full visual browser rendering and responsive screenshot review. The browser installation download failed in this environment. JavaScript/DOM tests do not establish pixel-level layout correctness.
- CMake generator execution (CMake was unavailable here). Direct compiler builds were used.
- Exchange reconciliation against authenticated executions. No credentials or orders were used, and exact fill-fragmentation/account rounding cannot be reconstructed from aggregated public depth.
- Long-running uptime, exhaustive live-market coverage, exceptional settlement handling, predictive profitability or an independent audit.

A synthetic fixture is a software test, never an empirical strategy backtest. The release is a research application with explicit modeling limits, not a certified trading system.

## Automatic monitor extension — 2026-10-02

- Rebuilt both modular and generated single-file C++17 distributions with GCC 13,
  optimization and warning flags. Original 49 tests rerun: 49 passed, 0 failed.
- Added 32 monitor tests: 32 passed, 0 failed. Coverage includes robust median/MAD,
  isolated spikes, repeated moves, regime resets, causal prior-only references,
  wide/stale/crossed/one-sided/closed books, insufficient size, unknown fees,
  gaps and duplicate captures, rule/config changes, model expiry and uncertainty,
  execution allowance, and separation of synthetic and real histories.
- Fifteen-cycle synthetic integration run matched the expected candidate sequence:
  one independent-model example, four candidates during confirmed transient and
  persistent moves, then only the model example after the persistent baseline reset.
- New dashboard DOM checks passed: table, SVG chart, inspector selection, filters,
  search, sort, disconnected/error/stale/expired suppression and HTML escaping.
- Local HTTP integration passed: continuously changing API responses, no-store
  caching, Host validation, refusal to serve arbitrary project files, two-capture
  retention and clean shutdown.
- Live monitor captured three KXHIGHNY markets. One usable two-sided book entered
  warmup; two one-sided books were blocked. This was a connectivity/quality check,
  not evidence of a profitable signal or a predictive backtest.

The Windows/MSVC, full visual rendering, long-running uptime and profitability
limitations above still apply. Python's standard library serves the local monitor;
all market collection and pricing/anomaly calculations remain in C++.
