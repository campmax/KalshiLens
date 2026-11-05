# Monitor model and limits

## Causal robust history

Only prior accepted two-sided midpoints enter the reference. The initial defaults
require eight observations spanning at least 60 seconds, at most 60 retained
accepted samples, and a two-hour lookback. The reference is their median. The
noise scale is max(1.4826 * median absolute deviation, $0.005, current spread / 2).
The score is (reference - current midpoint) / scale. It is a descriptive robust
score, NOT a normally distributed significance test or calibrated probability.

A candidate move must exceed both 3.5 scale units and $0.02. A first abnormal
observation is quarantined rather than inserted into the accepted history. Two
consecutive same-direction observations are needed for an alert. A reversal,
invalid/missing observation or changed model resets confirmation. Five consecutive
abnormal same-direction observations replace the old reference history with the
new observations; the monitor must warm up again. This heuristic trades speed
against false positives. Real news can invalidate an old reference immediately,
well before the five-observation reset. Outlier treatment does not establish
that observed quotes are erroneous.

Long receipt gaps, changed contract rules/close times, or changed scoring settings
reset history. Real and synthetic state cannot be combined. Duplicate and
out-of-order receipt timestamps cannot confirm a signal. Repeated REST reads may
still observe the same underlying exchange orders: local receipt time does not
establish quote freshness at the matching engine.

## Quantity, price and costs

The original engine reconstructs asks from opposite-side bids and sweeps depth at
the requested quantity. Partial fills are excluded from candidate comparisons.
The net gap is reference probability minus (purchase notional + estimated taker
fee)/filled quantity minus execution_buffer. For NO, use 1 minus the YES
reference. The conservative gap uses the lower bound for YES or 1 minus the
upper bound for NO. The side with the highest conservative gap is displayed.
Default minimum gap is one cent per contract after all modeled costs.

Historical reference bounds are clipped median +/- two noise scales. They are
heuristic dispersion bands, not statistical confidence intervals. Supplied model
bounds are user/model assumptions, not automatically validated coverage intervals.
Expected dollars multiply the gap by quantity; they assume ordinary $1 binary
settlement at those probabilities. Historical price deviations are not by
 themselves legitimate settlement forecasts. A candidate can lose its full cost.

## Quality gates

The monitor blocks inactive/closed, non-$1-binary, multivariate, crossed,
one-sided, stale, missing-timestamp and excessively wide books. Close-time parsing
accepts UTC Z timestamps, optionally fractional seconds; unrecognized timestamps
fail closed. Model inputs must be ordered within [0,1], sourced, not future-dated
and not expired. Unknown fees produce no cost-adjusted ranking. Account-specific
fee details and fill fragmentation remain limitations of the original model.
No multiple-comparison correction is claimed: scores across many correlated
markets are screening metrics, not p-values or calibrated discovery rates.

## Scheduler and display

A Python standard-library scheduler invokes the C++ public collector and anomaly
engine. Each successful cycle replaces state/output files and a synchronized
in-memory dashboard response. Failed cycles retain earlier records but show an
error; the UI removes current-candidate status while unavailable or stale. There
is no server endpoint for modifying files or placing orders. The server binds
127.0.0.1 only, accepts a local Host header, exposes only / and /api/latest, and
serves no arbitrary directory. Market text is escaped before insertion into HTML.

The browser fetches every two seconds. Public API captures are sequential, with
a minimum 300ms request interval; the default scheduler pauses 15 seconds after
completion. Failures use a bounded increasing pause. Live collection needs a
running local process and network access. It does not run from a static hosted
HTML page. The scope is the configured bounded scan, not the entire exchange.
No external news ingestion or autonomous event-specific forecasting is included.

Primary API references checked:
https://docs.kalshi.com/getting_started/orderbook_responses
https://docs.kalshi.com/getting_started/fee_rounding
