@echo off
rem Builds HopmonNative.exe with Visual Studio 2022 (run from a Developer Command Prompt or any shell with cmake on PATH).
cd /d "%~dp0"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release || goto :error
echo.
echo Built build\Release\HopmonNative.exe - copy it next to your Hopmon.exe.
pause
exit /b 0
:error
echo Build failed.
pause
exit /b 1
