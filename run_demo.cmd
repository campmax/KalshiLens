@echo off
setlocal
cd /d "%~dp0"
if not exist bin\kalshilens.exe (
 echo Please run build_windows.cmd first.
 pause
 exit /b 1
)
bin\kalshilens.exe demo --open
set "KL_RESULT=%ERRORLEVEL%"
if not "%KL_RESULT%"=="0" echo Run failed. Read the error above; no synthetic data was substituted.
pause
exit /b %KL_RESULT%
