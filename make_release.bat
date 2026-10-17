@echo off
rem Builds HopmonNative.exe (Release) with Visual Studio and packages
rem build\HopmonNative-<version>-windows-x64.zip with the program, README, CONFIG, CHANGELOG and
rem license files. The version comes from CMakeLists.txt. See build_windows.bat for the details.
call "%~dp0build_windows.bat" release
