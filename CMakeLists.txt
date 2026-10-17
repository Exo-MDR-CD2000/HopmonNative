cmake_minimum_required(VERSION 3.20)
# The project version is the single source of the program's version number: it is written into
# the exe's version info (res/hopmon_native.rc), the log, and the release zip's name.
project(HopmonNative VERSION 1.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(HOPMON_FETCH_SDL "Download and build SDL3 if it is not installed" ON)

find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  if(HOPMON_FETCH_SDL)
    message(STATUS "SDL3 not found - fetching release-3.4.18 from GitHub")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.18
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
  else()
    message(FATAL_ERROR "SDL3 not found. Install it or enable HOPMON_FETCH_SDL.")
  endif()
endif()

file(GLOB_RECURSE HOPMON_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/*.cpp)
# miniz (zip reading for zipped mods) is C
list(APPEND HOPMON_SOURCES ${CMAKE_SOURCE_DIR}/third_party/miniz.c)

if(WIN32)
  add_executable(HopmonNative WIN32 ${HOPMON_SOURCES} res/hopmon_native.rc)
else()
  add_executable(HopmonNative ${HOPMON_SOURCES})
endif()

configure_file(res/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)
# the resource compiler doesn't track header changes: rebuild the version info when it changes
set_source_files_properties(res/hopmon_native.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/generated/version.h)
target_include_directories(HopmonNative PRIVATE src third_party ${CMAKE_BINARY_DIR}/generated)
if(TARGET SDL3::SDL3)
  target_link_libraries(HopmonNative PRIVATE SDL3::SDL3)
else()
  target_link_libraries(HopmonNative PRIVATE SDL3::SDL3-static)
endif()

if(WIN32)
  target_link_libraries(HopmonNative PRIVATE winmm)
  target_compile_definitions(HopmonNative PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(MSVC)
  target_compile_options(HopmonNative PRIVATE /W3 /utf-8 /fp:precise)
  # The program's entry point is main(), but a Windows GUI app (no console) makes the MSVC linker
  # look for WinMain. Start from main() instead (MinGW does this on its own).
  target_link_options(HopmonNative PRIVATE /ENTRY:mainCRTStartup)
else()
  target_compile_options(HopmonNative PRIVATE -Wall -Wno-unused-parameter -Wno-sign-compare -ffp-contract=off)
  if(NOT WIN32)
    target_link_libraries(HopmonNative PRIVATE m pthread)
  endif()
endif()

if(MINGW)
  # MSYS2 / MinGW-w64: link the GCC runtime (libstdc++, libgcc, winpthread) statically so the exe
  # runs on its own instead of needing DLLs from the MSYS2 folder next to it.
  target_link_options(HopmonNative PRIVATE -static)
  # windres looks for the icon relative to the build folder, so point it at res/ as well
  set_property(SOURCE res/hopmon_native.rc APPEND PROPERTY INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/res)
endif()

# Copy SDL3.dll next to the executable when SDL is linked dynamically
if(WIN32 AND TARGET SDL3::SDL3-shared)
  add_custom_command(TARGET HopmonNative POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:HopmonNative>)
endif()

# ---------------------------------------------------------------------------------------------
# Release zip: `cmake --build build --config Release --target package` (or make_release.bat)
# creates HopmonNative-<version>-windows-x64.zip with the program and its documents.
install(TARGETS HopmonNative RUNTIME DESTINATION .)
install(FILES README.md CONFIG.md MODDING.md CHANGELOG.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION .)
# the example mods ship as examples (copy them into mods/ to try them), and empty mods/levelpacks/ and mods/addons/ folders
install(DIRECTORY examples/mods/ DESTINATION examples/mods)
install(DIRECTORY DESTINATION mods/levelpacks)
install(DIRECTORY DESTINATION mods/addons)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME HopmonNative)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
if(WIN32)
  set(CPACK_SYSTEM_NAME windows-x64)
endif()
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
