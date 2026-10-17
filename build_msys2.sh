#!/usr/bin/env bash
# Builds HopmonNative.exe with MSYS2's GCC - no Visual Studio needed.
#
# Run it from the "MSYS2 UCRT64" shell. One-time setup in that shell:
#   pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
#
# Usage:
#   ./build_msys2.sh           builds build-msys2/HopmonNative.exe
#   ./build_msys2.sh release   also creates build-msys2/HopmonNative-<version>-windows-x64.zip
set -euo pipefail
cd "$(dirname "$0")"

case "${MSYSTEM:-}" in
  UCRT64|MINGW64|CLANG64) ;;
  *)
    echo "Run this from the MSYS2 UCRT64 shell (current environment: ${MSYSTEM:-not MSYS2})."
    exit 1 ;;
esac

# SDL3 is always fetched and linked statically (an MSYS2-installed SDL3 would add an SDL3.dll
# dependency). A separate build folder keeps this apart from a Visual Studio build in build/.
cmake -S . -B build-msys2 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
cmake --build build-msys2

if [[ "${1:-}" == "release" ]]; then
  cmake --build build-msys2 --target package
  echo
  echo "Release zip created:"
  ls build-msys2/HopmonNative-*.zip
fi

echo
echo "Built build-msys2/HopmonNative.exe - copy it next to your Hopmon.exe."
