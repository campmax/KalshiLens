# Data, configuration and reproducibility

## Network boundary

Only unauthenticated HTTPS GET to `https://external-api.kalshi.com/trade-api/v2` is implemented: `/markets`, `/markets/{ticker}`, `/markets/{ticker}/orderbook`, `/events/{event}`, `/series/{series}`. TLS verification stays enabled; redirects are disabled. Tickers/cursors are URL encoded. Default minimum request spacing is 300ms (configurable minimum 250ms). HTTP 429 and 5xx are retried up to three attempts with bounded backoff. Transport failures are recorded and surfaced. Response bodies are capped at 20MiB; imported JSON at 64MiB.

Requests return public data. No authentication, private signing key storage, or order API exists in this program. Raw archives may contain market text/URLs supplied by the exchange. The HTML report escapes markup and embedded JSON and loads no remote scripts. CSV strings beginning with formula characters are prefixed to reduce spreadsheet formula execution. Treat exported files as research records.

## Run directory

- `snapshot.json`: `kalshilens.snapshot.v1`, source, synthetic flag, capture start/end, coverage and `records`.
- Each record: original `market`, `event`, `series`, `orderbook`; local `received_ms`, RTT when available and errors.
- `raw/N.json`: request log and parsed response, including metadata used for fees.
- `http_audit.json`: HTTP status, path, attempt, local timing and errors. On a discovery failure this can be the principal output.
- `config_used.json`: resolved settings and filter choices. Intended as an audit record, not an input config (contains additional command/output fields).
- `analysis.json`: normalized quotes, warnings, fee estimates, size sweeps, scenarios, conditional checks and FNV1a64 snapshot fingerprint. FNV is a reproducibility aid, not a cryptographic authenticity guarantee.
- `report.html`: fully self-contained view of the analysis. Can be shared offline.
- `markets.csv`, `execution.csv`: normalized market and sweep tables.

Watch creates `capture_1`, `capture_2`, etc.; the top report shows the latest capture with observed history. A partial run is usable. `compare` emits comparison JSON/CSV for matched tickers; it does not infer why prices moved. `score` emits calibration JSON/CSV and console scores, not an HTML dashboard.

## Research config

See `examples/research_demo.json`. Optional top-level fields: `quantity`, `member` (`direct` or `fcm`), `max_skew_seconds`, `stale_after_seconds`, `beliefs`, `relationships`. Unknown fields are rejected. Command-line quantity/member override the file.

Beliefs map ticker to `probability` in [0,1], `uncertainty` and nonempty `source`. Implication records have `type: "implies"`, `antecedent`, `consequent`, `reviewed` and `rationale`. Unreviewed relationships are skipped. Defaults are deliberately illustrative, not a recommendation.

`score` input is an array of unique `{ "id": "your-forecast-id", "probability": 0.6, "outcome": 1 }` records. Outcomes must be 0 or 1. Retain the original forecast timestamp and evidence separately. The demo scores are invented.

## Extending the program

Keep analytics independent of HTTP; fake transports test collector behavior without the network. Add fee types explicitly with reference cases rather than defaulting unknown types to a standard fee. Preserve raw data and version schemas when adding fields. Add malformed-data and boundary tests for accounting changes. Do not substitute market names for validated settlement relationships.

Future work could add a documented WebSocket snapshot/delta recorder, independently versioned forecast models, event-time replay, and an audited historical paper-trading simulator. Those features are not represented as complete in version 1.0.
