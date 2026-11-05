# New monitor

For automatic anomaly detection and the updating dashboard, open **START_MONITOR.md** first. The instructions below describe the original static reports.

# Start here — KalshiLens 1.0

A separate C++ project for researching Kalshi's public prices and order books.
No Python, API key, paid library, or account login is required. It cannot place orders.

## Easiest Windows setup

1. Extract the entire ZIP (right-click → Extract All). Do not run inside the ZIP.
2. In Visual Studio Installer, ensure **Desktop development with C++** is installed, including the MSVC compiler and a Windows SDK.
3. Double-click **build_windows.cmd** in this folder. It locates Visual Studio, builds the app and runs 49 calculation/API tests. First compilation may take a minute.
4. When it says BUILD AND TESTS PASSED, close that window. Double-click **run_demo.cmd**. Your browser opens the dashboard with clearly labelled invented markets.
5. Double-click **run_live.cmd** for a small live public-market scan. An internet connection and access to Kalshi's public API are required. The report path appears in the console.

Every run creates a new directory under `runs`. The report is `report.html`. A saved report is a snapshot; refreshing the browser does not fetch new prices. Run another scan to collect new data. The included `examples/demo/report.html` can also be opened immediately without compiling.

## If you prefer Solution Explorer

Create a NEW **Console App (C++)** project named KalshiLens. Keep your ETF backtester separate.

1. In **Solution Explorer → Source Files**, remove the generated `.cpp` containing the default `main` from this new project.
2. Right-click **Source Files → Add → Existing Item**, select **KalshiLens.cpp** from this download.
3. Right-click the project → Properties. Select **All Configurations** and **All Platforms**. Under C/C++ → Language set **C++ Language Standard → ISO C++17** (or later). Under C/C++ → Precompiled Headers choose **Not Using Precompiled Headers**.
4. Select **Release**, **x64**. Build → Build Solution. Fix any build error before running; do not run an older build when prompted.
5. Press Ctrl+F5. Choose 2 for the demo or 1 for live data. The generated report opens in your browser.

Compile EITHER the single `KalshiLens.cpp` OR `src/main.cpp`, never both. Both contain the program entry point. Do not add `tests/tests.cpp` to the app project: it has its own test entry point.

The Windows libraries are linked by source directives. The single-file version includes the JSON dependency and dashboard. You do not paste code into Visual Studio Installer, Property Manager or Git Changes.

## PowerShell commands after building

Open PowerShell in this extracted folder (right-click empty space → Open in Terminal).

```powershell
.\bin\kalshilens.exe demo --open
.\bin\kalshilens.exe scan --series KXHIGHNY --max-markets 12 --open
.\bin\kalshilens.exe watch --series KXHIGHNY --max-markets 6 --samples 5 --interval 30 --open
.\bin\kalshilens.exe help
```

The scripts are `.cmd`, not PowerShell scripts. A blocked PowerShell profile does not require changing the system execution policy to use these commands.

## Troubleshooting

- **Unresolved external symbol main:** confirm the intended `.cpp` is in Source Files, Item Type is C/C++ Compiler, and Excluded From Build is No. Use a Console App project.
- **main already defined:** remove the generated default `.cpp` or test entry point from the app project.
- **Cannot find cl.exe:** install the C++ workload, then rerun build_windows.cmd.
- **Cannot open output file:** close the running app before rebuilding.
- **HTTPS error / no markets:** read the console and `http_audit.json`. Check connectivity or try another series. Failed requests never switch silently to demo data.
- **Unknown fees:** unsupported or unavailable event/series metadata suppresses fee-adjusted results. This is deliberate.
- **Stale badge:** saved observations have aged. Make a new scan; editing timestamps is not a valid refresh.

See README.md for the research workflow and docs/METHODOLOGY.md for assumptions.
