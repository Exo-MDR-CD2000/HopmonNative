#pragma once
#include <cstdarg>
#include <cstdio>

// fopen for a UTF-8 path. On Windows plain fopen reads paths in the system code page, so folders
// with characters outside it (an accented user name, say) can't be opened; this can.
FILE* OpenFileUtf8(const char* path, const char* mode);

void LogInit(const char* path);
void Log(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
void FatalError(const char* title, const char* fmt, ...);
