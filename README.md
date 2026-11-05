# KalshiLens

C++17 and Python prediction-market research with order-book depth, VWAP, fee-aware scenarios and robust anomaly monitoring.

# KalshiLens — automatic price monitor

**Start with [START_MONITOR.md](START_MONITOR.md) for the new continuously updating anomaly dashboard.** It adds rolling median/MAD detection, outlier quarantine, repeated-observation confirmation, persistent-move resets, expiring external forecast inputs, full-size fee-adjusted gaps and live quality status. The new local dashboard requires Python 3 with only its standard library; the collector and analytics remain C++17.

A C++17 public-data research terminal for Kalshi binary prediction markets, with a self-contained HTML dashboard. Designed to make pricing, liquidity, fees and assumptions inspectable. It studies potential mispricing; it does not manipulate markets or send orders.

**Windows users: start with START_HERE.md.** No Python is needed for the original static C++ commands; the new live dashboard uses Python. The included demonstration is synthetic and says nothing about actual profitability.

## What is implemented

- Public HTTPS market discovery with bounded cursor pagination, series/event/ticker filters, sequential rate limiting, metadata caching and request audit files.
- Exact fixed-point parsing of prices and fractional contract quantities; complementary YES/NO asks reconstructed from bids.
- Order-book depth, near-touch imbalance, spreads, midpoint, capacity-limited sweeps and size-dependent VWAP.
- Event/series fee overrides, quadratic taker fee model, direct-member/FCM balance precision, rounding accumulator simulation and explicit unknown-fee handling.
- Probability scenarios with source-labelled user estimates, uncertainty ranges, fee-adjusted break-even and possible profit/loss. No claim that a quote supplies an objective probability.
- Conditional consistency checks for mutually exclusive NO baskets and manually reviewed logical implications. No automatic claim of exhaustive outcomes or atomic arbitrage.
- Bounded repeated capture, timestamp/skew/staleness checks and snapshot comparison.
- Forecast calibration with Brier score, clipped log loss, reliability bins and Wilson intervals.
- Searchable responsive offline dashboard with book-depth charts, scenario controls, research notes and CSV export. No remote scripts or fonts.
- Immutable run folders, raw response archive, configuration record, JSON/CSV outputs, reproducibility fingerprint and 49 tests.

## Suggested research workflow

1. Run `demo` to understand the interface. Compare one-sided, crossed, thin and unknown-fee examples.
2. Scan a small series whose settlement rules you understand. Inspect the rules before interpreting any consistency flag.
3. Use `watch` to record price movements. Sequential polling is a sampling process, not a full exchange feed or a reconstruction of every trade/cancellation.
4. Supply an independently developed probability estimate and its source. Examine the uncertainty range and costs rather than selecting only the most favorable point estimate.
5. Record forecasts BEFORE settlement, then use `score` to evaluate calibration. Preserve a separate held-out set; do not retroactively rewrite predictions.
6. Compare multiple captures and document why a hypothesis failed. This version has no historical fill simulator or demonstrated trading strategy.

## Commands

Run from the project directory. On Windows use `.\bin\kalshilens.exe`; on Linux use `./build/kalshilens`.

```text
kalshilens demo --open
kalshilens scan --series KXHIGHNY --max-markets 12 --open
kalshilens scan --event YOUR-EVENT-TICKER --max-markets 24
kalshilens scan --ticker YOUR-MARKET-TICKER
kalshilens watch --series KXHIGHNY --max-markets 6 --samples 5 --interval 30
kalshilens analyze --in examples/demo/snapshot.json --config examples/research_demo.json
kalshilens compare --before runs/FIRST/snapshot.json --after runs/SECOND/snapshot.json
kalshilens score --in examples/forecasts_demo.json
kalshilens help
```

`--out` must name a NEW directory. `--quantity 12.50` requests fractional contracts. `--member fcm` uses cent balance precision; default direct uses $0.0001. The watch interval is a pause AFTER each capture, not a fixed-frequency guarantee. Refresh the browser after the next capture to see the updated report. Ctrl+C preserves completed captures. Old imported snapshots remain useful for descriptive analysis but stale structural candidates are suppressed.

## Build

Windows: `build_windows.cmd`, or open this folder in Visual Studio with CMake support. Use the separate test target to run tests. Single-file instructions are in START_HERE.md.

Linux (C++17 compiler, CMake and libcurl development package required):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/kalshilens demo
```

Without CMake:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp -lcurl -o kalshilens
g++ -std=c++17 -O2 tests/tests.cpp -lcurl -o kalshilens_tests
./kalshilens_tests
```

Windows uses native WinHTTP. Linux uses libcurl. The nlohmann JSON 3.11.3 single header and MIT license are vendored. No build-time download is required on Windows. Native Windows compilation needs verification on your machine; validation performed here is recorded in docs/VALIDATION.md.

## Source map

| File | Responsibility |
|---|---|
| src/core.hpp | Fixed-point book normalization, sweeps, fee ledger, scenarios, structural bounds, calibration |
| src/client.hpp | Read-only HTTPS, collection, audit archive, synthetic fixture, comparisons |
| src/main.cpp | CLI validation, config, run orchestration and report/CSV writing |
| web/dashboard.html | Editable HTML/CSS/JavaScript dashboard template |
| src/dashboard.hpp | Generated embedded dashboard template |
| KalshiLens.cpp | Generated single-file Visual Studio distribution |
| tests/tests.cpp | Accounting, malformed data and collector regression tests |
| tools/generate_single_file.py | Optional maintainer tool to regenerate embedded/template distribution |

After editing modular sources or web/dashboard.html, run the generator (Python is only needed for this maintainer convenience), or build the modular source with CMake. The distributed single-file source already includes the dashboard.

## Scope and limitations

Only ordinary $1 binary, non-multivariate markets are supported. No credentials, account history, execution, quote posting, cancellation, wallet, strategy optimizer, full-market crawling, high-frequency feed, predictive model, or manipulation detector is implemented. Depth and imbalance do not identify intent. Conditional checks depend on rules and could fail under exceptional settlements. Fee estimates can differ from actual fills due to fragmentation and account-specific provisions. Data can cancel or change before any hypothetical fill. See docs/METHODOLOGY.md and docs/API_AND_FILES.md.
