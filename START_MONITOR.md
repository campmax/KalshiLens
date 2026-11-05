# Start the automatic price monitor

This update adds a new monitor to the original KalshiLens application. The C++
engine collects and scores market data. A small Python standard-library process
keeps it running and serves a continuously updating dashboard on your computer.

## Windows

1. Extract the entire ZIP into a new folder. Keep the previous version separately.
2. Double-click **build_windows.cmd**. It compiles the program and runs the original
   49 tests plus 32 new monitor tests. Do not proceed if the build fails.
3. Double-click **run_monitor_demo.cmd** first. Your browser opens the simulated
   monitor. It collects a sample every two seconds. After about 20 seconds,
   invented rises, falls, isolated outliers and persistent moves become visible.
4. Close that console (or press Ctrl+C), then double-click **run_monitor.cmd**.
   It fetches real public Kalshi quotes. No API key or brokerage login is needed.
5. Keep the terminal open. Stop with Ctrl+C. If the browser fails to open, visit
   http://127.0.0.1:8765/ while the collector is running.

You already installed Python for the ETF downloader. This monitor needs no
additional Python packages. The C++ workload in Visual Studio is still required.
The report is a local application: putting its HTML on GitHub Pages does not run
the collector or supply live updates.

## What to look at

- **YES gap +5¢:** YES is five cents below the chosen reference midpoint/probability.
- **YES gap −5¢:** YES is five cents above that reference; a NO purchase might be
  the better comparison. Neither means you can actually trade at the midpoint.
- **Conservative net gap:** the pessimistic reference value minus the full-size
  purchase cost per contract, estimated fees and an extra execution allowance.
- **Historical candidate:** a repeated unusual move with a positive conditional
  net gap relative to prior prices. It needs an independent explanation; news
  may have changed the actual odds.
- **Model-based candidate:** the same cost check against a separately supplied
  probability range. The conclusion depends on that model's credibility.
- **Building history:** at least eight prior usable observations are required.
  In real mode, this usually takes several minutes; each API capture takes time.
- **Persistent move: reset:** the price stayed away from its old reference for
  five captures. The monitor retires that reference rather than treating the
  old price as permanently correct.
- **Not current / quality blocked:** no current candidate. Old numbers can remain
  visible for review, but they are not counted as current opportunities.

Click a market for the updating chart, costs, uncertainty range and reasons.
Search and use the candidate filter to reduce clutter. Zero candidates is normal.

## Follow a specific group

Open a terminal in the extracted project folder:

```powershell
py tools\live_dashboard.py --series KXHIGHNY --max-markets 8
```

Replace the series with one you want to study, or use `--event EVENT-TICKER` or
`--ticker MARKET-TICKER`. A smaller group gives faster, fresher snapshots. The
unfiltered default selects eight markets from the first scanned page; it is NOT
a scan of every listed market. Use `--pages` to broaden discovery, but this costs
more requests and can make earlier books stale. Each capture refreshes metadata,
fees and book depth. Market membership may rotate; disappearing markets break
consecutive confirmation.

Default polling pause: 15 seconds AFTER the capture completes. The browser checks
for a completed update every two seconds. This is sequential REST polling, not
instant exchange streaming. Timestamp checks can reject a slow batch.

## Change assumptions

Edit monitor_config.json. Requested quantity defaults to 10 contracts; the extra
execution allowance is $0.005 per contract; minimum conservative edge $0.01;
maximum spread $0.08. Save the file; it is reloaded at the next capture. Changing
history/scoring settings resets the historical baseline. Invalid configuration
stops new rankings until fixed; it does not silently retain a working setting.

## Independent probabilities

The program does not infer true probabilities from prices or automatically read
sports news, weather forecasts or economic releases. Without another model, it
only supplies historical anomaly estimates.

If you have a separately justified estimate, record it with:

```powershell
py tools\set_forecast.py YOUR-TICKER --probability 0.60 --lower 0.55 --upper 0.65 --source "Name and timestamp of my independent model" --minutes 15
```

The values above are illustrations. Do not apply them to an actual market without
an independent basis. The next cycle automatically incorporates the saved input.
An external forecasting program can update the same `forecasts` object using an
atomic file replacement. Each entry requires probability, lower, upper, source,
as_of_ms and valid_until_ms (UTC Unix milliseconds). The optional rules_hash can
bind an estimate to the rules fingerprint in monitor output. Expired estimates
are blocked. This is an input interface, not an included predictive model.

## Saved records and sharing

Each launch makes its own runs/monitor_TIMESTAMP folder. It contains current
state, latest output, logs, and up to 120 recent capture directories with raw API
responses, normalized results and exact monitor settings. Older capture folders
from that session are rotated out automatically. Increase `--retain` if needed.
A restart deliberately starts fresh history. Original static scan/watch commands
remain available for longer manual archival research.

No API credentials, account reads, order placement or cancellation are implemented.
Do not interpret a public-book anomaly as evidence of manipulation or sure profit.
