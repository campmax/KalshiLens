@echo off
setlocal
cd /d "%~dp0"
if not exist bin\kalshilens.exe (
 echo Run build_windows.cmd first.
 pause
 exit /b 1
)
py --version >nul 2>nul
if errorlevel 1 (
 echo Python launcher not found. Install Python, then reopen this window.
 pause
 exit /b 1
)
py tools\live_dashboard.py
pause
