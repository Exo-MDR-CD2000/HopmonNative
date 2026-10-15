#pragma once
#include <cstdarg>

void LogInit(const char* path);
void Log(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
void FatalError(const char* title, const char* fmt, ...);
