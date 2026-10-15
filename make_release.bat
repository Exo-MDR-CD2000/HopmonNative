@echo off
rem Builds HopmonNative.exe (Release) and packages HopmonNative-<version>-windows-x64.zip
rem with the program, README, CONFIG, CHANGELOG and license files. The version comes from
rem CMakeLists.txt. Run from a Developer Command Prompt for VS 2022 or any shell with cmake.
cd /d "%~dp0"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release || goto :error
cmake --build build --config Release --target package || goto :error
echo.
echo Release zip created in the build folder:
dir /b build\HopmonNative-*.zip
pause
exit /b 0
:error
echo Release build failed.
pause
exit /b 1
