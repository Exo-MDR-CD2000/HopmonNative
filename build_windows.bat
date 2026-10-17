@echo off
rem Builds HopmonNative.exe with Visual Studio 2022, 2026 or newer (Community, Professional,
rem Enterprise or Build Tools). Double-click it or run it from any command prompt: it finds
rem Visual Studio by itself, so it doesn't need a Developer Command Prompt.
rem   build_windows.bat           builds build\HopmonNative.exe
rem   build_windows.bat release   also creates build\HopmonNative-<version>-windows-x64.zip
setlocal
cd /d "%~dp0"
set "RESULT=0"

rem Set up the Visual Studio compiler unless this already is a Developer Command Prompt.
where cl >nul 2>nul && goto :have_vs
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vs
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :no_vs
echo Using Visual Studio in %VSDIR%
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul || goto :no_vs
:have_vs
rem Visual Studio's own CMake and Ninja come with the "C++ CMake tools for Windows" component.
where cmake >nul 2>nul || goto :no_cmake
where ninja >nul 2>nul || goto :no_cmake

rem Older versions of this script made a Visual Studio solution in build\. CMake can't change
rem the generator of an existing build folder, so that folder is started over.
if exist build\CMakeCache.txt (
  findstr /c:"CMAKE_GENERATOR:INTERNAL=Ninja" build\CMakeCache.txt >nul || rmdir /s /q build
)

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || goto :error
cmake --build build || goto :error
if /i "%~1"=="release" (
  cmake --build build --target package || goto :error
  echo.
  echo Release zip created in the build folder:
  dir /b build\HopmonNative-*.zip
)
echo.
echo Built build\HopmonNative.exe - copy it next to your Hopmon.exe.
goto :done

:no_vs
echo Could not find Visual Studio with the C++ compiler.
echo In the Visual Studio Installer, install the "Desktop development with C++" workload.
goto :error
:no_cmake
echo CMake or Ninja was not found.
echo In the Visual Studio Installer, add the "C++ CMake tools for Windows" component.
:error
echo Build failed.
set "RESULT=1"
:done
pause
exit /b %RESULT%
