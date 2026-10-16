#include "log.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <string>
#endif

static FILE* g_log = nullptr;

FILE* OpenFileUtf8(const char* path, const char* mode) {
#ifdef _WIN32
    auto wide = [](const char* s) {
        int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
        std::wstring w(n > 0 ? (size_t)n : 1, L'\0');
        if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
        return w;
    };
    return _wfopen(wide(path).c_str(), wide(mode).c_str());
#else
    return fopen(path, mode);
#endif
}

void LogInit(const char* path) {
    if (path) g_log = OpenFileUtf8(path, "w");
}

void Log(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s\n", buf);
    if (g_log) {
        fprintf(g_log, "%s\n", buf);
        fflush(g_log);
    }
}

void FatalError(const char* title, const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Log("FATAL: %s", buf);
    if (!SDL_getenv("HOPMON_HEADLESS")) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, buf, nullptr);
    exit(1);
}
