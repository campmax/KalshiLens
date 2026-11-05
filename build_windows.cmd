@echo off
setlocal
cd /d "%~dp0"
where cl.exe >nul 2>nul
if not errorlevel 1 goto compiler_ready
set "KL_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%KL_VSWHERE%" goto missing_compiler
for /f "usebackq tokens=*" %%i in (`"%KL_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "KL_VSROOT=%%i"
if not defined KL_VSROOT goto missing_compiler
call "%KL_VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto failed
:compiler_ready
if not exist bin mkdir bin
if not exist build mkdir build
echo Building KalshiLens...
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /bigobj /DNOMINMAX src\main.cpp /Fo"build\main.obj" /Fe"bin\kalshilens.exe" /link winhttp.lib shell32.lib
if errorlevel 1 goto failed
echo Building tests...
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /bigobj /DNOMINMAX tests\tests.cpp /Fo"build\tests.obj" /Fe"bin\kalshilens_tests.exe" /link winhttp.lib
if errorlevel 1 goto failed
bin\kalshilens_tests.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /bigobj /DNOMINMAX tests\monitor_tests.cpp /Fo"build\monitor_tests.obj" /Fe"bin\monitor_tests.exe" /link winhttp.lib
if errorlevel 1 goto failed
bin\monitor_tests.exe
if errorlevel 1 goto failed
echo.
echo BUILD AND TESTS PASSED. Next: run_monitor_demo.cmd or run_monitor.cmd.
pause
exit /b 0
:missing_compiler
echo C++ compiler not found. Install Desktop development with C++ in Visual Studio Installer.
echo Then rerun this file, or use the x64 Native Tools Command Prompt.
:failed
echo BUILD FAILED. Read the first error above. Do not run an old executable.
pause
exit /b 1
