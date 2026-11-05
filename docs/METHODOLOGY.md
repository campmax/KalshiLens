# Calculation conventions

## Order book and precision

Kalshi's fixed-point order book returns YES and NO bids. A NO bid at p provides a YES ask at 1−p, and a YES bid provides a NO ask at 1−p. Bids are sorted descending, duplicate levels are aggregated, zero sizes removed. Prices are integer ten-thousandths of a dollar; quantities integer hundredths of a contract. Their product is an exact integer count of microdollars. Malformed precision, endpoint bid prices and legacy schemas are rejected.

YES midpoint = (best YES bid + best YES ask)/2, only when both exist. This is a descriptive quote statistic, not a probability forecast. Spread is ask minus bid. Imbalance uses bid quantity within $0.02 of each side's best bid: (YES quantity − NO quantity)/(YES quantity + NO quantity). A large value is neither evidence of manipulation nor a directional model.

## Hypothetical buys

Walk reconstructed asks from cheapest upward until the requested size is filled or visible depth ends. Report filled size and partial status explicitly. VWAP = notional/filled contracts. Fee-adjusted break-even = total cost/filled contracts. An incomplete basket cannot support a full-size consistency result. These sweeps assume displayed liquidity survives and omit latency, queue position and hidden depth. There is no sell-side unwind/strategy execution model.

## Fees

The implemented taker model is 0.07 × multiplier × contracts × p × (1−p). Event fee overrides take priority over series metadata. `quadratic` and `quadratic_with_maker_fees` use this taker model. Maker rebates/fees are not simulated because all sweeps cross available asks. Unsupported `flat` schedules or missing metadata produce unknown fees, not zero fees. Potential waivers are flagged but not credited.

The fee-rounding API documentation distinguishes direct-member $0.0001 and FCM $0.01 balance grids. Theoretical trade fees are rounded up to microdollars, cost plus fee is rounded up to the balance grid, rounding excess is accumulated, and grid-sized rebates are limited to avoid a negative fee. Each book price level is treated as ONE hypothetical fill and each independent sweep starts a fresh accumulator. Real fragmentation, starting accumulator state, exact fee versions and account provisions can change results. The price/quantity notional is integer-exact; the quadratic multiplier calculation uses long double with a tiny numerical tolerance before ceiling. This is an estimate, not an exchange statement reconciliation engine.

## Probability scenarios

For requested contracts C with full cost K, independently estimated YES probability q gives YES expected profit Cq−K; NO uses C(1−q)−K. Apply the same formula to clipped q±uncertainty. If the selected contract wins, profit is C−K; otherwise loss is K. Assumes ordinary $1/zero binary settlement and holding to settlement, with no financing/tax/exit costs. The dashboard's initial 50% is illustrative, never an inferred forecast. Configured estimates require a source/justification.

## Conditional bounds

For N mutually exclusive markets, at most one can settle YES. Buying one NO contract in each therefore pays at least N−1 under ordinary binary settlement. A subset preserves this bound; it does not make the subset exhaustive. This project never invents a YES-basket guarantee from mutual exclusivity alone.

For a manually reviewed implication A ⇒ B, NO(A)+YES(B) pays at least $1 under the same binary settlement assumptions. Different dates, thresholds, observation sources, exceptional resolutions or rule changes can invalidate a human assertion. Relationships default to unreviewed and are ignored until explicitly marked reviewed with a rationale.

A positive net floor is only a conditional consistency candidate. Unknown fees, stale/invalid legs, insufficient depth or excessive capture skew suppress it. Legs cannot be executed atomically here. Settlement exceptions may invalidate the mathematical assumptions. There is no assured profit claim.

## Time and capture coverage

Received timestamps are LOCAL receipt times, not matching-engine update times. Local clocks must be correct. Default age tolerance is 60 seconds; maximum cross-leg skew 10 seconds. Requests are sequential, so large scans can age while still running. Staleness is assessed at analysis time; the dashboard is a frozen capture, never a current executable quote. Time-series plots connect sampled midpoints and miss all intervening events.

Discovery scans a bounded number of pages and selects the highest 24-hour-volume markets only WITHIN those pages. This is not a global liquidity ranking. Coverage/truncation information accompanies the report. A book, metadata or HTTP failure is retained rather than replaced with synthetic data.

## Forecast validation

Brier = mean((q−outcome)²); smaller is better. Log loss is the negative mean log assigned probability of realized outcomes, with exact 0/1 clipped to 1e−15. Ten probability bins summarize mean forecasts and observed frequency. Wilson 95% intervals assume independent Bernoulli outcomes; related contracts violate that assumption. This version trusts the input forecast timestamp provenance: it cannot prove predictions preceded outcomes. Small samples are not evidence of skill.

## Primary specifications checked for this version

- https://docs.kalshi.com/getting_started/quick_start_market_data
- https://docs.kalshi.com/getting_started/orderbook_responses
- https://docs.kalshi.com/getting_started/fee_rounding
- https://docs.kalshi.com/api-reference/market/get-markets
- https://docs.kalshi.com/api-reference/events/get-event
- https://docs.kalshi.com/api-reference/series/get-series
- https://kalshi.com/docs/kalshi-fee-schedule.pdf

API schemas and fee schedules can change; keep these checks current before relying on new observations.
